#include <M5Unified.h>
#include <cstdio>

namespace {
constexpr uint32_t kRefreshMs = 100;

void drawStatus(const m5::IMU_Class::imu_data_t& data) {
  const int cx = M5.Display.width() / 2;
  char line[64];
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextDatum(top_center);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setFont(&fonts::FreeMonoBold9pt7b);
  M5.Display.drawString("CoreP4X BMI270", cx, 36);
  std::snprintf(line, sizeof(line), "Accel: %+.2f %+.2f %+.2f g", data.accel.x, data.accel.y, data.accel.z);
  M5.Display.drawString(line, cx, 122);
  std::snprintf(line, sizeof(line), "Gyro:  %+.1f %+.1f %+.1f dps", data.gyro.x, data.gyro.y, data.gyro.z);
  M5.Display.drawString(line, cx, 168);
  M5.Display.setFont(&fonts::Font0);
  M5.Display.drawString("Move the device to observe changes", cx, 438);
}
}

void setup() {
  Serial.begin(115200);
  auto cfg = M5.config();
  cfg.internal_imu = true;
  cfg.internal_mic = false;
  cfg.internal_spk = false;
  M5.begin(cfg);
  M5.Display.setRotation(0);
  Serial.printf("CoreP4X IMU: board=%d imu=%s\n", static_cast<int>(M5.getBoard()),
                M5.Imu.isEnabled() ? "enabled" : "disabled");
  if (!M5.Imu.isEnabled()) {
    M5.Display.fillScreen(TFT_RED);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setFont(&fonts::FreeMonoBold9pt7b);
    M5.Display.setTextColor(TFT_WHITE, TFT_RED);
    M5.Display.drawString("BMI270 not detected", M5.Display.width() / 2, M5.Display.height() / 2);
  }
}

void loop() {
  M5.update();
  if (!M5.Imu.isEnabled()) return;
  static uint32_t last_refresh = 0;
  if (millis() - last_refresh < kRefreshMs) return;
  last_refresh = millis();
  M5.Imu.update();
  const auto& data = M5.Imu.getImuData();
  Serial.printf("imu accel=%+.3f,%+.3f,%+.3f gyro=%+.2f,%+.2f,%+.2f\n", data.accel.x,
                data.accel.y, data.accel.z, data.gyro.x, data.gyro.y, data.gyro.z);
  drawStatus(data);
}
