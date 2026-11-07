#include <M5Unified.h>
#include <cstdio>
#include <esp_heap_caps.h>

namespace {
constexpr uint16_t kColors[] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE, TFT_BLACK};
constexpr const char* kColorNames[] = {"RED", "GREEN", "BLUE", "WHITE", "BLACK"};
constexpr size_t kColorCount = sizeof(kColors) / sizeof(kColors[0]);
size_t s_color_index = 0;
uint8_t s_rotation = 0;
uint32_t s_touch_count = 0;
int16_t s_touch_x = -1;
int16_t s_touch_y = -1;

void drawCorner(const char* label, int16_t x, int16_t y, uint16_t color) {
  constexpr int16_t kSize = 72;
  M5.Display.fillRect(x, y, kSize, kSize, color);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setFont(&fonts::Font4);
  M5.Display.setTextColor(TFT_BLACK, color);
  M5.Display.drawString(label, x + kSize / 2, y + kSize / 2);
}

void drawScreen() {
  const uint16_t bg = kColors[s_color_index];
  const uint16_t fg = bg == TFT_WHITE ? TFT_BLACK : TFT_WHITE;
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  char line[48];
  M5.Display.fillScreen(bg);
  drawCorner("TL", 0, 0, TFT_RED);
  drawCorner("TR", w - 72, 0, TFT_GREEN);
  drawCorner("BL", 0, h - 72, TFT_BLUE);
  drawCorner("BR", w - 72, h - 72, TFT_YELLOW);
  M5.Display.setTextColor(fg, bg);
  M5.Display.setTextDatum(top_center);
  M5.Display.setFont(&fonts::Font2);
  M5.Display.drawString("TOP", w / 2, 12);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setFont(&fonts::FreeMonoBold9pt7b);
  std::snprintf(line, sizeof(line), "ROTATION %u", s_rotation);
  M5.Display.drawString(line, w / 2, h / 2 - 66);
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
  M5.Display.drawString("Tap for rotation 0 - 7", w / 2, h - 84);
}
}

void setup() {
  Serial.begin(115200);
  auto cfg = M5.config();
  cfg.internal_mic = false;
  cfg.internal_spk = false;
  M5.begin(cfg);
  M5.Display.setRotation(s_rotation);
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
    s_rotation = (s_rotation + 1) & 7;
    M5.Display.setRotation(s_rotation);
    Serial.printf("touch press: x=%d y=%d count=%lu rotation=%u\n", touch.x, touch.y,
                  static_cast<unsigned long>(s_touch_count), s_rotation);
    drawScreen();
  }
}
