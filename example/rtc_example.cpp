#include "M5Unified.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

namespace {

static constexpr uint32_t kDrawIntervalMs = 250;
static constexpr const char* kWeekName[7] = {
  "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

void drawRtcScreen(bool enabled, const m5::rtc_datetime_t* dt, bool low_voltage)
{
  const int cx = M5.Lcd.width() / 2;
  const int cy = M5.Lcd.height() / 2 + 8;

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

  char line[64];
  const int weekday = (dt->date.weekDay >= 0 && dt->date.weekDay < 7) ? dt->date.weekDay : 0;

  std::snprintf(line, sizeof(line), "Date : %04d-%02d-%02d %s",
                dt->date.year,
                dt->date.month,
                dt->date.date,
                kWeekName[weekday]);
  M5.Lcd.drawString(line, cx, cy - 52);

  std::snprintf(line, sizeof(line), "Time : %02d:%02d:%02d",
                dt->time.hours,
                dt->time.minutes,
                dt->time.seconds);
  M5.Lcd.drawString(line, cx, cy - 16);

  M5.Lcd.setTextColor(low_voltage ? TFT_YELLOW : TFT_GREEN, TFT_BLACK);
  M5.Lcd.drawString(low_voltage ? "RTC status : voltage low" : "RTC status : OK", cx, cy + 18);

  M5.Lcd.setTextColor(TFT_CYAN, TFT_BLACK);
  M5.Lcd.drawString("BtnA: sync from build", cx, cy + 58);
  M5.Lcd.drawString("BtnB: refresh", cx, cy + 86);
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
  drawRtcScreen(M5.Rtc.isEnabled() && ok, &dt, M5.Rtc.getVoltLow());
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
