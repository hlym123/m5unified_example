#include <M5Unified.h>

#include <driver/spi_master.h>
#include <esp_err.h>
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

constexpr int kTfMosiPin = 7;
constexpr int kTfMisoPin = 8;
constexpr int kTfClockPin = 10;
constexpr int kTfCsPin = 50;
constexpr uint32_t kTfFrequency = 20000000;

constexpr uint8_t kM5Ioe1Address = 0x4F;
constexpr uint8_t kM5Ioe1I2cConfigRegister = 0x23;
constexpr uint8_t kM5Ioe1GpioModeHighRegister = 0x14;
constexpr uint8_t kM5Ioe1GpioModeLowRegister = 0x04;
constexpr uint8_t kM5Ioe1GpioOutputHighRegister = 0x06;
constexpr uint8_t kM5Ioe1GpioInputHighRegister = 0x08;
constexpr uint8_t kM5Ioe1GpioPullupHighRegister = 0x0A;
constexpr uint8_t kM5Ioe1GpioPulldownHighRegister = 0x0C;
constexpr uint8_t kShared3v3Mask = 1U << 3;  // M5IOE1_G12
constexpr uint8_t kCardDetectMask = 1U << 4;  // M5IOE1_G13, active low
constexpr uint32_t kM5Ioe1Frequency = 100000;

constexpr char kMountPoint[] = "/sdcard";
constexpr char kTestPath[] = "/sdcard/corep4x_tf_test.txt";
constexpr char kTestPayload[] = "M5Stack CoreP4X TF card read/write test\n";

bool s_test_passed = false;
uint32_t s_next_retry_ms = 0;

void drawStatus(const char* result, uint32_t color, const char* detail1,
                const char* detail2) {
  M5.Display.startWrite();
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextDatum(top_center);
  M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
  M5.Display.setFont(&fonts::FreeMonoBold9pt7b);
  M5.Display.drawString("CoreP4X TF Card", M5.Display.width() / 2, 42);

  M5.Display.setTextColor(color, TFT_BLACK);
  M5.Display.setFont(&fonts::Font7);
  M5.Display.drawString(result, M5.Display.width() / 2, 128);

  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setFont(&fonts::Font2);
  M5.Display.drawString(detail1, M5.Display.width() / 2, 250);
  M5.Display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  M5.Display.drawString(detail2, M5.Display.width() / 2, 286);
  M5.Display.endWrite();
}

bool enableShared3v3() {
  bool ok = M5.In_I2C.writeRegister8(kM5Ioe1Address, kM5Ioe1I2cConfigRegister,
                                     0x00, kM5Ioe1Frequency);
  ok = M5.In_I2C.bitOff(kM5Ioe1Address, kM5Ioe1GpioModeHighRegister,
                        kShared3v3Mask, kM5Ioe1Frequency) && ok;
  ok = M5.In_I2C.bitOn(kM5Ioe1Address, kM5Ioe1GpioModeLowRegister,
                       kShared3v3Mask, kM5Ioe1Frequency) && ok;
  ok = M5.In_I2C.bitOn(kM5Ioe1Address, kM5Ioe1GpioOutputHighRegister,
                       kShared3v3Mask, kM5Ioe1Frequency) && ok;
  M5.delay(300);

  const uint8_t output = M5.In_I2C.readRegister8(
      kM5Ioe1Address, kM5Ioe1GpioOutputHighRegister, kM5Ioe1Frequency);
  return ok && (output & kShared3v3Mask);
}

