#include "M5Unified.h"

static constexpr uint16_t kColors[] = {RED, GREEN, BLUE, WHITE, BLACK};
static constexpr const char* kColorNames[] = {"RED", "GREEN", "BLUE", "WHITE", "BLACK"};
static constexpr size_t kColorCount = sizeof(kColors) / sizeof(kColors[0]);
static constexpr uint8_t kBrightnessLevels[] = {32, 64, 96, 128, 160, 192, 224, 255};
static constexpr size_t kBrightnessCount = sizeof(kBrightnessLevels) / sizeof(kBrightnessLevels[0]);
static constexpr size_t kRotationCount = 4;
static constexpr int kBorderWidth = 3;

static void drawColorScreen(size_t color_index, uint8_t brightness, uint8_t rotation)
{
    const uint16_t bg = kColors[color_index];
    const bool light_bg = bg == WHITE;
    const int width = M5.Lcd.width();
    const int height = M5.Lcd.height();

    M5.Lcd.fillScreen(WHITE);
    M5.Lcd.fillRect(kBorderWidth, kBorderWidth,
                    width - 2 * kBorderWidth, height - 2 * kBorderWidth, bg);
    M5.Lcd.setTextDatum(middle_center);
    M5.Lcd.setTextColor(light_bg ? BLACK : WHITE, bg);
    M5.Lcd.setFont(&fonts::FreeMono9pt7b);
    M5.Lcd.drawString(kColorNames[color_index], width / 2, height / 2 - 24);

    char status[48];
    snprintf(status, sizeof(status), "R%u  B%u", rotation, brightness);
    M5.Lcd.drawString(status, width / 2, height / 2 + 4);
    M5.Lcd.drawString("A/B:color C:rotate", width / 2, height / 2 + 28);
}

void setup(void)
{
    auto cfg = M5.config();
    cfg.internal_mic = false;
    cfg.internal_spk = false;
    cfg.internal_imu = false;
    cfg.internal_rtc = false;
    cfg.output_power = false;
    M5.begin(cfg);
    M5.Lcd.setRotation(0);
    M5.Lcd.setBrightness(kBrightnessLevels[3]);
    Serial.printf("Display test: board=%d size=%dx%d rotation=%d brightness=%u\n",
                  static_cast<int>(M5.getBoard()), M5.Lcd.width(), M5.Lcd.height(),
                  M5.Lcd.getRotation(), M5.Lcd.getBrightness());
    drawColorScreen(0, M5.Lcd.getBrightness(), M5.Lcd.getRotation());
}

void loop(void)
{
    M5.update();
    static size_t color_index = 0;
    static size_t brightness_index = 3;
    static uint8_t rotation = 0;

    if (M5.BtnA.wasPressed()) {
        color_index = (color_index + kColorCount - 1) % kColorCount;
        drawColorScreen(color_index, kBrightnessLevels[brightness_index], rotation);
    }
    if (M5.BtnB.wasPressed()) {
        color_index = (color_index + 1) % kColorCount;
        drawColorScreen(color_index, kBrightnessLevels[brightness_index], rotation);
    }
    if (M5.BtnC.wasClicked()) {
        rotation = (rotation + 1) % kRotationCount;
        M5.Lcd.setRotation(rotation);
        Serial.printf("display rotation=%u size=%dx%d\n", rotation, M5.Lcd.width(), M5.Lcd.height());
        drawColorScreen(color_index, kBrightnessLevels[brightness_index], rotation);
    }
    if (M5.BtnC.wasHold()) {
        brightness_index = (brightness_index + 1) % kBrightnessCount;
        M5.Lcd.setBrightness(kBrightnessLevels[brightness_index]);
        Serial.printf("display brightness=%u\n", M5.Lcd.getBrightness());
        drawColorScreen(color_index, M5.Lcd.getBrightness(), rotation);
    }
}
