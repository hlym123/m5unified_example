#include "M5Unified.h"
 

static constexpr uint16_t kColors[] = {RED, GREEN, BLUE, WHITE, BLACK};
static constexpr const char* kColorNames[] = {"RED", "GREEN", "BLUE", "WHITE", "BLACK"};
static constexpr size_t kColorCount = sizeof(kColors) / sizeof(kColors[0]);

static void drawColorScreen(size_t index) {
    const uint16_t bg = kColors[index];
    const bool isLightBg = (bg == WHITE);

    M5.Lcd.fillScreen(bg);
    M5.Lcd.setTextDatum(middle_center);
    M5.Lcd.setTextColor(isLightBg ? BLACK : WHITE, bg);
    M5.Lcd.setFont(&fonts::FreeMono9pt7b);
    M5.Lcd.drawString(kColorNames[index], M5.Lcd.width() / 2, M5.Lcd.height() / 2 - 10);
    M5.Lcd.drawString("Press BtnA", M5.Lcd.width() / 2, M5.Lcd.height() / 2 + 20);
}

void setup(void) {
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Lcd.setRotation(1);

    drawColorScreen(0);
}

void loop(void) {
    M5.update();
    static size_t s_color_index = 0;
    if (M5.BtnA.wasPressed()) {
        s_color_index = (s_color_index + 1) % kColorCount;
        drawColorScreen(s_color_index);
    }
}
