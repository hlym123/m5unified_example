#include <M5Unified.h>
#include <cstdio>

namespace {
constexpr uint32_t kRefreshIntervalMs = 500;
constexpr uint32_t kLogIntervalMs = 1000;

struct Button {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
  const char* label;
};

constexpr Button kChargeButton{28, 260, 424, 54, "CHARGE"};
constexpr Button kPortAButton{28, 326, 424, 54, "PORT.A 5V"};
constexpr Button kUsbButton{28, 392, 424, 54, "USB HOST 5V"};
constexpr int16_t kValueX = 226;
constexpr int16_t kValueWidth = 222;

bool s_charge_enabled = true;
bool s_port_a_enabled = false;
bool s_usb_enabled = false;

bool contains(const Button& button, int16_t x, int16_t y) {
  return x >= button.x && x < button.x + button.w
      && y >= button.y && y < button.y + button.h;
}

const char* chargingName(m5::Power_Class::is_charging_t state) {
  switch (state) {
    case m5::Power_Class::is_charging: return "CHARGING";
    case m5::Power_Class::is_discharging: return "NOT CHARGING";
    default: return "UNKNOWN";
  }
}

void drawButton(const Button& button, bool enabled) {
  const uint16_t color = enabled ? TFT_DARKGREEN : TFT_DARKGREY;
  char text[32];
  std::snprintf(text, sizeof(text), "%s  %s", button.label,
                enabled ? "ON" : "OFF");
  M5.Display.startWrite();
  M5.Display.fillRoundRect(button.x, button.y, button.w, button.h, 6, color);
  M5.Display.drawRoundRect(button.x, button.y, button.w, button.h, 6, TFT_WHITE);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setFont(&fonts::Font4);
  M5.Display.setTextColor(TFT_WHITE, color);
  M5.Display.drawString(text, button.x + button.w / 2,
                        button.y + button.h / 2);
  M5.Display.endWrite();
}

void drawStaticScreen() {
  M5.Display.startWrite();
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextDatum(top_center);
  M5.Display.setFont(&fonts::FreeMonoBold9pt7b);
  M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
  M5.Display.drawString("CoreP4X Power", M5.Display.width() / 2, 22);

  M5.Display.setTextDatum(middle_left);
  M5.Display.setFont(&fonts::Font4);
  M5.Display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  M5.Display.drawString("VIN", 32, 92);
  M5.Display.drawString("BATTERY", 32, 136);
  M5.Display.drawString("LEVEL", 32, 180);
  M5.Display.drawString("STATUS", 32, 224);
  M5.Display.endWrite();

  drawButton(kChargeButton, s_charge_enabled);
  drawButton(kPortAButton, s_port_a_enabled);
  drawButton(kUsbButton, s_usb_enabled);
}

void drawValue(const char* text, int16_t y, uint16_t color) {
  M5.Display.setTextDatum(middle_left);
  M5.Display.setFont(&fonts::Font4);
  M5.Display.setTextColor(color, TFT_BLACK);
  M5.Display.setTextPadding(kValueWidth);
  M5.Display.drawString(text, kValueX, y);
  M5.Display.setTextPadding(0);
}

void drawValues() {
  const int vin_mv = M5.Power.getVBUSVoltage();
  const int battery_mv = M5.Power.getBatteryVoltage();
  const long level = M5.Power.getBatteryLevel();
  const auto charging = M5.Power.isCharging();
  char text[32];

  M5.Display.startWrite();
  std::snprintf(text, sizeof(text), "%d mV", vin_mv);
  drawValue(text, 92, TFT_WHITE);
  std::snprintf(text, sizeof(text), "%d mV", battery_mv);
  drawValue(text, 136, TFT_WHITE);
  std::snprintf(text, sizeof(text), "%ld %%", level);
  drawValue(text, 180, TFT_WHITE);
  drawValue(chargingName(charging), 224,
            charging == m5::Power_Class::is_charging ? TFT_GREEN : TFT_YELLOW);
  M5.Display.endWrite();
}

void handleTouch(int16_t x, int16_t y) {
  if (contains(kChargeButton, x, y)) {
    s_charge_enabled = !s_charge_enabled;
    M5.Power.setBatteryCharge(s_charge_enabled);
    drawButton(kChargeButton, s_charge_enabled);
    Serial.printf("[POWER] charge=%d\n", s_charge_enabled);
  } else if (contains(kPortAButton, x, y)) {
    s_port_a_enabled = !s_port_a_enabled;
    M5.Power.setExtOutput(s_port_a_enabled, m5::ext_PA);
    drawButton(kPortAButton, s_port_a_enabled);
    Serial.printf("[POWER] port_a_5v=%d readback=%d\n", s_port_a_enabled,
                  M5.Power.getExtOutput());
  } else if (contains(kUsbButton, x, y)) {
    s_usb_enabled = !s_usb_enabled;
    M5.Power.setUsbOutput(s_usb_enabled);
    drawButton(kUsbButton, s_usb_enabled);
    Serial.printf("[POWER] usb_host_5v=%d readback=%d\n", s_usb_enabled,
                  M5.Power.getUsbOutput());
  }
}

void logStatus() {
  Serial.printf("[POWER] type=%d vin=%dmV battery=%dmV level=%ld status=%s "
                "charge_en=%d port_a=%d usb=%d 5vout=%umV\n",
                static_cast<int>(M5.Power.getType()), M5.Power.getVBUSVoltage(),
                M5.Power.getBatteryVoltage(),
                static_cast<long>(M5.Power.getBatteryLevel()),
                chargingName(M5.Power.isCharging()), s_charge_enabled,
                M5.Power.getExtOutput(), M5.Power.getUsbOutput(),
                M5.Power.M5pm1.get5VoutVoltage());
}
}  // namespace

void setup() {
  Serial.begin(115200);
  auto cfg = M5.config();
  cfg.internal_mic = false;
  cfg.internal_spk = false;
  M5.begin(cfg);
  M5.Display.setRotation(0);

  s_charge_enabled = true;
  s_port_a_enabled = false;
  s_usb_enabled = false;
  M5.Power.setBatteryCharge(s_charge_enabled);
  M5.Power.setExtOutput(s_port_a_enabled, m5::ext_PA);
  M5.Power.setUsbOutput(s_usb_enabled);

  Serial.printf("[POWER] board=%d pmic=%d M5PM1=%s M5IOE1=%s\n",
                static_cast<int>(M5.getBoard()),
                static_cast<int>(M5.Power.getType()),
                M5.In_I2C.scanID(0x6E) ? "OK" : "MISSING",
                M5.In_I2C.scanID(0x4F) ? "OK" : "MISSING");
  drawStaticScreen();
  drawValues();
  logStatus();
}

void loop() {
  M5.update();
  static uint32_t next_refresh_ms = 0;
  static uint32_t next_log_ms = 0;
  const uint32_t now = millis();

  if (M5.Touch.getCount() && M5.Touch.getDetail(0).wasPressed()) {
    const auto& touch = M5.Touch.getDetail(0);
    handleTouch(touch.x, touch.y);
  }

  if (now >= next_refresh_ms) {
    next_refresh_ms = now + kRefreshIntervalMs;
    drawValues();
  }
  if (now >= next_log_ms) {
    next_log_ms = now + kLogIntervalMs;
    logStatus();
  }

  M5.delay(5);
}
