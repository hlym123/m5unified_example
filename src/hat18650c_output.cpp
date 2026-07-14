// SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
//
// SPDX-License-Identifier: MIT

#include <M5Hat_18650C.h>
#include <M5Unified.h>
#include <Wire.h>

namespace {

constexpr int8_t kHatSda = 8;
constexpr int8_t kHatScl = 0;
constexpr uint32_t kHatI2cFrequency = 400000;
constexpr uint8_t kChargerAddress = 0x6A;
constexpr uint8_t kChargerControlRegister = 0x01;
constexpr uint8_t kBoostEnableMask = 0x01;
constexpr uint32_t kRefreshIntervalMs = 1000;
constexpr uint16_t kTitleColor = 0x15FB;
constexpr uint16_t kMutedColor = 0x8C51;
constexpr uint16_t kOutputColor = 0xFB26;

M5Hat_18650C hat;
M5Canvas canvas(&M5.Display);

bool hatReady = false;
bool outputEnabled = false;
uint32_t nextRefreshMs = 0;
uint32_t logSequence = 0;
uint32_t sampleCount = 0;
uint32_t errorCount = 0;

void drawText(const char *text, int32_t x, int32_t y, uint8_t font,
              uint16_t color, textdatum_t datum = top_left) {
  canvas.setTextDatum(datum);
  canvas.setTextFont(font);
  canvas.setTextColor(color, TFT_BLACK);
  canvas.drawString(text, x, y);
}

void drawHatMissing() {
  canvas.fillScreen(TFT_BLACK);
  drawText("OUTPUT", canvas.width() / 2, 6, 2, kOutputColor, top_center);
  drawText("HAT NOT FOUND", canvas.width() / 2, 92, 2, TFT_RED, middle_center);
  drawText("INA226 0x41", canvas.width() / 2, 122, 1, kMutedColor,
           middle_center);
  drawText("AW32257 0x6A", canvas.width() / 2, 138, 1, kMutedColor,
           middle_center);
  canvas.pushSprite(0, 0);
}

bool readOutputEnabled(bool &enabled) {
  Wire.beginTransmission(kChargerAddress);
  Wire.write(kChargerControlRegister);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  if (Wire.requestFrom(kChargerAddress, static_cast<uint8_t>(1),
                       static_cast<uint8_t>(true)) != 1) {
    return false;
  }

  enabled = (Wire.read() & kBoostEnableMask) != 0;
  return true;
}

void drawStatus() {
  const float voltage = hat.getBatteryVoltage();
  const float current = hat.getBatteryCurrent();
  const bool outputReadOk = readOutputEnabled(outputEnabled);
  const bool sampleOk = !isnan(voltage) && !isnan(current) && outputReadOk;
  ++sampleCount;
  if (!sampleOk) {
    ++errorCount;
  }
  const uint16_t valueColor = outputEnabled ? kOutputColor : TFT_WHITE;

  char voltageText[16];
  char currentText[16];
  char outputText[16];
  char errorText[24];

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
  snprintf(outputText, sizeof(outputText), "OUT %s",
           outputEnabled ? "ON" : "OFF");
  snprintf(errorText, sizeof(errorText), "ERR %lu/%lu",
           static_cast<unsigned long>(errorCount),
           static_cast<unsigned long>(sampleCount));

  canvas.fillScreen(TFT_BLACK);
  drawText("OUTPUT", canvas.width() / 2, 6, 2, kOutputColor, top_center);

  drawText("STATUS", 10, 36, 1, kTitleColor);
  drawText("VOLT", 18, 56, 1, kMutedColor);
  drawText(voltageText, 18, 70, 2, valueColor);
  drawText("CURR", 18, 102, 1, kMutedColor);
  drawText(currentText, 18, 116, 2, valueColor);

  drawText("CONTROL", 10, 151, 1, kTitleColor);
  drawText(outputText, 18, 167, 2, outputEnabled ? kOutputColor : kMutedColor);
  drawText(outputEnabled ? "BOOST" : "STANDBY", 18, 195, 1,
           outputEnabled ? kOutputColor : kMutedColor);
  drawText(errorText, 18, 208, 1, errorCount ? TFT_RED : kMutedColor);
  drawText("A:ON/OFF", canvas.width() / 2, 224, 1, kMutedColor, top_center);
  canvas.pushSprite(0, 0);

  Serial.printf(
      "[%06lu] VBAT=%.3fV current=%+.3fA output=%d err_count=%lu/%lu\r\n",
      static_cast<unsigned long>(++logSequence), voltage, current,
      outputEnabled ? 1 : 0, static_cast<unsigned long>(errorCount),
      static_cast<unsigned long>(sampleCount));
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

  readOutputEnabled(outputEnabled);
  Serial.println("HAT 18650C output ready");
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
  M5.Power.setExtOutput(true);

  canvas.setColorDepth(16);
  canvas.createSprite(M5.Display.width(), M5.Display.height());
  beginHat();
  nextRefreshMs = millis() + kRefreshIntervalMs;
}

void loop() {
  M5.update();

  bool redraw = false;
  if (hatReady && M5.BtnA.wasPressed()) {
    outputEnabled = !outputEnabled;
    hat.setBoostMode(outputEnabled);
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
