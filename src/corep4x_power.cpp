#include <M5Unified.h>
#include <cstdio>

namespace {
bool s_i2c_devices[120] = {};
uint32_t s_last_scan_ms = 0;

const char* chargingName(m5::Power_Class::is_charging_t charging) {
  switch (charging) {
    case m5::Power_Class::is_charging: return "charging";
    case m5::Power_Class::is_discharging: return "discharging";
    default: return "unknown";
  }
}

uint8_t scanInternalI2c() {
  M5.In_I2C.scanID(s_i2c_devices);
  uint8_t count = 0;
  for (bool present : s_i2c_devices) count += present;
  return count;
}

void drawScreen(uint8_t device_count) {
  const int cx = M5.Display.width() / 2;
  const int cy = M5.Display.height() / 2;
  char line[56];
  const auto charging = M5.Power.isCharging();
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setFont(&fonts::FreeMonoBold9pt7b);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.drawString("CoreP4X power probe", cx, cy - 76);
  std::snprintf(line, sizeof(line), "power type: %d", static_cast<int>(M5.Power.getType()));
  M5.Display.drawString(line, cx, cy - 42);
  std::snprintf(line, sizeof(line), "battery: %d mV  %d%%", static_cast<int>(M5.Power.getBatteryVoltage()), static_cast<int>(M5.Power.getBatteryLevel()));
  M5.Display.drawString(line, cx, cy - 8);
  std::snprintf(line, sizeof(line), "charging: %s", chargingName(charging));
  M5.Display.drawString(line, cx, cy + 26);
  std::snprintf(line, sizeof(line), "internal I2C: %u device(s)", device_count);
  M5.Display.drawString(line, cx, cy + 60);
  M5.Display.setFont(&fonts::Font0);
  M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
  M5.Display.drawString("Tap to rescan", cx, M5.Display.height() - 20);
}

void scanAndDraw() {
  const uint8_t count = scanInternalI2c();
  Serial.printf("power: type=%d battery=%dmV level=%ld charging=%s I2C:", static_cast<int>(M5.Power.getType()), static_cast<int>(M5.Power.getBatteryVoltage()), static_cast<long>(M5.Power.getBatteryLevel()), chargingName(M5.Power.isCharging()));
  for (uint8_t address = 0; address < 120; ++address) if (s_i2c_devices[address]) Serial.printf(" 0x%02X", address);
  Serial.println();
  drawScreen(count);
}
}

void setup() {
  Serial.begin(115200);
  auto cfg = M5.config();
  cfg.internal_mic = false;
  cfg.internal_spk = false;
  M5.begin(cfg);
  M5.Display.setRotation(0);
  scanAndDraw();
}

void loop() {
  M5.update();
  const bool tapped = M5.Touch.getCount() && M5.Touch.getDetail(0).wasClicked();
  const uint32_t now = millis();
  if (tapped || now - s_last_scan_ms >= 3000) { s_last_scan_ms = now; scanAndDraw(); }
}
