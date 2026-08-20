#include <M5Unified.h>
#include <cstdio>

namespace {
constexpr uint32_t kDisplayIntervalMs = 200;
constexpr uint32_t kLogIntervalMs = 1000;
constexpr int16_t kValueX = 238;
constexpr int16_t kValueWidth = 214;
constexpr int16_t kRowY[] = {128, 174, 220, 292, 338, 384};

void drawStaticScreen() {
  M5.Display.startWrite();
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextDatum(top_center);
  M5.Display.setFont(&fonts::FreeMonoBold9pt7b);
  M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
  M5.Display.drawString("CoreP4X BMI270", M5.Display.width() / 2, 28);

  M5.Display.setTextDatum(middle_left);
  M5.Display.setFont(&fonts::Font4);
  M5.Display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  M5.Display.drawString("ACC X", 28, kRowY[0]);
  M5.Display.drawString("ACC Y", 28, kRowY[1]);
  M5.Display.drawString("ACC Z", 28, kRowY[2]);
  M5.Display.drawString("GYRO X", 28, kRowY[3]);
  M5.Display.drawString("GYRO Y", 28, kRowY[4]);
  M5.Display.drawString("GYRO Z", 28, kRowY[5]);

  M5.Display.setTextDatum(bottom_center);
  M5.Display.setFont(&fonts::Font2);
  M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  M5.Display.drawString("ACC: g        GYRO: dps", M5.Display.width() / 2, 458);
  M5.Display.endWrite();
}

void drawValue(float value, int16_t y, uint16_t color, uint8_t decimals) {
  char text[20];
  std::snprintf(text, sizeof(text), decimals == 2 ? "%+.2f" : "%+.1f", value);
  M5.Display.setTextDatum(middle_left);
  M5.Display.setFont(&fonts::Font4);
  M5.Display.setTextColor(color, TFT_BLACK);
  M5.Display.setTextPadding(kValueWidth);
  M5.Display.drawString(text, kValueX, y);
  M5.Display.setTextPadding(0);
}

void drawValues(const m5::IMU_Class::imu_data_t& data) {
  M5.Display.startWrite();
  drawValue(data.accel.x, kRowY[0], TFT_GREEN, 2);
  drawValue(data.accel.y, kRowY[1], TFT_GREEN, 2);
  drawValue(data.accel.z, kRowY[2], TFT_GREEN, 2);
  drawValue(data.gyro.x, kRowY[3], TFT_YELLOW, 1);
  drawValue(data.gyro.y, kRowY[4], TFT_YELLOW, 1);
  drawValue(data.gyro.z, kRowY[5], TFT_YELLOW, 1);
  M5.Display.endWrite();
}

void drawNotFound() {
  M5.Display.fillScreen(TFT_RED);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setFont(&fonts::FreeMonoBold9pt7b);
  M5.Display.setTextColor(TFT_WHITE, TFT_RED);
  M5.Display.drawString("BMI270 NOT FOUND", M5.Display.width() / 2,
                        M5.Display.height() / 2);
}
}  // namespace

void setup() {
  Serial.begin(115200);
  auto cfg = M5.config();
  cfg.internal_imu = true;
  cfg.internal_mic = false;
  cfg.internal_spk = false;
  M5.begin(cfg);
  M5.Display.setRotation(0);

  Serial.printf("[IMU] board=%d enabled=%d\n", static_cast<int>(M5.getBoard()),
                M5.Imu.isEnabled());
  if (!M5.Imu.isEnabled()) {
    drawNotFound();
    return;
  }

  drawStaticScreen();
  M5.Imu.update();
  drawValues(M5.Imu.getImuData());
}

void loop() {
  M5.update();
  if (!M5.Imu.isEnabled()) {
    M5.delay(100);
    return;
  }

  static uint32_t next_display_ms = 0;
  static uint32_t next_log_ms = 0;
  const uint32_t now = millis();
  M5.Imu.update();
  const auto& data = M5.Imu.getImuData();

  if (now >= next_display_ms) {
    next_display_ms = now + kDisplayIntervalMs;
    drawValues(data);
  }

  if (now >= next_log_ms) {
    next_log_ms = now + kLogIntervalMs;
    Serial.printf("[IMU] acc=%+.3f,%+.3f,%+.3f gyro=%+.2f,%+.2f,%+.2f\n",
                  data.accel.x, data.accel.y, data.accel.z,
                  data.gyro.x, data.gyro.y, data.gyro.z);
  }

  M5.delay(5);
}
