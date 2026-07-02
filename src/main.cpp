#include "M5Unified.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

namespace {

static constexpr uint32_t kDrawIntervalMs = 1000;
static constexpr const char* kWeekName[7] = {
  "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

char s_prev_lines[3][64] = {};

void resetRtcLineCache(void)
{
  for (auto& line : s_prev_lines) {
    line[0] = '\0';
  }
}

void drawRtcLine(int index, int y, const char* text, uint16_t color)
{
  if (std::strcmp(s_prev_lines[index], text) == 0) {
    return;
  }

  M5.Lcd.fillRect(0, y - 9, M5.Lcd.width(), 18, TFT_BLACK);
  M5.Lcd.setTextColor(color, TFT_BLACK);
  M5.Lcd.drawString(text, M5.Lcd.width() / 2, y);
  std::snprintf(s_prev_lines[index], sizeof(s_prev_lines[index]), "%s", text);
}

void drawRtcScreen(bool enabled, const m5::rtc_datetime_t* dt, bool low_voltage)
{
  const int cx = M5.Lcd.width() / 2;
  const int cy = M5.Lcd.height() / 2 + 8;
  static bool s_layout_drawn = false;
  static bool s_last_enabled = false;

  if (!s_layout_drawn || s_last_enabled != enabled) {
    s_layout_drawn = true;
    s_last_enabled = enabled;
    resetRtcLineCache();

    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.drawCircle(cx, cy, (M5.Lcd.height() / 2) - 4, TFT_DARKGREY);
    M5.Lcd.setTextDatum(top_center);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.setFont(&fonts::FreeMono9pt7b);
    M5.Lcd.drawString("RTC Demo", cx, 42);

    M5.Lcd.setTextDatum(middle_center);
    M5.Lcd.setFont(&fonts::Font2);

    if (!enabled) {
      M5.Lcd.setTextColor(TFT_RED, TFT_BLACK);
      M5.Lcd.drawString("RTC not found", cx, cy - 18);
      M5.Lcd.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
      M5.Lcd.drawString("Check board wiring", cx, cy + 12);
      M5.Lcd.drawString("or config", cx, cy + 36);
      return;
    }

    M5.Lcd.setTextColor(TFT_CYAN, TFT_BLACK);
    M5.Lcd.drawString("BtnA: sync from build", cx, cy + 58);
    M5.Lcd.drawString("BtnB: refresh", cx, cy + 86);
  }

  M5.Lcd.setTextDatum(middle_center);
  M5.Lcd.setFont(&fonts::Font2);

  char line[64];
  const int weekday = (dt->date.weekDay >= 0 && dt->date.weekDay < 7) ? dt->date.weekDay : 0;

  std::snprintf(line, sizeof(line), "Date : %04d-%02d-%02d %s",
                dt->date.year,
                dt->date.month,
                dt->date.date,
                kWeekName[weekday]);
  drawRtcLine(0, cy - 52, line, TFT_WHITE);

  std::snprintf(line, sizeof(line), "Time : %02d:%02d:%02d",
                dt->time.hours,
                dt->time.minutes,
                dt->time.seconds);
  drawRtcLine(1, cy - 16, line, TFT_WHITE);

  drawRtcLine(2, cy + 18,
              low_voltage ? "RTC status : voltage low" : "RTC status : OK",
              low_voltage ? TFT_YELLOW : TFT_GREEN);
}

bool parseBuildTime(tm* out)
{
  if (!out) { return false; }

  static constexpr const char* month_name = "JanFebMarAprMayJunJulAugSepOctNovDec";
  const char* build_date = __DATE__;
  const char* build_time = __TIME__;

  char month_str[4] = { build_date[0], build_date[1], build_date[2], '\0' };
  const char* month_pos = std::strstr(month_name, month_str);
  if (!month_pos) { return false; }

  tm t = {};
  t.tm_mon = (month_pos - month_name) / 3;
  t.tm_mday = std::atoi(build_date + 4);
  t.tm_year = std::atoi(build_date + 7) - 1900;
  t.tm_hour = std::atoi(build_time + 0);
  t.tm_min = std::atoi(build_time + 3);
  t.tm_sec = std::atoi(build_time + 6);
  t.tm_isdst = 0;
  mktime(&t);  // let libc derive tm_wday from the calendar date
  *out = t;
  return true;
}

void syncRtcFromBuildTime(void)
{
  tm build_tm;
  if (!parseBuildTime(&build_tm)) {
    return;
  }
  M5.Rtc.setDateTime(&build_tm);
  M5.Rtc.setSystemTimeFromRtc();
}

}  // namespace

void setup(void)
{
  Serial.begin(115200);
  delay(300);

  auto cfg = M5.config();
  cfg.internal_rtc = true;
  M5.begin(cfg);
  M5.Lcd.setRotation(0);
  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);

  if (M5.Rtc.isEnabled()) {
    syncRtcFromBuildTime();
  }

  m5::rtc_datetime_t dt;
  const bool ok = M5.Rtc.getDateTime(&dt);
  const bool rx8130_found = M5.In_I2C.isEnabled() && M5.In_I2C.scanID(0x32, 100000);
  const bool rtc_enabled = M5.Rtc.isEnabled();
  const bool rtc_low_voltage = rtc_enabled && M5.Rtc.getVoltLow();
  Serial.printf("[StickS3 RTC Demo] board=%d pmic=%d in_i2c=%d rx8130=%d rtc_enabled=%d rtc_ok=%d rtc_low_voltage=%d\r\n",
                static_cast<int>(M5.getBoard()),
                static_cast<int>(M5.Power.getType()),
                M5.In_I2C.isEnabled() ? 1 : 0,
                rx8130_found ? 1 : 0,
                rtc_enabled ? 1 : 0,
                ok ? 1 : 0,
                rtc_low_voltage ? 1 : 0);
  drawRtcScreen(rtc_enabled && ok, &dt, rtc_low_voltage);
}

void loop(void)
{
  M5.update();

  if (M5.BtnA.wasPressed() && M5.Rtc.isEnabled()) {
    syncRtcFromBuildTime();
  }

  static uint32_t next_draw_ms = 0;
  const uint32_t now = millis();
  if (!M5.BtnB.wasPressed() && now < next_draw_ms) {
    return;
  }
  next_draw_ms = now + kDrawIntervalMs;

  m5::rtc_datetime_t dt;
  const bool ok = M5.Rtc.getDateTime(&dt);
  drawRtcScreen(M5.Rtc.isEnabled() && ok, &dt, M5.Rtc.getVoltLow());
}
