#include <M5Unified.h>
#include <cstdio>
#include <esp_heap_caps.h>

namespace {
constexpr uint16_t kColors[] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE, TFT_BLACK};
constexpr const char* kColorNames[] = {"RED", "GREEN", "BLUE", "WHITE", "BLACK"};
constexpr size_t kColorCount = sizeof(kColors) / sizeof(kColors[0]);
size_t s_color_index = 0;
uint32_t s_touch_count = 0;
int16_t s_touch_x = -1;
int16_t s_touch_y = -1;

void drawScreen() {
  const uint16_t bg = kColors[s_color_index];
  const uint16_t fg = bg == TFT_WHITE ? TFT_BLACK : TFT_WHITE;
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  char line[48];
  M5.Display.fillScreen(bg);
  M5.Display.setTextColor(fg, bg);
  M5.Display.setTextDatum(top_center);
  M5.Display.setFont(&fonts::Font2);
  M5.Display.drawString("CoreP4X display + touch", w / 2, 12);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setFont(&fonts::FreeMonoBold9pt7b);
  M5.Display.drawString(kColorNames[s_color_index], w / 2, h / 2 - 38);
  std::snprintf(line, sizeof(line), "board: %d", static_cast<int>(M5.getBoard()));
  M5.Display.drawString(line, w / 2, h / 2 - 10);
  std::snprintf(line, sizeof(line), "touches: %lu", static_cast<unsigned long>(s_touch_count));
  M5.Display.drawString(line, w / 2, h / 2 + 18);
  std::snprintf(line, sizeof(line), "last: %d, %d", s_touch_x, s_touch_y);
  M5.Display.drawString(line, w / 2, h / 2 + 46);
  std::snprintf(line, sizeof(line), "PSRAM free: %lu KiB",
                static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
  M5.Display.drawString(line, w / 2, h / 2 + 74);
  M5.Display.setFont(&fonts::Font0);
  M5.Display.drawString("Tap to change color", w / 2, h - 20);
}
}

void setup() {
  Serial.begin(115200);
  auto cfg = M5.config();
  cfg.internal_mic = false;
  cfg.internal_spk = false;
  M5.begin(cfg);
  M5.Display.setRotation(0);
  Serial.printf("CoreP4X display-touch: board=%d display=%dx%d touch=%s\n",
                static_cast<int>(M5.getBoard()), M5.Display.width(), M5.Display.height(),
                M5.Touch.isEnabled() ? "enabled" : "disabled");
  Serial.printf("psram_free=%lu\n",
                static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
  drawScreen();
}

void loop() {
  M5.update();
  if (!M5.Touch.getCount()) return;
  const auto& touch = M5.Touch.getDetail(0);
  s_touch_x = touch.x;
  s_touch_y = touch.y;
  if (touch.wasPressed()) {
    ++s_touch_count;
    s_color_index = (s_color_index + 1) % kColorCount;
    Serial.printf("touch press: x=%d y=%d count=%lu\n", touch.x, touch.y,
                  static_cast<unsigned long>(s_touch_count));
    drawScreen();
  }
}
