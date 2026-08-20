#include <M5Unified.h>
#include <cstdio>
#include <ctime>

namespace {
constexpr uint32_t kLogIntervalMs = 5000;
constexpr uint32_t kRtcRefreshIntervalMs = 500;

struct Button {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
  const char* label;
};

constexpr int16_t kFieldX[] = {90, 240, 390};
constexpr Button kSetButton{32, 368, 416, 72, "SET TIME"};
constexpr Button kPlusButtons[] = {
    {36, 112, 108, 52, "+"}, {186, 112, 108, 52, "+"}, {336, 112, 108, 52, "+"}};
constexpr Button kMinusButtons[] = {
    {36, 270, 108, 52, "-"}, {186, 270, 108, 52, "-"}, {336, 270, 108, 52, "-"}};
constexpr Button kCancelButton{32, 368, 196, 72, "CANCEL"};
constexpr Button kSaveButton{252, 368, 196, 72, "SAVE"};

tm s_live_time = {};
int s_hour = 0;
int s_minute = 0;
int s_second = 0;
bool s_editing = false;
bool s_rtc_ok = false;
bool s_volt_low = false;
int s_drawn_hour = -1;
int s_drawn_minute = -1;
int s_drawn_second = -1;
int s_drawn_year = -1;
int s_drawn_month = -1;
int s_drawn_day = -1;
char s_status[40] = "Clock";

bool contains(const Button& button, int16_t x, int16_t y) {
  return x >= button.x && x < button.x + button.w && y >= button.y && y < button.y + button.h;
}

void setStatus(const char* status) {
  std::snprintf(s_status, sizeof(s_status), "%s", status);
}

bool readRtc() {
  if (!M5.Rtc.isEnabled()) {
    s_rtc_ok = false;
    s_volt_low = false;
    return false;
  }

  m5::rtc_datetime_t dt;
  if (!M5.Rtc.getDateTime(&dt)) {
    s_rtc_ok = false;
    s_volt_low = M5.Rtc.getVoltLow();
    return false;
  }

  s_live_time = {};
  s_live_time.tm_year = dt.date.year - 1900;
  s_live_time.tm_mon = dt.date.month - 1;
  s_live_time.tm_mday = dt.date.date;
  s_live_time.tm_wday = dt.date.weekDay;
  s_live_time.tm_hour = dt.time.hours;
  s_live_time.tm_min = dt.time.minutes;
  s_live_time.tm_sec = dt.time.seconds;
  s_live_time.tm_isdst = 0;
  s_rtc_ok = true;
  s_volt_low = M5.Rtc.getVoltLow();
  return true;
}

void copyLiveTime() {
  s_hour = s_live_time.tm_hour;
  s_minute = s_live_time.tm_min;
  s_second = s_live_time.tm_sec;
}

void drawButton(const Button& button, uint16_t color) {
  M5.Display.fillRoundRect(button.x, button.y, button.w, button.h, 8, color);
  M5.Display.drawRoundRect(button.x, button.y, button.w, button.h, 8, TFT_WHITE);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setFont(&fonts::Font4);
  M5.Display.setTextColor(TFT_WHITE, color);
  M5.Display.drawString(button.label, button.x + button.w / 2, button.y + button.h / 2);
}

void resetDrawCache() {
  s_drawn_hour = -1;
  s_drawn_minute = -1;
  s_drawn_second = -1;
  s_drawn_year = -1;
  s_drawn_month = -1;
  s_drawn_day = -1;
}

void drawDate(bool force = false) {
  const int year = s_live_time.tm_year + 1900;
  const int month = s_live_time.tm_mon + 1;
  const int day = s_live_time.tm_mday;
  if (!force && year == s_drawn_year && month == s_drawn_month && day == s_drawn_day) return;

  M5.Display.startWrite();
  char line[32];
  M5.Display.fillRect(0, 72, M5.Display.width(), 42, TFT_BLACK);
  M5.Display.setTextDatum(top_center);
  M5.Display.setFont(&fonts::Font4);
  if (s_rtc_ok) {
    std::snprintf(line, sizeof(line), "%04d-%02d-%02d", year, month, day);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  } else {
    std::snprintf(line, sizeof(line), "RTC NOT FOUND");
    M5.Display.setTextColor(TFT_RED, TFT_BLACK);
  }
  M5.Display.drawString(line, M5.Display.width() / 2, 76);
  s_drawn_year = year;
  s_drawn_month = month;
  s_drawn_day = day;
  M5.Display.endWrite();
}

void drawStatus() {
  M5.Display.startWrite();
  M5.Display.fillRect(0, 292, M5.Display.width(), 38, TFT_BLACK);
  M5.Display.setTextDatum(top_center);
  M5.Display.setFont(&fonts::Font2);
  M5.Display.setTextColor(s_volt_low ? TFT_YELLOW : TFT_GREEN, TFT_BLACK);
  M5.Display.drawString(s_volt_low ? "RTC VOLTAGE LOW" : s_status, M5.Display.width() / 2, 300);
  M5.Display.endWrite();
}

void drawTimeField(int value, int16_t x, int16_t y) {
  char text[3];
  std::snprintf(text, sizeof(text), "%02d", value);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setFont(&fonts::Font7);
  M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
  M5.Display.setTextPadding(108);
  M5.Display.drawString(text, x, y);
  M5.Display.setTextPadding(0);
}

void drawSeparators(int16_t y) {
  M5.Display.setTextDatum(middle_center);
  M5.Display.setFont(&fonts::Font7);
  M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
  M5.Display.drawString(":", 165, y);
  M5.Display.drawString(":", 315, y);
}

void drawTime(bool force = false) {
  const int16_t y = s_editing ? 218 : 190;
  M5.Display.startWrite();
  if (force) drawSeparators(y);
  if (force || s_hour != s_drawn_hour) drawTimeField(s_hour, kFieldX[0], y);
  if (force || s_minute != s_drawn_minute) drawTimeField(s_minute, kFieldX[1], y);
  if (force || s_second != s_drawn_second) drawTimeField(s_second, kFieldX[2], y);
  s_drawn_hour = s_hour;
  s_drawn_minute = s_minute;
  s_drawn_second = s_second;
  M5.Display.endWrite();
}

void drawLiveScreen() {
  s_editing = false;
  resetDrawCache();
  M5.Display.startWrite();
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextDatum(top_center);
  M5.Display.setFont(&fonts::FreeMonoBold9pt7b);
  M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
  M5.Display.drawString("CoreP4X RTC Clock", M5.Display.width() / 2, 28);
  drawDate(true);
  drawTime(true);
  drawStatus();
  drawButton(kSetButton, TFT_DARKCYAN);
  M5.Display.endWrite();
}

void drawEditScreen() {
  s_editing = true;
  resetDrawCache();
  M5.Display.startWrite();
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextDatum(top_center);
  M5.Display.setFont(&fonts::FreeMonoBold9pt7b);
  M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
  M5.Display.drawString("Set Time", M5.Display.width() / 2, 22);

  M5.Display.setFont(&fonts::Font2);
  M5.Display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  M5.Display.drawString("HOUR", kFieldX[0], 76);
  M5.Display.drawString("MIN", kFieldX[1], 76);
  M5.Display.drawString("SEC", kFieldX[2], 76);

  for (const auto& button : kPlusButtons) drawButton(button, TFT_DARKGREEN);
  for (const auto& button : kMinusButtons) drawButton(button, TFT_MAROON);
  drawTime(true);
  drawButton(kCancelButton, TFT_DARKGREY);
  drawButton(kSaveButton, TFT_DARKCYAN);
  M5.Display.endWrite();
}

void beginEdit() {
  if (!readRtc()) {
    setStatus("RTC not found");
    drawStatus();
    return;
  }
  copyLiveTime();
  drawEditScreen();
  Serial.printf("[RTC] edit begin %02d:%02d:%02d\n", s_hour, s_minute, s_second);
}

void cancelEdit() {
  readRtc();
  copyLiveTime();
  setStatus("Clock");
  drawLiveScreen();
  Serial.println("[RTC] edit cancelled");
}

void saveEditTime() {
  if (!readRtc()) {
    setStatus("RTC not found");
    drawLiveScreen();
    Serial.println("[RTC] save failed: rtc not found");
    return;
  }

  tm value = s_live_time;
  value.tm_hour = s_hour;
  value.tm_min = s_minute;
  value.tm_sec = s_second;
  M5.Rtc.setDateTime(&value);
  M5.Rtc.setSystemTimeFromRtc();
  M5.delay(20);

  readRtc();
  copyLiveTime();
  setStatus("Saved");
  drawLiveScreen();
  Serial.printf("[RTC] save %04d-%02d-%02d %02d:%02d:%02d\n",
                value.tm_year + 1900, value.tm_mon + 1, value.tm_mday,
                value.tm_hour, value.tm_min, value.tm_sec);
}

void adjustField(int index, int delta) {
  int* values[] = {&s_hour, &s_minute, &s_second};
  const int limits[] = {24, 60, 60};
  *values[index] = (*values[index] + delta + limits[index]) % limits[index];
  drawTime();
  Serial.printf("[RTC] edit %02d:%02d:%02d\n", s_hour, s_minute, s_second);
}

void handleTouch(int16_t x, int16_t y) {
  if (!s_editing) {
    if (contains(kSetButton, x, y)) beginEdit();
    return;
  }

  for (int i = 0; i < 3; ++i) {
    if (contains(kPlusButtons[i], x, y)) {
      adjustField(i, 1);
      return;
    }
    if (contains(kMinusButtons[i], x, y)) {
      adjustField(i, -1);
      return;
    }
  }
  if (contains(kCancelButton, x, y)) {
    cancelEdit();
  } else if (contains(kSaveButton, x, y)) {
    saveEditTime();
  }
}

void logRtc() {
  Serial.printf("[RTC] enabled=%d ok=%d volt_low=%d time=%04d-%02d-%02d %02d:%02d:%02d mode=%s\n",
                M5.Rtc.isEnabled(), s_rtc_ok, s_volt_low,
                s_live_time.tm_year + 1900, s_live_time.tm_mon + 1, s_live_time.tm_mday,
                s_live_time.tm_hour, s_live_time.tm_min, s_live_time.tm_sec,
                s_editing ? "EDIT" : "LIVE");
}
}  // namespace

void setup() {
  Serial.begin(115200);
  auto cfg = M5.config();
  cfg.internal_rtc = true;
  M5.begin(cfg);
  M5.Display.setRotation(0);
  M5.Display.setTextSize(1);

  readRtc();
  copyLiveTime();
  drawLiveScreen();
  logRtc();
}

void loop() {
  M5.update();

  static uint32_t next_log_ms = 0;
  static int last_live_second = -1;
  static uint32_t next_rtc_refresh_ms = 0;
  const uint32_t now = millis();

  if (!s_editing && now >= next_rtc_refresh_ms) {
    next_rtc_refresh_ms = now + kRtcRefreshIntervalMs;
    if (readRtc() && s_live_time.tm_sec != last_live_second) {
      copyLiveTime();
      drawDate();
      drawTime();
      last_live_second = s_live_time.tm_sec;
    }
  }

  if (M5.Touch.getCount() && M5.Touch.getDetail(0).wasPressed()) {
    const auto& touch = M5.Touch.getDetail(0);
    handleTouch(touch.x, touch.y);
  }

  if (now >= next_log_ms) {
    next_log_ms = now + kLogIntervalMs;
    logRtc();
  }

  M5.delay(5);
}