bool cardInserted(bool* config_ok, uint8_t* input_value) {
  bool ok = M5.In_I2C.bitOff(kM5Ioe1Address, kM5Ioe1GpioModeLowRegister,
                             kCardDetectMask, kM5Ioe1Frequency);
  ok = M5.In_I2C.bitOff(kM5Ioe1Address, kM5Ioe1GpioPulldownHighRegister,
                        kCardDetectMask, kM5Ioe1Frequency) && ok;
  ok = M5.In_I2C.bitOn(kM5Ioe1Address, kM5Ioe1GpioPullupHighRegister,
                       kCardDetectMask, kM5Ioe1Frequency) && ok;
  M5.delay(20);

  *input_value = M5.In_I2C.readRegister8(
      kM5Ioe1Address, kM5Ioe1GpioInputHighRegister, kM5Ioe1Frequency);
  *config_ok = ok;
  return (*input_value & kCardDetectMask) == 0;
}

void cleanupSd(sdmmc_card_t* card, bool bus_initialized) {
  if (card != nullptr) {
    esp_vfs_fat_sdcard_unmount(kMountPoint, card);
  }
  if (bus_initialized) {
    spi_bus_free(SPI3_HOST);
  }
}

bool runTfCardTest() {
  Serial.printf("[TF] pins MOSI=G%d MISO=G%d CLK=G%d CS=G%d frequency=%lu\n",
                kTfMosiPin, kTfMisoPin, kTfClockPin, kTfCsPin,
                static_cast<unsigned long>(kTfFrequency));

  const bool power_ok = enableShared3v3();
  Serial.printf("[TF] power M5IOE1_G12=%s\n", power_ok ? "PASS" : "FAIL");
  if (!power_ok) {
    drawStatus("FAIL", TFT_RED, "3V3 rail enable failed", "M5IOE1_G12");
    return false;
  }

  bool detect_config_ok = false;
  uint8_t detect_input = 0xFF;
  const bool card_inserted = cardInserted(&detect_config_ok, &detect_input);
  Serial.printf("[TF] detect M5IOE1_G13=%s raw=0x%02X config=%s\n",
                card_inserted ? "LOW/PRESENT" : "HIGH/NOT_INSERTED",
                detect_input, detect_config_ok ? "PASS" : "FAIL");
  if (!detect_config_ok || !card_inserted) {
    drawStatus("FAIL", TFT_RED,
               detect_config_ok ? "TF card not detected" : "Card detect config failed",
               "M5IOE1_G13 must be LOW");
    return false;
  }

  spi_bus_config_t bus_config = {};
  bus_config.mosi_io_num = kTfMosiPin;
  bus_config.miso_io_num = kTfMisoPin;
  bus_config.sclk_io_num = kTfClockPin;
  bus_config.quadwp_io_num = -1;
  bus_config.quadhd_io_num = -1;
  bus_config.max_transfer_sz = 2048;

  esp_err_t result = spi_bus_initialize(SPI3_HOST, &bus_config, SPI_DMA_CH_AUTO);
  if (result != ESP_OK) {
    Serial.printf("[TF] spi_bus_initialize=FAIL error=%s (0x%X)\n",
                  esp_err_to_name(result), static_cast<unsigned>(result));
    drawStatus("FAIL", TFT_RED, "SPI3 init failed", esp_err_to_name(result));
    return false;
  }

  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  host.slot = SPI3_HOST;
  host.max_freq_khz = kTfFrequency / 1000;

  sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
  slot_config.gpio_cs = static_cast<gpio_num_t>(kTfCsPin);
  slot_config.host_id = SPI3_HOST;

  esp_vfs_fat_sdmmc_mount_config_t mount_config = {};
  mount_config.format_if_mount_failed = false;
  mount_config.max_files = 5;
  mount_config.allocation_unit_size = 64 * 1024;

  sdmmc_card_t* card = nullptr;
  result = esp_vfs_fat_sdspi_mount(kMountPoint, &host, &slot_config,
                                   &mount_config, &card);
  if (result != ESP_OK) {
    Serial.printf("[TF] mount=FAIL error=%s (0x%X)\n",
                  esp_err_to_name(result), static_cast<unsigned>(result));
    drawStatus("FAIL", TFT_RED, "TF card mount failed", esp_err_to_name(result));
    cleanupSd(nullptr, true);
    return false;
  }

  const uint64_t card_size_mib =
      static_cast<uint64_t>(card->csd.capacity) * card->csd.sector_size
      / (1024ULL * 1024ULL);
  Serial.printf("[TF] mount=PASS name=%s card=%lluMiB max_freq=%ukHz\n",
                card->cid.name, card_size_mib, host.max_freq_khz);

  std::remove(kTestPath);
  FILE* file = std::fopen(kTestPath, "wb");
  if (!file) {
    Serial.printf("[TF] write=FAIL path=%s open failed\n", kTestPath);
    drawStatus("FAIL", TFT_RED, "Open for write failed", kTestPath);
    cleanupSd(card, true);
    return false;
  }

  const size_t expected_length = std::strlen(kTestPayload);
  const size_t written = std::fwrite(kTestPayload, 1, expected_length, file);
  std::fclose(file);
  const bool write_ok = written == expected_length;
  Serial.printf("[TF] write=%s path=%s bytes=%u/%u\n",
                write_ok ? "PASS" : "FAIL", kTestPath,
                static_cast<unsigned>(written),
                static_cast<unsigned>(expected_length));

  char buffer[sizeof(kTestPayload)] = {};
  file = std::fopen(kTestPath, "rb");
  const size_t read = file ? std::fread(buffer, 1, sizeof(buffer) - 1, file) : 0;
  if (file) {
    std::fclose(file);
  }
  const bool read_ok = read == expected_length;
  const bool content_ok = read_ok && std::memcmp(buffer, kTestPayload,
                                                 expected_length) == 0;
  Serial.printf("[TF] read=%s bytes=%u/%u content=%s\n",
                read_ok ? "PASS" : "FAIL", static_cast<unsigned>(read),
                static_cast<unsigned>(expected_length),
                content_ok ? "PASS" : "FAIL");

  const bool cleanup_ok = std::remove(kTestPath) == 0;
  Serial.printf("[TF] cleanup=%s path=%s\n",
                cleanup_ok ? "PASS" : "FAIL", kTestPath);

  const bool test_result = write_ok && content_ok && cleanup_ok;
  char card_info[64];
  std::snprintf(card_info, sizeof(card_info), "%s / %llu MiB",
                card->cid.name, card_size_mib);
  drawStatus(test_result ? "PASS" : "FAIL", test_result ? TFT_GREEN : TFT_RED,
             test_result ? "Write and read verified" : "Data verification failed",
             card_info);
  Serial.printf("[TF] RESULT=%s\n", test_result ? "PASS" : "FAIL");
  cleanupSd(card, true);
  return test_result;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);
  auto cfg = M5.config();
  cfg.internal_mic = false;
  cfg.internal_spk = false;
  cfg.serial_baudrate = 115200;
  M5.begin(cfg);
  M5.Display.setRotation(0);
  M5.Display.setBrightness(64);

  const bool board_ok = M5.getBoard() == m5::board_t::board_M5CoreP4X;
  Serial.printf("[BOARD] name=M5Stack_CoreP4X id=%d expected=%d result=%s\n",
                static_cast<int>(M5.getBoard()),
                static_cast<int>(m5::board_t::board_M5CoreP4X),
                board_ok ? "PASS" : "FAIL");
  if (!board_ok) {
    drawStatus("FAIL", TFT_RED, "Board detection failed", "Expected CoreP4X");
    return;
  }
  drawStatus("TEST", TFT_YELLOW, "Testing TF card", "Please wait");
  s_test_passed = runTfCardTest();
  s_next_retry_ms = millis() + 5000;
}

void loop() {
  M5.update();
  const uint32_t now = millis();
  if (!s_test_passed && static_cast<int32_t>(now - s_next_retry_ms) >= 0) {
    Serial.println("[TF] retrying failed test");
    s_test_passed = runTfCardTest();
    s_next_retry_ms = millis() + 5000;
  }
  M5.delay(20);
}
