#include <M5Unified.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

constexpr uint32_t kI2cFrequency = 400000;
constexpr uint32_t kScanIntervalMs = 100;
constexpr uint32_t kRetryIntervalMs = 5000;

bool s_test_passed = false;
uint32_t s_next_retry_ms = 0;

uint8_t scanBus(bool* present) {
  std::memset(present, 0, sizeof(bool) * 120);
  M5.Ex_I2C.scanID(present, kI2cFrequency);
  uint8_t count = 0;
  for (uint8_t address = 0x08; address <= 0x77; ++address) {
    count += present[address];
  }
  return count;
}

uint8_t commonDevices(const bool* first, const bool* second, bool* common) {
  uint8_t count = 0;
  for (uint8_t address = 0x08; address <= 0x77; ++address) {
    common[address] = first[address] && second[address];
    count += common[address];
  }
  return count;
}

uint8_t poweredOffDevices(const bool* candidates, bool* present) {
  uint8_t count = 0;
  for (uint8_t address = 0x08; address <= 0x77; ++address) {
    present[address] = candidates[address]
                    && M5.Ex_I2C.scanID(address, kI2cFrequency);
    count += present[address];
  }
  return count;
}

void logAddresses(const char* stage, const bool* present, uint8_t count) {
  Serial.printf("[GROVE] %s count=%u addresses:", stage, count);
  for (uint8_t address = 0x08; address <= 0x77; ++address) {
    if (present[address]) {
      Serial.printf(" 0x%02X", address);
    }
  }
  Serial.println();
}

void formatAddresses(char* buffer, size_t size, const bool* present) {
  size_t used = 0;
  for (uint8_t address = 0x08; address <= 0x77; ++address) {
    if (!present[address] || used >= size) {
      continue;
    }
    const int written = std::snprintf(buffer + used, size - used,
                                      used == 0 ? "0x%02X" : " 0x%02X", address);
    if (written < 0 || static_cast<size_t>(written) >= size - used) {
      break;
    }
    used += static_cast<size_t>(written);
  }
  if (used == 0) {
    std::snprintf(buffer, size, "none");
  }
}

void drawResult(const char* result, uint32_t color, const char* addresses,
                const char* detail) {
  M5.Display.startWrite();
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextDatum(top_center);
  M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
  M5.Display.setFont(&fonts::FreeMonoBold9pt7b);
  M5.Display.drawString("CoreP4X Grove I2C", M5.Display.width() / 2, 34);

  M5.Display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  M5.Display.setFont(&fonts::Font2);
  M5.Display.drawString("SDA G18 / SCL G16 / 400 kHz",
                        M5.Display.width() / 2, 84);
  M5.Display.drawString("Power: M5IOE1_G5", M5.Display.width() / 2, 112);

  M5.Display.setTextColor(color, TFT_BLACK);
  M5.Display.setFont(&fonts::Font7);
  M5.Display.drawString(result, M5.Display.width() / 2, 168);

  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setFont(&fonts::Font4);
  M5.Display.drawString(addresses, M5.Display.width() / 2, 274);
  M5.Display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  M5.Display.setFont(&fonts::Font2);
  M5.Display.drawString(detail, M5.Display.width() / 2, 326);
  M5.Display.endWrite();
}

bool runGroveTest() {
  Serial.printf("[GROVE] bus port=%d SDA=G%d SCL=G%d frequency=%lu\n",
                static_cast<int>(M5.Ex_I2C.getPort()), M5.Ex_I2C.getSDA(),
                M5.Ex_I2C.getSCL(), static_cast<unsigned long>(kI2cFrequency));

  M5.Power.setExtOutput(true, m5::ext_port_mask_t::ext_PA);
  M5.delay(150);
  const bool power_on = M5.Power.getExtOutput();
  Serial.printf("[GROVE] power M5IOE1_G5 requested=ON actual=%s\n",
                power_on ? "ON" : "OFF");

  bool first[120] = {};
  bool second[120] = {};
  bool stable[120] = {};
  const uint8_t first_count = scanBus(first);
  M5.delay(kScanIntervalMs);
  const uint8_t second_count = scanBus(second);
  const uint8_t stable_count = commonDevices(first, second, stable);
  logAddresses("scan1", first, first_count);
  logAddresses("scan2", second, second_count);
  logAddresses("stable", stable, stable_count);

  M5.Power.setExtOutput(false, m5::ext_port_mask_t::ext_PA);
  M5.delay(120);
  const bool power_off = !M5.Power.getExtOutput();
  bool off_present[120] = {};
  const uint8_t off_count = poweredOffDevices(stable, off_present);
  logAddresses("power_off", off_present, off_count);

  M5.Power.setExtOutput(true, m5::ext_port_mask_t::ext_PA);
  M5.delay(150);
  const bool power_restored = M5.Power.getExtOutput();

  const bool pins_ok = M5.Ex_I2C.getSDA() == 18 && M5.Ex_I2C.getSCL() == 16;
  const bool result = pins_ok && power_on && power_off && power_restored
                   && stable_count > 0 && off_count == 0;
  char addresses[96] = {};
  formatAddresses(addresses, sizeof(addresses), stable);
  const char* detail = !pins_ok ? "External I2C pin mapping failed"
                     : !power_on ? "Grove power enable failed"
                     : stable_count == 0 ? "No stable I2C device found"
                     : off_count != 0 ? "Device still responds powered off"
                     : !power_restored ? "Grove power restore failed"
                     : "Stable scan and power gate verified";
  Serial.printf("[GROVE] pins=%s power_on=%s power_off=%s restore=%s "
                "stable=%u off_present=%u RESULT=%s\n",
                pins_ok ? "PASS" : "FAIL", power_on ? "PASS" : "FAIL",
                power_off ? "PASS" : "FAIL", power_restored ? "PASS" : "FAIL",
                stable_count, off_count, result ? "PASS" : "FAIL");
  drawResult(result ? "PASS" : "FAIL", result ? TFT_GREEN : TFT_RED,
             addresses, detail);
  return result;
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
  const bool bus_ok = M5.Ex_I2C.begin();
  Serial.printf("[BOARD] name=M5Stack_CoreP4X id=%d expected=%d result=%s\n",
                static_cast<int>(M5.getBoard()),
                static_cast<int>(m5::board_t::board_M5CoreP4X),
                board_ok ? "PASS" : "FAIL");
  Serial.printf("[GROVE] external_bus_begin=%s\n", bus_ok ? "PASS" : "FAIL");
  if (!board_ok || !bus_ok) {
    drawResult("FAIL", TFT_RED, "none",
               !board_ok ? "Board detection failed" : "External I2C init failed");
    return;
  }

  s_test_passed = runGroveTest();
  s_next_retry_ms = millis() + kRetryIntervalMs;
}

void loop() {
  M5.update();
  const uint32_t now = millis();
  if (!s_test_passed && static_cast<int32_t>(now - s_next_retry_ms) >= 0) {
    Serial.println("[GROVE] retrying failed test");
    s_test_passed = runGroveTest();
    s_next_retry_ms = millis() + kRetryIntervalMs;
  }
  M5.delay(20);
}
