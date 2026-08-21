#include <M5Unified.h>

#include <cstdio>

namespace {

constexpr uint8_t kRtcAddress = 0x32;
constexpr uint8_t kM5Ioe1Address = 0x4F;
constexpr uint8_t kM5Pm1Address = 0x6E;
constexpr uint8_t kM5Pm1PowerConfigRegister = 0x06;
constexpr uint8_t kM5Pm1ChargeEnableMask = 1U << 0;
constexpr uint32_t kRefreshIntervalMs = 1000;
constexpr uint32_t kScanIntervalMs = 5000;
constexpr uint32_t kLogIntervalMs = 3000;

constexpr uint32_t kBackground = 0x101820U;
constexpr uint32_t kPanel = 0x18242EU;
constexpr uint32_t kBorder = 0x355064U;
constexpr uint32_t kText = 0xF1F5F7U;
constexpr uint32_t kMuted = 0xA9BAC4U;
constexpr uint32_t kGood = 0x45D483U;
constexpr uint32_t kWarning = 0xFFB84DU;

struct Button {
  int32_t x;
  int32_t y;
  int32_t width;
  int32_t height;

  bool contains(int32_t px, int32_t py) const {
    return px >= x && px < x + width && py >= y && py < y + height;
  }
};

bool s_i2c_devices[120] = {};
uint8_t s_i2c_device_count = 0;
uint32_t s_last_refresh_ms = 0;
uint32_t s_last_scan_ms = 0;
uint32_t s_last_log_ms = 0;
char s_action[48] = "Tap an output to toggle it";

const char* onOff(bool enabled) {
  return enabled ? "ON" : "OFF";
}

const char* chargingName(m5::Power_Class::is_charging_t charging) {
  switch (charging) {
    case m5::Power_Class::is_charging:
      return "charging";
    case m5::Power_Class::is_discharging:
      return "discharging";
    default:
      return "unknown";
  }
}

const char* powerSourceName(m5::M5PM1_Class::pwr_src_t source) {
  switch (source) {
    case m5::M5PM1_Class::none:
      return "none";
    case m5::M5PM1_Class::vin:
      return "VIN";
    case m5::M5PM1_Class::vinout:
      return "VIN output";
    case m5::M5PM1_Class::battery:
      return "battery";
    case m5::M5PM1_Class::vin | m5::M5PM1_Class::vinout:
      return "VIN + output";
    case m5::M5PM1_Class::vin | m5::M5PM1_Class::battery:
      return "VIN + battery";
    case m5::M5PM1_Class::vinout | m5::M5PM1_Class::battery:
      return "output + battery";
    case m5::M5PM1_Class::vin | m5::M5PM1_Class::vinout |
        m5::M5PM1_Class::battery:
      return "VIN + output + battery";
    default:
      return "unknown";
  }
}

void scanInternalI2c() {
  M5.In_I2C.scanID(s_i2c_devices);
  s_i2c_device_count = 0;
  for (bool present : s_i2c_devices) {
    s_i2c_device_count += present;
  }
  s_last_scan_ms = millis();

  Serial.printf(
      "[I2C] count=%u RX8130@0x32=%s M5IOE1@0x4F=%s M5PM1@0x6E=%s devices:",
      s_i2c_device_count, onOff(s_i2c_devices[kRtcAddress]),
      onOff(s_i2c_devices[kM5Ioe1Address]), onOff(s_i2c_devices[kM5Pm1Address]));
  for (uint8_t address = 0; address < 120; ++address) {
    if (s_i2c_devices[address]) {
      Serial.printf(" 0x%02X", address);
    }
  }
  Serial.println();
}

Button buttonAt(uint8_t index) {
  constexpr int32_t margin = 28;
  constexpr int32_t gap = 16;
  const int32_t width = (M5.Display.width() - margin * 2 - gap * 2) / 3;
  return {margin + index * (width + gap), M5.Display.height() - 104, width, 68};
}

void drawButton(const Button& button, const char* label, bool active) {
  const uint32_t fill = active ? kGood : kPanel;
  const uint32_t foreground = active ? TFT_BLACK : kText;
  M5.Display.fillRoundRect(button.x, button.y, button.width, button.height, 6, fill);
  M5.Display.drawRoundRect(button.x, button.y, button.width, button.height, 6, kBorder);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(foreground, fill);
  M5.Display.drawString(label, button.x + button.width / 2,
                        button.y + button.height / 2);
}

void drawMetricLabel(int32_t x, int32_t y, const char* label) {
  M5.Display.setFont(&fonts::Font0);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextColor(kMuted, kPanel);
  M5.Display.drawString(label, x, y);
}

void drawMetricValue(int32_t x, int32_t y, int32_t width, const char* value,
                     uint32_t value_color = kText) {
  M5.Display.setFont(&fonts::Font2);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextColor(value_color, kPanel);
  M5.Display.setTextPadding(width);
  M5.Display.drawString(value, x, y);
  M5.Display.setTextPadding(0);
}

bool chargeEnabled() {
  return M5.Power.M5pm1.readRegister8(kM5Pm1PowerConfigRegister)
       & kM5Pm1ChargeEnableMask;
}

bool rtcString(char* buffer, size_t size) {
  m5::rtc_datetime_t datetime;
  if (!M5.Rtc.isEnabled() || !M5.Rtc.getDateTime(&datetime)) {
    std::snprintf(buffer, size, "not detected");
    return false;
  }
  std::snprintf(buffer, size, "%04d-%02d-%02d %02d:%02d:%02d", datetime.date.year,
                datetime.date.month, datetime.date.date, datetime.time.hours,
                datetime.time.minutes, datetime.time.seconds);
  return true;
}

void drawAction() {
  M5.Display.setFont(&fonts::Font2);
  M5.Display.setTextDatum(top_center);
  M5.Display.setTextColor(kMuted, kBackground);
  M5.Display.setTextPadding(M5.Display.width() - 36);
  M5.Display.drawString(s_action, M5.Display.width() / 2, 350);
  M5.Display.setTextPadding(0);
}

void drawOutputButtons() {
  const bool port_a_enabled = M5.Power.getExtOutput();
  const bool usb_enabled = M5.Power.getUsbOutput();
  const bool charge_enabled = chargeEnabled();
  M5.Display.setFont(&fonts::Font2);
  drawButton(buttonAt(0), port_a_enabled ? "PORT A: ON" : "PORT A: OFF", port_a_enabled);
  drawButton(buttonAt(1), usb_enabled ? "USB HOST: ON" : "USB HOST: OFF", usb_enabled);
  drawButton(buttonAt(2), charge_enabled ? "CHARGE: ON" : "CHARGE: OFF", charge_enabled);
}

void drawStaticScreen() {
  constexpr int32_t panel_x = 18;
  constexpr int32_t panel_y = 54;
  constexpr int32_t panel_width = 444;
  constexpr int32_t panel_height = 286;
  constexpr int32_t column_width = 200;
  constexpr int32_t left_x = 34;
  constexpr int32_t right_x = 246;
  constexpr int32_t first_row_y = 66;
  constexpr int32_t row_height = 43;
  static constexpr const char* left_labels[] = {
      "BOARD", "PMIC", "PM1 SOURCE", "VBUS", "BATTERY", "CHARGING"};
  static constexpr const char* right_labels[] = {
      "PM1 5VOUT", "PORT A 5V", "USB HOST 5V", "CHARGE ENABLE", "RTC", "INTERNAL I2C"};

  M5.Display.startWrite();
  M5.Display.fillScreen(kBackground);
  M5.Display.setFont(&fonts::FreeMonoBold9pt7b);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextColor(kText, kBackground);
  M5.Display.drawString("CoreP4X power", panel_x, 18);
  M5.Display.setFont(&fonts::Font2);
  M5.Display.setTextDatum(top_right);
  M5.Display.setTextColor(kMuted, kBackground);
  M5.Display.drawString("M5Unified validation", M5.Display.width() - panel_x, 20);

  M5.Display.fillRoundRect(panel_x, panel_y, panel_width, panel_height, 6, kPanel);
  M5.Display.drawRoundRect(panel_x, panel_y, panel_width, panel_height, 6, kBorder);
  for (uint8_t row = 0; row < 6; ++row) {
    const int32_t y = first_row_y + row * row_height;
    drawMetricLabel(left_x, y, left_labels[row]);
    drawMetricLabel(right_x, y, right_labels[row]);
  }
  drawAction();
  drawOutputButtons();
  M5.Display.endWrite();
}

void drawPowerValues() {
  constexpr int32_t column_width = 200;
  constexpr int32_t left_x = 34;
  constexpr int32_t right_x = 246;
  constexpr int32_t first_value_y = 78;
  constexpr int32_t row_height = 43;

  const bool port_a_enabled = M5.Power.getExtOutput();
  const bool usb_enabled = M5.Power.getUsbOutput();
  const bool charge_enabled = chargeEnabled();
  const auto charging = M5.Power.isCharging();
  const auto source = M5.Power.M5pm1.getPowerSource();
  const int16_t vbus_mv = M5.Power.getVBUSVoltage();
  const int16_t battery_mv = M5.Power.getBatteryVoltage();
  const int32_t battery_level = M5.Power.getBatteryLevel();
  const uint16_t output_5v_mv = M5.Power.M5pm1.get5VoutVoltage();

  char board[32];
  char battery[32];
  char vbus[24];
  char output_5v[24];
  char rtc[32];
  char i2c[32];
  std::snprintf(board, sizeof(board), "CoreP4X (%d)", static_cast<int>(M5.getBoard()));
  std::snprintf(battery, sizeof(battery), "%d mV / %ld%%", battery_mv,
                static_cast<long>(battery_level));
  std::snprintf(vbus, sizeof(vbus), "%d mV", vbus_mv);
  std::snprintf(output_5v, sizeof(output_5v), "%u mV", output_5v_mv);
  const bool rtc_ok = rtcString(rtc, sizeof(rtc));
  const bool i2c_ok = s_i2c_devices[kRtcAddress] && s_i2c_devices[kM5Ioe1Address]
                   && s_i2c_devices[kM5Pm1Address];
  std::snprintf(i2c, sizeof(i2c), "%u found / %s", s_i2c_device_count,
                i2c_ok ? "PASS" : "CHECK");

  M5.Display.startWrite();
  drawMetricValue(left_x, first_value_y + row_height * 0, column_width, board);
  drawMetricValue(left_x, first_value_y + row_height * 1, column_width, "M5PM1");
  drawMetricValue(left_x, first_value_y + row_height * 2, column_width,
                  powerSourceName(source), source == m5::M5PM1_Class::none ? kWarning : kText);
  drawMetricValue(left_x, first_value_y + row_height * 3, column_width, vbus,
                  vbus_mv > 0 ? kGood : kWarning);
  drawMetricValue(left_x, first_value_y + row_height * 4, column_width, battery,
                  battery_mv <= 0 ? kWarning
                                  : (charging == m5::Power_Class::is_charging ? kGood : kText));
  drawMetricValue(left_x, first_value_y + row_height * 5, column_width,
                  chargingName(charging),
                  charging == m5::Power_Class::charge_unknown ? kWarning : kText);

  drawMetricValue(right_x, first_value_y + row_height * 0, column_width, output_5v,
                  output_5v_mv > 4000 ? kGood : kWarning);
  drawMetricValue(right_x, first_value_y + row_height * 1, column_width,
                  onOff(port_a_enabled), port_a_enabled ? kGood : kText);
  drawMetricValue(right_x, first_value_y + row_height * 2, column_width,
                  onOff(usb_enabled), usb_enabled ? kGood : kText);
  drawMetricValue(right_x, first_value_y + row_height * 3, column_width,
                  onOff(charge_enabled), charge_enabled ? kGood : kWarning);
  drawMetricValue(right_x, first_value_y + row_height * 4, column_width, rtc,
                  rtc_ok ? kGood : kWarning);
  drawMetricValue(right_x, first_value_y + row_height * 5, column_width, i2c,
                  i2c_ok ? kGood : kWarning);
  M5.Display.endWrite();
}

void logPowerState() {
  char rtc[32];
  const bool rtc_ok = rtcString(rtc, sizeof(rtc));
  Serial.printf(
      "[POWER] board=M5Stack_CoreP4X id=%d pmic=%d source=%s vbus=%dmV "
      "battery=%dmV level=%ld%% charging=%s 5vout=%umV portA=%s usbHost=%s "
      "chargeEnabled=%s rtc=%s(%s)\n",
      static_cast<int>(M5.getBoard()), static_cast<int>(M5.Power.getType()),
      powerSourceName(M5.Power.M5pm1.getPowerSource()), M5.Power.getVBUSVoltage(),
      M5.Power.getBatteryVoltage(), static_cast<long>(M5.Power.getBatteryLevel()),
      chargingName(M5.Power.isCharging()), M5.Power.M5pm1.get5VoutVoltage(),
      onOff(M5.Power.getExtOutput()), onOff(M5.Power.getUsbOutput()),
      onOff(chargeEnabled()), rtc_ok ? "OK" : "FAIL", rtc);
}

void togglePortA() {
  const bool requested = !M5.Power.getExtOutput();
  M5.Power.setExtOutput(requested, m5::ext_port_mask_t::ext_PA);
  M5.delay(20);
  const bool actual = M5.Power.getExtOutput();
  std::snprintf(s_action, sizeof(s_action), "Port A requested %s: %s", onOff(requested),
                actual == requested ? "PASS" : "FAIL");
  Serial.printf("[ACTION] portA requested=%s actual=%s result=%s\n", onOff(requested),
                onOff(actual), actual == requested ? "PASS" : "FAIL");
}

void toggleUsbHost() {
  const bool requested = !M5.Power.getUsbOutput();
  M5.Power.setUsbOutput(requested);
  M5.delay(20);
  const bool actual = M5.Power.getUsbOutput();
  std::snprintf(s_action, sizeof(s_action), "USB Host requested %s: %s", onOff(requested),
                actual == requested ? "PASS" : "FAIL");
  Serial.printf("[ACTION] usbHost requested=%s actual=%s result=%s\n", onOff(requested),
                onOff(actual), actual == requested ? "PASS" : "FAIL");
}

void toggleCharge() {
  const bool requested = !chargeEnabled();
  M5.Power.setBatteryCharge(requested);
  M5.delay(20);
  const bool actual = chargeEnabled();
  std::snprintf(s_action, sizeof(s_action), "Charge requested %s: %s", onOff(requested),
                actual == requested ? "PASS" : "FAIL");
  Serial.printf("[ACTION] charge requested=%s actual=%s result=%s\n", onOff(requested),
                onOff(actual), actual == requested ? "PASS" : "FAIL");
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

  Serial.printf("[BOARD] name=M5Stack_CoreP4X id=%d expected=%d result=%s\n",
                static_cast<int>(M5.getBoard()),
                static_cast<int>(m5::board_t::board_M5CoreP4X),
                M5.getBoard() == m5::board_t::board_M5CoreP4X ? "PASS" : "FAIL");
  scanInternalI2c();
  logPowerState();
  drawStaticScreen();
  drawPowerValues();
  const uint32_t now = millis();
  s_last_refresh_ms = now;
  s_last_log_ms = now;
}

void loop() {
  M5.update();
  const uint32_t now = millis();

  if (M5.Touch.getCount()) {
    const auto touch = M5.Touch.getDetail(0);
    if (touch.wasClicked()) {
      if (buttonAt(0).contains(touch.x, touch.y)) {
        togglePortA();
      } else if (buttonAt(1).contains(touch.x, touch.y)) {
        toggleUsbHost();
      } else if (buttonAt(2).contains(touch.x, touch.y)) {
        toggleCharge();
      }
      drawAction();
      drawOutputButtons();
      drawPowerValues();
      s_last_refresh_ms = now;
    }
  }

  if (now - s_last_scan_ms >= kScanIntervalMs) {
    scanInternalI2c();
  }
  if (now - s_last_refresh_ms >= kRefreshIntervalMs) {
    drawPowerValues();
    s_last_refresh_ms = now;
  }
  if (now - s_last_log_ms >= kLogIntervalMs) {
    logPowerState();
    s_last_log_ms = now;
  }
  M5.delay(10);
}
