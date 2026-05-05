#include "M5Unified.h"
#include <cstdio>

// IMU: accel XYZ + gyro XYZ. Library uses ~g and ~deg/s; show m/s^2 and rad/s.
// Title drawn once; only the data band is cleared each refresh to reduce flicker.

static constexpr uint32_t kDrawIntervalMs = 80;

static constexpr int kPad = 6;
static constexpr int kLineH = 18;
static constexpr int kDataTop = 40;

static constexpr float kGToMps2   = 9.80665f;
static constexpr float kDpsToRps  = 3.14159265f / 180.0f;

static void drawTitle(void) {
    M5.Lcd.setTextDatum(top_center);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setFont(&fonts::FreeMono9pt7b);
    M5.Lcd.drawString("IMU Read Example", M5.Lcd.width() / 2, 8);
}

static void drawImuValues(const m5::IMU_Class::imu_data_t &d) {
    const int w = M5.Lcd.width();
    const int h = M5.Lcd.height();

    M5.Lcd.fillRect(0, kDataTop, w, h - kDataTop, BLACK);
    M5.Lcd.setTextDatum(top_left);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setFont(&fonts::FreeMono9pt7b);

    char line[56];
    int y = kDataTop;

    const double ax = (double)d.accel.x * (double)kGToMps2;
    const double ay = (double)d.accel.y * (double)kGToMps2;
    const double az = (double)d.accel.z * (double)kGToMps2;
    const double gx = (double)d.gyro.x * (double)kDpsToRps;
    const double gy = (double)d.gyro.y * (double)kDpsToRps;
    const double gz = (double)d.gyro.z * (double)kDpsToRps;

    snprintf(line, sizeof(line), "Accel X: %7.2f m/s^2", ax);
    M5.Lcd.drawString(line, kPad, y);
    y += kLineH;
    snprintf(line, sizeof(line), "Accel Y: %7.2f m/s^2", ay);
    M5.Lcd.drawString(line, kPad, y);
    y += kLineH;
    snprintf(line, sizeof(line), "Accel Z: %7.2f m/s^2", az);
    M5.Lcd.drawString(line, kPad, y);
    y += kLineH;
    snprintf(line, sizeof(line), "Gyro X: %7.3f rad/s", gx);
    M5.Lcd.drawString(line, kPad, y);
    y += kLineH;
    snprintf(line, sizeof(line), "Gyro Y: %7.3f rad/s", gy);
    M5.Lcd.drawString(line, kPad, y);
    y += kLineH;
    snprintf(line, sizeof(line), "Gyro Z: %7.3f rad/s", gz);
    M5.Lcd.drawString(line, kPad, y);
}

void setup(void) {
    M5.begin();
    M5.Lcd.setRotation(1);
    M5.Lcd.setTextColor(WHITE);

    if (!M5.Imu.isEnabled()) {
        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.setTextDatum(middle_center);
        M5.Lcd.setFont(&fonts::FreeMono9pt7b);
        M5.Lcd.drawString("IMU Read Example", M5.Lcd.width() / 2, M5.Lcd.height() / 2 - 20);
        M5.Lcd.drawString("No IMU", M5.Lcd.width() / 2, M5.Lcd.height() / 2 + 10);
        return;
    }

    M5.Lcd.fillScreen(BLACK);
    drawTitle();
    M5.Imu.update();
    drawImuValues(M5.Imu.getImuData());
}

void loop(void) {
    M5.update();

    if (!M5.Imu.isEnabled()) {
        return;
    }

    M5.Imu.update();

    static uint32_t s_last_draw = 0;
    const uint32_t now = millis();
    if (now - s_last_draw < kDrawIntervalMs) {
        return;
    }
    s_last_draw = now;

    drawImuValues(M5.Imu.getImuData());
}
