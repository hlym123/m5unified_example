#include <M5Unified.h>
#include <esp_heap_caps.h>

namespace {
constexpr uint16_t kStrokeColors[] = {
    TFT_RED, TFT_BLUE, TFT_GREEN, TFT_MAGENTA, TFT_CYAN, TFT_BLACK,
};
constexpr size_t kStrokeColorCount = sizeof(kStrokeColors) / sizeof(kStrokeColors[0]);
constexpr int32_t kMoveLogIntervalMs = 50;
constexpr int32_t kPointRadius = 5;

uint32_t s_stroke_count = 0;
uint32_t s_point_count = 0;
uint32_t s_last_move_log_ms = 0;
int16_t s_last_x = -1;
int16_t s_last_y = -1;
uint16_t s_stroke_color = kStrokeColors[0];
bool s_was_touching = false;

void drawCanvas() {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  M5.Display.fillScreen(TFT_WHITE);
  M5.Display.drawRect(0, 0, w, h, TFT_BLACK);
  M5.Display.drawLine(w / 2, 0, w / 2, h - 1, TFT_LIGHTGREY);
  M5.Display.drawLine(0, h / 2, w - 1, h / 2, TFT_LIGHTGREY);
  M5.Display.fillCircle(0, 0, 12, TFT_RED);
  M5.Display.fillCircle(w - 1, 0, 12, TFT_GREEN);
  M5.Display.fillCircle(0, h - 1, 12, TFT_BLUE);
  M5.Display.fillCircle(w - 1, h - 1, 12, TFT_YELLOW);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.setTextDatum(top_center);
  M5.Display.setFont(&fonts::Font2);
  M5.Display.drawString("CoreP4X TOUCH DRAW", w / 2, 12);
}
}

void setup() {
  Serial.begin(115200);
  // An unopened USB Serial/JTAG port must not stall the touch drawing loop.
  Serial.setTxTimeoutMs(0);
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
  Serial.println("Touch the screen and drag to draw points.");
  drawCanvas();
}

void loop() {
  M5.update();
  if (!M5.Touch.getCount()) {
    if (s_was_touching) {
      Serial.printf("touch release: x=%d y=%d stroke=%lu points=%lu\n", s_last_x, s_last_y,
                    static_cast<unsigned long>(s_stroke_count),
                    static_cast<unsigned long>(s_point_count));
      s_was_touching = false;
    }
    return;
  }

  const auto& touch = M5.Touch.getDetail(0);
  const bool in_bounds = touch.x >= 0 && touch.x < M5.Display.width() && touch.y >= 0 &&
                         touch.y < M5.Display.height();
  if (!in_bounds) {
    Serial.printf("touch out-of-range: x=%d y=%d\n", touch.x, touch.y);
    return;
  }

  if (touch.wasPressed() || !s_was_touching) {
    ++s_stroke_count;
    s_stroke_color = kStrokeColors[(s_stroke_count - 1) % kStrokeColorCount];
    Serial.printf("touch press: x=%d y=%d stroke=%lu\n", touch.x, touch.y,
                  static_cast<unsigned long>(s_stroke_count));
  }

  s_was_touching = true;
  s_last_x = touch.x;
  s_last_y = touch.y;
  ++s_point_count;
  M5.Display.fillCircle(touch.x, touch.y, kPointRadius, s_stroke_color);

  const uint32_t now = millis();
  if (now - s_last_move_log_ms >= kMoveLogIntervalMs) {
    s_last_move_log_ms = now;
    Serial.printf("touch move: x=%d y=%d stroke=%lu point=%lu\n", touch.x, touch.y,
                  static_cast<unsigned long>(s_stroke_count),
                  static_cast<unsigned long>(s_point_count));
  }
}
