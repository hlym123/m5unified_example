#include "M5Unified.h"
#include <SD.h>
#include <SPI.h>
#include <cstdio>

namespace {

static constexpr uint32_t kPowerLogIntervalMs = 5000;
static constexpr uint32_t kScreenRefreshIntervalMs = 15000;
static constexpr int kCardDetectPin = 48;

SPIClass sd_spi(FSPI);
bool sd_mounted = false;
bool charge_enabled = true;
uint32_t key1_count = 0;
uint32_t key2_count = 0;
uint32_t power_key_count = 0;

const char* chargingStateName(m5::Power_Class::is_charging_t state)
{
  switch (state) {
  case m5::Power_Class::is_charging_t::is_charging:
    return "charging";
  case m5::Power_Class::is_charging_t::is_discharging:
    return "not charging";
  default:
    return "unknown";
  }
}

void printPowerStatus()
{
  Serial.printf("Power: PMIC=%u VBAT=%d mV VBUS=%d mV level=%d%% charge_enable=%s state=%s key=%u\n",
                static_cast<unsigned>(M5.Power.getType()),
                M5.Power.getBatteryVoltage(),
                M5.Power.getVBUSVoltage(),
                M5.Power.getBatteryLevel(),
                charge_enabled ? "on" : "off",
                chargingStateName(M5.Power.isCharging()),
                static_cast<unsigned>(M5.Power.getKeyState()));
}

void drawDashboard()
{
  char line[96];
  constexpr int left = 30;

  M5.Display.startWrite();
  M5.Display.fillScreen(TFT_WHITE);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.setTextDatum(top_left);
  M5.Display.setFont(&fonts::Font4);
  M5.Display.setTextSize(2);
  M5.Display.drawString("M5Stack PaperDIY", left, 24);
  M5.Display.drawFastHLine(left, 82, M5.Display.width() - left * 2, TFT_BLACK);

  M5.Display.setTextSize(1);
  std::snprintf(line, sizeof(line), "Board ID: %u [%s]",
                static_cast<unsigned>(M5.getBoard()),
                M5.getBoard() == m5::board_t::board_M5PaperDIY ? "OK" : "ERROR");
  M5.Display.drawString(line, left, 110);

  std::snprintf(line, sizeof(line), "Display: %d x %d",
                M5.Display.width(), M5.Display.height());
  M5.Display.drawString(line, left, 160);

  std::snprintf(line, sizeof(line), "M5PM1: %s",
                M5.Power.getType() == m5::Power_Class::pmic_t::pmic_m5pm1 ? "OK" : "ERROR");
  M5.Display.drawString(line, left, 220);

  std::snprintf(line, sizeof(line), "VBAT: %d mV    Battery: %d%%",
                M5.Power.getBatteryVoltage(), M5.Power.getBatteryLevel());
  M5.Display.drawString(line, left, 270);

  std::snprintf(line, sizeof(line), "VBUS: %d mV", M5.Power.getVBUSVoltage());
  M5.Display.drawString(line, left, 320);

  std::snprintf(line, sizeof(line), "Charge enable: %s", charge_enabled ? "ON" : "OFF");
  M5.Display.drawString(line, left, 370);

  std::snprintf(line, sizeof(line), "Charge state: %s", chargingStateName(M5.Power.isCharging()));
  M5.Display.drawString(line, left, 420);

  std::snprintf(line, sizeof(line), "TF: %s", sd_mounted ? "mounted" : "not mounted");
  M5.Display.drawString(line, left, 480);

  std::snprintf(line, sizeof(line), "Detect G48: %s",
                digitalRead(kCardDetectPin) == LOW ? "inserted" : "empty");
  M5.Display.drawString(line, left, 530);

  std::snprintf(line, sizeof(line), "KEY1 / BtnA: %u", static_cast<unsigned>(key1_count));
  M5.Display.drawString(line, left, 590);

  std::snprintf(line, sizeof(line), "KEY2 / BtnB: %u", static_cast<unsigned>(key2_count));
  M5.Display.drawString(line, left, 640);

  std::snprintf(line, sizeof(line), "PWR / BtnPWR: %u", static_cast<unsigned>(power_key_count));
  M5.Display.drawString(line, left, 690);

  M5.Display.drawString("KEY1 toggles battery charge", left, 770);
  M5.Display.drawString("KEY2/PWR verify input", left, 820);
  M5.Display.endWrite();
  M5.Display.display();
  M5.Display.waitDisplay();
}

void initSdCard()
{
  const int sclk = M5.getPin(m5::pin_name_t::sd_spi_sclk);
  const int mosi = M5.getPin(m5::pin_name_t::sd_spi_mosi);
  const int miso = M5.getPin(m5::pin_name_t::sd_spi_miso);
  const int cs = M5.getPin(m5::pin_name_t::sd_spi_cs);
  Serial.printf("TF pins: SCK=%d MOSI=%d MISO=%d CS=%d DET=%d\n",
                sclk, mosi, miso, cs, kCardDetectPin);

  pinMode(kCardDetectPin, INPUT_PULLUP);
  if (digitalRead(kCardDetectPin) != LOW) {
    Serial.println("TF not mounted; detect=empty");
    return;
  }
  sd_spi.begin(sclk, miso, mosi, cs);
  sd_mounted = SD.begin(cs, sd_spi, 25000000);
  if (sd_mounted) {
    Serial.printf("TF mounted: type=%u size=%llu MB\n",
                  static_cast<unsigned>(SD.cardType()),
                  static_cast<unsigned long long>(SD.cardSize() / (1024ULL * 1024ULL)));
  } else {
    Serial.printf("TF not mounted; detect=%s\n",
                  digitalRead(kCardDetectPin) == LOW ? "inserted" : "empty");
  }
}

}  // namespace

