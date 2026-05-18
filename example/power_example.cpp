#include "M5Unified.h"
#include <cstdio>

// 圆形屏参考：466×466（如 StopWatch）；布局以屏中心为基准。
static constexpr int kDispR = 466 / 2;

static bool s_chg_en   = true;
static bool s_ext5v_en = true;

static void draw_power_ui(void)
{
  using m5::Power_Class;

  const int cx = M5.Lcd.width() / 2;
  const int cy = M5.Lcd.height() / 2;

  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.drawCircle(cx, cy, kDispR - 2, TFT_DARKGREY);

  const int16_t vbat = M5.Power.getBatteryVoltage();
  const int16_t vusb = M5.Power.getVBUSVoltage();
  // ext_port_mask_t 在命名空间 m5（见 Power_Class.hpp），如 ext_PA、ext_MAIN。
  const float vext = M5.Power.getExtVoltage(m5::ext_PA);

  const Power_Class::is_charging_t chg = M5.Power.isCharging();
  const bool charging = (chg == Power_Class::is_charging_t::is_charging);

  char line0[48], line1[48], line2[48], line3[48], line4[48];
  std::snprintf(line0, sizeof(line0), "BAT: %d mV", (int)vbat);
  if (vusb >= 0) {
    std::snprintf(line1, sizeof(line1), "USB: %d mV", (int)vusb);
  } else {
    std::snprintf(line1, sizeof(line1), "USB: --");
  }
  std::snprintf(line2, sizeof(line2), "EXT: %.2f V", (double)(vext / 1000.0f));
  std::snprintf(line3, sizeof(line3), "CHG: %s  (A)", s_chg_en ? "ON" : "OFF");
  std::snprintf(line4, sizeof(line4), "5Vout: %s (B)", s_ext5v_en ? "ON" : "OFF");

  M5.Lcd.setFont(&fonts::efontCN_12);
  M5.Lcd.setTextDatum(middle_center);

  M5.Lcd.setTextColor(charging ? TFT_GREEN : TFT_WHITE, TFT_BLACK);
  M5.Lcd.drawString(line0, cx, cy - 72);

  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Lcd.drawString(line1, cx, cy - 36);
  M5.Lcd.drawString(line2, cx, cy);
  M5.Lcd.drawString(line3, cx, cy + 36);
  M5.Lcd.drawString(line4, cx, cy + 72);

  M5.Lcd.setFont(&fonts::Font0);
  M5.Lcd.setTextColor(TFT_CYAN, TFT_BLACK);
  M5.Lcd.drawString(charging ? "charging" : "not chg", cx, cy + 108);
}

void setup(void)
{
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Lcd.setRotation(0);

  s_chg_en   = true;
  s_ext5v_en = true;
  M5.Power.setBatteryCharge(s_chg_en);
  M5.Power.setExtOutput(s_ext5v_en, m5::ext_PA);

  draw_power_ui();
}

void loop(void)
{
  M5.update();

  if (M5.BtnA.wasPressed()) {
    s_chg_en = !s_chg_en;
    M5.Power.setBatteryCharge(s_chg_en);
    draw_power_ui();
  }
  if (M5.BtnB.wasPressed()) {
    s_ext5v_en = !s_ext5v_en;
    M5.Power.setExtOutput(s_ext5v_en, m5::ext_PA);
    draw_power_ui();
  }

  static uint32_t s_next_ms;
  const uint32_t now = millis();
  if (now >= s_next_ms) {
    s_next_ms = now + 250;
    draw_power_ui();
  }
}
