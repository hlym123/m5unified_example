// SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
//
// SPDX-License-Identifier: MIT

#include <M5Hat_18650C.h>
#include <M5Unified.h>
#include <Wire.h>

namespace {

constexpr int8_t kHatSda = 0;
constexpr int8_t kHatScl = 26;
constexpr uint32_t kHatI2cFrequency = 100000;
constexpr uint32_t kRefreshIntervalMs = 1000;
constexpr uint16_t kInitialChargeCurrentMa = 868;
constexpr uint16_t kChargeCurrentList[] = {500, 1000, 1500, 2500};

M5Hat_18650C hat;
M5Canvas canvas(&M5.Display);

bool hatReady = false;
bool chargeEnabled = true;
int8_t currentIndex = -1;
uint16_t chargeCurrentMa = kInitialChargeCurrentMa;
uint32_t nextRefreshMs = 0;

void drawText(const char *text, int32_t x, int32_t y, uint8_t font,
              uint16_t color, textdatum_t datum = top_left) {
  canvas.setTextDatum(datum);
  canvas.setTextFont(font);
  canvas.setTextColor(color, TFT_BLACK);
  canvas.drawString(text, x, y);
}

void drawHatMissing() {
  canvas.fillScreen(TFT_BLACK);
  drawText("CHARGE", canvas.width() / 2, 6, 2, TFT_GREEN, top_center);
  drawText("HAT NOT FOUND", canvas.width() / 2, 92, 2, TFT_RED, middle_center);
  drawText("INA226 0x41", canvas.width() / 2, 122, 1, TFT_DARKGREY,
           middle_center);
  drawText("AW32257 0x6A", canvas.width() / 2, 138, 1, TFT_DARKGREY,
           middle_center);
  canvas.pushSprite(0, 0);
}

void drawStatus() {
  const float voltage = hat.getBatteryVoltage();
  const float current = hat.getBatteryCurrent();
  const bool charging = hat.isCharging();
  const uint16_t valueColor = charging ? TFT_GREEN : TFT_WHITE;

  char voltageText[16];
  char currentText[16];
  char chargeText[16];
  char settingText[16];

  if (isnan(voltage)) {
    snprintf(voltageText, sizeof(voltageText), "--.--V");
  } else {
    snprintf(voltageText, sizeof(voltageText), "%4.2fV", voltage);
  }
  if (isnan(current)) {
    snprintf(currentText, sizeof(currentText), "--.--A");
  } else {
    snprintf(currentText, sizeof(currentText), "%+4.2fA", current);
  }
  snprintf(chargeText, sizeof(chargeText), "CHG %s",
           chargeEnabled ? "ON" : "OFF");
  snprintf(settingText, sizeof(settingText), "SET %umA", chargeCurrentMa);

  canvas.fillScreen(TFT_BLACK);
  drawText("CHARGE", canvas.width() / 2, 6, 2, TFT_GREEN, top_center);

  drawText("STATUS", 10, 36, 1, TFT_CYAN);
  drawText("VOLT", 18, 56, 1, TFT_DARKGREY);
  drawText(voltageText, 18, 70, 2, valueColor);
  drawText("CURR", 18, 102, 1, TFT_DARKGREY);
  drawText(currentText, 18, 116, 2, valueColor);

  drawText("CONTROL", 10, 151, 1, TFT_CYAN);
  drawText(chargeText, 18, 167, 2, chargeEnabled ? TFT_GREEN : TFT_RED);
  drawText(settingText, 18, 195, 1, TFT_DARKGREY);
  drawText("A:ON/OFF B:mA", 18, 222, 1, TFT_DARKGREY);
  canvas.pushSprite(0, 0);

  Serial.printf("VBAT=%.3fV current=%+.3fA charging=%d enabled=%d set=%umA\r\n",
                voltage, current, charging ? 1 : 0, chargeEnabled ? 1 : 0,
                chargeCurrentMa);
}

bool beginHat() {
  Wire.end();
  Wire.begin(kHatSda, kHatScl, kHatI2cFrequency);
  hatReady = hat.begin(&Wire);
  if (!hatReady) {
    Serial.println("HAT 18650C not found (INA226 0x41, AW32257 0x6A)");
    drawHatMissing();
    return false;
  }

  chargeEnabled = true;
  currentIndex = -1;
  chargeCurrentMa = kInitialChargeCurrentMa;
  hat.setChargerEnabled(chargeEnabled);
  Serial.println("HAT 18650C ready");
  drawStatus();
  return true;
}

} // namespace

void setup() {
  Serial.begin(115200);

  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setRotation(0);
  M5.Power.setBatteryCharge(true);
  M5.Power.setExtOutput(false);

  canvas.setColorDepth(16);
  canvas.createSprite(M5.Display.width(), M5.Display.height());
  beginHat();
  nextRefreshMs = millis() + kRefreshIntervalMs;
}

void loop() {
  M5.update();

  bool redraw = false;
  if (hatReady && M5.BtnA.wasPressed()) {
    chargeEnabled = !chargeEnabled;
    hat.setChargerEnabled(chargeEnabled);
    redraw = true;
  }

  if (hatReady && M5.BtnB.wasPressed()) {
    currentIndex = (currentIndex + 1) %
                   (sizeof(kChargeCurrentList) / sizeof(kChargeCurrentList[0]));
    chargeCurrentMa = kChargeCurrentList[currentIndex];
    hat.setChargeCurrent(chargeCurrentMa);
    redraw = true;
  }

  const uint32_t now = millis();
  if (redraw || static_cast<int32_t>(now - nextRefreshMs) >= 0) {
    nextRefreshMs = now + kRefreshIntervalMs;
    if (hatReady) {
      drawStatus();
    } else {
      beginHat();
    }
  }
}
