#include <M5Unified.h>
#include <SD.h>
#include <SPI.h>

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
constexpr uint8_t kShared3v3Mask = 1U << 3;  // M5IOE1_G12
constexpr uint32_t kM5Ioe1Frequency = 100000;

constexpr char kTestPath[] = "/corep4x_tf_test.txt";
constexpr char kTestPayload[] = "M5Stack CoreP4X TF card read/write test\n";

SPIClass s_tf_spi(FSPI);

const char* cardTypeName(uint8_t type) {
  switch (type) {
    case CARD_MMC:
      return "MMC";
    case CARD_SD:
      return "SDSC";
    case CARD_SDHC:
      return "SDHC/SDXC";
    default:
      return "UNKNOWN";
  }
}

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
  M5.delay(20);

  const uint8_t output = M5.In_I2C.readRegister8(
      kM5Ioe1Address, kM5Ioe1GpioOutputHighRegister, kM5Ioe1Frequency);
  return ok && (output & kShared3v3Mask);
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

  pinMode(kTfCsPin, OUTPUT);
  digitalWrite(kTfCsPin, HIGH);
  s_tf_spi.begin(kTfClockPin, kTfMisoPin, kTfMosiPin, kTfCsPin);
  if (!SD.begin(kTfCsPin, s_tf_spi, kTfFrequency)) {
    Serial.println("[TF] mount=FAIL");
    drawStatus("FAIL", TFT_RED, "TF card mount failed", "Check card insertion");
    s_tf_spi.end();
    return false;
  }

  const uint8_t card_type = SD.cardType();
  if (card_type == CARD_NONE) {
    Serial.println("[TF] card=FAIL type=NONE");
    drawStatus("FAIL", TFT_RED, "No TF card detected", "Mount returned no card");
    SD.end();
    s_tf_spi.end();
    return false;
  }

  const uint64_t card_size_mib = SD.cardSize() / (1024ULL * 1024ULL);
  const uint64_t total_mib = SD.totalBytes() / (1024ULL * 1024ULL);
  const uint64_t used_mib = SD.usedBytes() / (1024ULL * 1024ULL);
  Serial.printf("[TF] mount=PASS type=%s card=%lluMiB total=%lluMiB used=%lluMiB\n",
                cardTypeName(card_type), card_size_mib, total_mib, used_mib);

  SD.remove(kTestPath);
  File file = SD.open(kTestPath, FILE_WRITE);
  if (!file) {
    Serial.printf("[TF] write=FAIL path=%s open failed\n", kTestPath);
    drawStatus("FAIL", TFT_RED, "Open for write failed", kTestPath);
    SD.end();
    s_tf_spi.end();
    return false;
  }

  const size_t expected_length = std::strlen(kTestPayload);
  const size_t written = file.write(
      reinterpret_cast<const uint8_t*>(kTestPayload), expected_length);
  file.close();
  const bool write_ok = written == expected_length;
  Serial.printf("[TF] write=%s path=%s bytes=%u/%u\n",
                write_ok ? "PASS" : "FAIL", kTestPath,
                static_cast<unsigned>(written),
                static_cast<unsigned>(expected_length));

  char buffer[sizeof(kTestPayload)] = {};
  file = SD.open(kTestPath, FILE_READ);
  const size_t read = file ? file.read(reinterpret_cast<uint8_t*>(buffer),
                                       sizeof(buffer) - 1)
                           : 0;
  if (file) {
    file.close();
  }
  const bool read_ok = read == expected_length;
  const bool content_ok = read_ok && std::memcmp(buffer, kTestPayload,
                                                 expected_length) == 0;
  Serial.printf("[TF] read=%s bytes=%u/%u content=%s\n",
                read_ok ? "PASS" : "FAIL", static_cast<unsigned>(read),
                static_cast<unsigned>(expected_length),
                content_ok ? "PASS" : "FAIL");

  const bool cleanup_ok = SD.remove(kTestPath);
  Serial.printf("[TF] cleanup=%s path=%s\n",
                cleanup_ok ? "PASS" : "FAIL", kTestPath);

  const bool result = write_ok && content_ok && cleanup_ok;
  char card_info[64];
  std::snprintf(card_info, sizeof(card_info), "%s / %llu MiB",
                cardTypeName(card_type), card_size_mib);
  drawStatus(result ? "PASS" : "FAIL", result ? TFT_GREEN : TFT_RED,
             result ? "Write and read verified" : "Data verification failed",
             card_info);
  Serial.printf("[TF] RESULT=%s\n", result ? "PASS" : "FAIL");
  SD.end();
  s_tf_spi.end();
  return result;
}

}  // namespace

void setup() {
  Serial.begin(115200);
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
  runTfCardTest();
}

void loop() {
  M5.update();
  M5.delay(20);
}