void setup()
{
  Serial.begin(115200);
  delay(1500);
  Serial.println("PaperDIY test boot");

  auto cfg = M5.config();
  cfg.internal_imu = false;
  cfg.internal_rtc = false;
  cfg.internal_spk = false;
  cfg.internal_mic = false;
  Serial.println("M5.begin start");
  M5.begin(cfg);
  Serial.println("M5.begin done");
  M5.Display.setRotation(0);
  charge_enabled = true;
  M5.Power.setBatteryCharge(charge_enabled);

  Serial.printf("Detected board ID: %u (expected 34)\n", static_cast<unsigned>(M5.getBoard()));
  Serial.printf("Display size: %d x %d\n", M5.Display.width(), M5.Display.height());
  Serial.printf("I2C pins: SCL=%d SDA=%d\n",
                M5.getPin(m5::pin_name_t::in_i2c_scl),
                M5.getPin(m5::pin_name_t::in_i2c_sda));
  if (M5.getBoard() != m5::board_t::board_M5PaperDIY) {
    Serial.println("ERROR: PaperDIY was not detected");
  }
  if (M5.Power.getType() != m5::Power_Class::pmic_t::pmic_m5pm1) {
    Serial.println("ERROR: M5PM1 was not initialized");
  }

  printPowerStatus();
  Serial.println("Initial dashboard draw start");
  drawDashboard();
  Serial.println("Initial dashboard draw done");
  Serial.println("TF test start");
  initSdCard();
  Serial.println("TF test done");
  drawDashboard();
}

void loop()
{
  M5.update();
  bool input_changed = false;
  if (M5.BtnA.wasPressed()) {
    ++key1_count;
    charge_enabled = !charge_enabled;
    M5.Power.setBatteryCharge(charge_enabled);
    input_changed = true;
    Serial.printf("KEY1 / BtnA: battery charge %s\n", charge_enabled ? "enabled" : "disabled");
  }
  if (M5.BtnB.wasPressed()) {
    ++key2_count;
    input_changed = true;
    Serial.printf("KEY2 / BtnB pressed: %u\n", static_cast<unsigned>(key2_count));
  }
  if (M5.BtnPWR.wasPressed()) {
    ++power_key_count;
    input_changed = true;
    Serial.printf("PWR / BtnPWR pressed: %u\n", static_cast<unsigned>(power_key_count));
  }

  static uint32_t next_power_log_ms = millis() + kPowerLogIntervalMs;
  if (static_cast<int32_t>(millis() - next_power_log_ms) >= 0) {
    printPowerStatus();
    next_power_log_ms = millis() + kPowerLogIntervalMs;
  }

  static uint32_t next_screen_refresh_ms = millis() + kScreenRefreshIntervalMs;
  if (input_changed || static_cast<int32_t>(millis() - next_screen_refresh_ms) >= 0) {
    drawDashboard();
    next_screen_refresh_ms = millis() + kScreenRefreshIntervalMs;
  }
}
