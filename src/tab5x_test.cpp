#include <cstdio>
#include <cstdlib>

#include <M5Unified.h>

namespace {

constexpr uint16_t kColors[] = {
  TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE, TFT_BLACK,
};
constexpr const char* kColorNames[] = {
  "RED", "GREEN", "BLUE", "WHITE", "BLACK",
};
constexpr size_t kColorCount = sizeof(kColors) / sizeof(kColors[0]);

size_t color_index = 0;
bool touch_was_pressed = false;
int32_t last_touch_x = -1;
int32_t last_touch_y = -1;

void draw_color()
{
  const uint16_t bg = kColors[color_index];
  const uint16_t fg = (bg == TFT_WHITE || bg == TFT_GREEN) ? TFT_BLACK : TFT_WHITE;
  M5.Display.fillScreen(bg);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(fg, bg);
  M5.Display.setTextSize(2);
  M5.Display.drawString(kColorNames[color_index], M5.Display.width() / 2,
                        M5.Display.height() / 2 - 18);
  M5.Display.setTextSize(1);
  M5.Display.drawString("Tab5X  A/B: color", M5.Display.width() / 2,
                        M5.Display.height() / 2 + 18);
}

}  // namespace

void setup()
{
  auto cfg = M5.config();
  cfg.internal_imu = false;
  cfg.internal_mic = false;
  cfg.internal_spk = false;
  cfg.internal_rtc = false;
  M5.begin(cfg);

  const auto board = M5.getBoard();
  const auto display_board = M5.Display.getBoard();
  const bool board_ok = board == m5::board_t::board_M5Tab5X;
  const bool display_ok = display_board == m5::board_t::board_M5Tab5X;
  std::printf("[BOARD] id=%d expected=35 display_id=%d result=%s\n",
              static_cast<int>(board), static_cast<int>(display_board),
              board_ok ? "OK" : "FAIL");
  std::printf("[DISPLAY] %dx%d result=%s\n",
              static_cast<int>(M5.Display.width()),
              static_cast<int>(M5.Display.height()),
              display_ok ? "OK" : "FAIL");

  M5.Display.setRotation(0);
  draw_color();
}

void loop()
{
  M5.update();

  if (M5.BtnA.wasPressed()) {
    color_index = (color_index + kColorCount - 1) % kColorCount;
    draw_color();
  }
  if (M5.BtnB.wasPressed()) {
    color_index = (color_index + 1) % kColorCount;
    draw_color();
  }

  const auto touch = M5.Touch.getDetail();
  if (touch.isPressed()) {
    M5.Display.fillCircle(touch.x, touch.y, 5, TFT_CYAN);
    if (!touch_was_pressed || std::abs(touch.x - last_touch_x) >= 3
                           || std::abs(touch.y - last_touch_y) >= 3) {
      std::printf("[TOUCH] state=pressed x=%d y=%d\n", touch.x, touch.y);
      last_touch_x = touch.x;
      last_touch_y = touch.y;
    }
  } else if (touch_was_pressed) {
    std::printf("[TOUCH] state=released x=%ld y=%ld\n",
                static_cast<long>(last_touch_x),
                static_cast<long>(last_touch_y));
  }
  touch_was_pressed = touch.isPressed();
}
