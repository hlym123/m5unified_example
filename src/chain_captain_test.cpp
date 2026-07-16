#include <Arduino.h>
#include <M5Unified.h>
#include <esp_heap_caps.h>

namespace {

constexpr uint32_t kRecordRate = 16000;
constexpr uint32_t kRecordSeconds = 3;
constexpr size_t kRecordSamples = kRecordRate * kRecordSeconds;
constexpr uint16_t kColors[] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE, TFT_BLACK};
constexpr const char* kColorNames[] = {"RED", "GREEN", "BLUE", "WHITE", "BLACK"};

int16_t* recording = nullptr;
bool port_a_enabled = false;
bool port_b_enabled = false;
bool audio_busy = false;

void drawStatus()
{
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(8, 8);
  M5.Display.printf("ChainCaptain ID: %d\n", static_cast<int>(M5.getBoard()));
  M5.Display.printf("RTC: %s  IMU: %s\n",
                    M5.Rtc.isEnabled() ? "OK" : "FAIL",
                    M5.Imu.isEnabled() ? "OK" : "FAIL");
  M5.Display.printf("BAT: %d mV\n", M5.Power.getBatteryVoltage());

  const auto charging = M5.Power.isCharging();
  M5.Display.printf("CHG: %s\n",
                    charging == m5::Power_Class::is_charging ? "YES" :
                    charging == m5::Power_Class::is_discharging ? "NO" : "UNKNOWN");
  M5.Display.printf("PortA 5V: %s\n", port_a_enabled ? "ON" : "OFF");
  M5.Display.printf("PortB 5V: %s\n", port_b_enabled ? "ON" : "OFF");
  M5.Display.println();
  M5.Display.println("A: record + playback");
  M5.Display.println("B: toggle Port A 5V");
  M5.Display.println("C: toggle Port B 5V");
}

void runDisplayTest()
{
  for (size_t i = 0; i < sizeof(kColors) / sizeof(kColors[0]); ++i) {
    Serial.printf("[DISPLAY] %s\n", kColorNames[i]);
    M5.Display.fillScreen(kColors[i]);
    M5.Display.display();
    delay(700);
  }
}

void printI2cDevices()
{
  constexpr uint8_t addresses[] = {0x18, 0x32, 0x4F, 0x68, 0x6E};
  constexpr const char* names[] = {"ES8311", "RX8130", "M5IOE1", "BMI270", "M5PM1"};
  for (size_t i = 0; i < sizeof(addresses); ++i) {
    Serial.printf("[I2C] %-7s 0x%02X: %s\n", names[i], addresses[i],
                  M5.In_I2C.scanID(addresses[i]) ? "OK" : "MISSING");
  }
}

void runAudioTest()
{
  if (!recording || audio_busy) { return; }
  audio_busy = true;

  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
  M5.Display.drawCenterString("Recording 3 seconds", M5.Display.width() / 2,
                              M5.Display.height() / 2 - 8);
  Serial.println("[AUDIO] recording");

  M5.Speaker.end();
  delay(20);
  M5.Mic.begin();
  if (!M5.Mic.record(recording, kRecordSamples, kRecordRate)) {
    Serial.println("[AUDIO] record start failed");
    audio_busy = false;
    drawStatus();
    return;
  }
  while (M5.Mic.isRecording()) {
    M5.update();
    delay(1);
  }

  M5.Mic.end();
  delay(20);
  M5.Speaker.begin();
  M5.Speaker.setVolume(200);
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
  M5.Display.drawCenterString("Playback", M5.Display.width() / 2,
                              M5.Display.height() / 2 - 8);
  Serial.println("[AUDIO] playback");
  M5.Speaker.playRaw(recording, kRecordSamples, kRecordRate, false, 1, -1);
  while (M5.Speaker.isPlaying()) {
    M5.update();
    delay(1);
  }
  M5.Speaker.end();
  M5.Mic.begin();
  Serial.println("[AUDIO] complete");

  audio_busy = false;
  drawStatus();
}

void printButtonEvents()
{
  if (M5.BtnA.wasPressed()) { Serial.println("[BUTTON] A pressed"); }
  if (M5.BtnA.wasReleased()) { Serial.println("[BUTTON] A released"); }
  if (M5.BtnB.wasPressed()) { Serial.println("[BUTTON] B pressed"); }
  if (M5.BtnB.wasReleased()) { Serial.println("[BUTTON] B released"); }
  if (M5.BtnC.wasPressed()) { Serial.println("[BUTTON] C pressed"); }
  if (M5.BtnC.wasReleased()) { Serial.println("[BUTTON] C released"); }
}

}  // namespace

void setup()
{
  Serial.begin(115200);
  delay(1500);

  auto cfg = M5.config();
  cfg.internal_rtc = true;
  cfg.internal_imu = true;
  cfg.internal_mic = true;
  cfg.internal_spk = true;
  cfg.output_power = false;
  M5.begin(cfg);

  Serial.println("\n=== M5Stack ChainCaptain hardware test ===");
  Serial.printf("[BOARD] expected=%d detected=%d result=%s\n",
                static_cast<int>(m5::board_t::board_M5ChainCaptain),
                static_cast<int>(M5.getBoard()),
                M5.getBoard() == m5::board_t::board_M5ChainCaptain ? "PASS" : "FAIL");
  printI2cDevices();
  runDisplayTest();

  recording = static_cast<int16_t*>(heap_caps_malloc(
      kRecordSamples * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  Serial.printf("[AUDIO] buffer=%s samples=%u\n", recording ? "OK" : "FAIL",
                static_cast<unsigned>(kRecordSamples));
  drawStatus();
}

void loop()
{
  M5.update();
  printButtonEvents();

  if (M5.BtnA.wasClicked()) {
    runAudioTest();
  }
  if (M5.BtnB.wasClicked()) {
    port_a_enabled = !port_a_enabled;
    M5.Power.setExtOutput(port_a_enabled, m5::ext_PA);
    Serial.printf("[POWER] Port A=%s voltage=%.0f mV\n", port_a_enabled ? "ON" : "OFF",
                  M5.Power.getExtVoltage(m5::ext_PA));
    drawStatus();
  }
  if (M5.BtnC.wasClicked()) {
    port_b_enabled = !port_b_enabled;
    M5.Power.setExtOutput(port_b_enabled, m5::ext_PB1);
    Serial.printf("[POWER] Port B=%s voltage=%.0f mV\n", port_b_enabled ? "ON" : "OFF",
                  M5.Power.getExtVoltage(m5::ext_PB1));
    drawStatus();
  }

  static uint32_t next_status = 0;
  if (!audio_busy && millis() >= next_status) {
    next_status = millis() + 2000;
    Serial.printf("[STATUS] board=%d rtc=%d imu=%d charging=%d ext=%d\n",
                  static_cast<int>(M5.getBoard()), M5.Rtc.isEnabled(), M5.Imu.isEnabled(),
                  static_cast<int>(M5.Power.isCharging()), M5.Power.getExtOutput());
  }
  delay(1);
}
