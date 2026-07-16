#include <Arduino.h>
#include <M5Unified.h>
#include <esp_heap_caps.h>

#include <algorithm>
#include <cstring>

namespace {

enum class TestMode : uint8_t {
  screen,
  audio,
  power,
  count,
};

constexpr uint32_t kRecordRate = 16000;
constexpr uint32_t kMaxRecordSeconds = 20;
constexpr size_t kMaxRecordSamples = kRecordRate * kMaxRecordSeconds;
constexpr size_t kRecordChunkSamples = kRecordRate / 10;
constexpr uint16_t kColors[] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE, TFT_BLACK};
constexpr const char* kColorNames[] = {"RED", "GREEN", "BLUE", "WHITE", "BLACK"};

TestMode mode = TestMode::screen;
size_t color_index = 0;
uint8_t brightness_percent = 100;
bool charge_enabled = true;

int16_t* recording = nullptr;
size_t recorded_samples = 0;
uint32_t record_started_ms = 0;
bool recording_active = false;
bool recording_started = false;
size_t recording_chunk_samples = 0;
bool playback_active = false;
uint32_t next_ui_refresh_ms = 0;
uint32_t next_power_log_ms = 0;
uint32_t next_audio_codec_log_ms = 0;

void drawTitle(const char* title)
{
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextDatum(top_center);
  M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
  M5.Display.setTextSize(2);
  M5.Display.drawString(title, M5.Display.width() / 2, 10);
  M5.Display.drawFastHLine(8, 36, M5.Display.width() - 16, TFT_DARKGREY);
}

void drawScreenPage()
{
  const uint16_t background = kColors[color_index];
  const uint16_t foreground = (background == TFT_WHITE || background == TFT_GREEN)
                            ? TFT_BLACK
                            : TFT_WHITE;

  M5.Display.fillScreen(background);
  M5.Display.drawRect(0, 0, M5.Display.width(), M5.Display.height(), foreground);
  M5.Display.drawRect(3, 3, M5.Display.width() - 6, M5.Display.height() - 6, foreground);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(foreground, background);
  M5.Display.setTextSize(2);
  M5.Display.drawString(kColorNames[color_index], M5.Display.width() / 2,
                        M5.Display.height() / 2 - 14);

  char brightness[24];
  snprintf(brightness, sizeof(brightness), "%u%%", brightness_percent);
  M5.Display.setTextSize(1);
  M5.Display.drawString(brightness, M5.Display.width() / 2,
                        M5.Display.height() / 2 + 18);
  M5.Display.setTextDatum(bottom_center);
  M5.Display.drawString("A:PAGE  B:COLOR  C:BRIGHT", M5.Display.width() / 2,
                        M5.Display.height() - 10);
}

const char* audioStatus()
{
  if (!recording) { return "BUFFER ERROR"; }
  if (recording_active) { return "RECORDING"; }
  if (playback_active) { return "PLAYING"; }
  if (recorded_samples) { return "RECORDED"; }
  return "READY";
}

void drawAudioPage()
{
  drawTitle("AUDIO TEST");
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(recording_active ? TFT_RED
                          : playback_active ? TFT_GREEN
                          : TFT_WHITE,
                          TFT_BLACK);
  M5.Display.drawString(audioStatus(), M5.Display.width() / 2, 83);

  uint32_t duration_ms = 0;
  if (recording_active) {
    duration_ms = millis() - record_started_ms;
  } else if (recorded_samples) {
    duration_ms = static_cast<uint32_t>((recorded_samples * 1000ULL) / kRecordRate);
  }
  char duration[32];
  snprintf(duration, sizeof(duration), "%lu.%01lus / %lus",
           static_cast<unsigned long>(duration_ms / 1000),
           static_cast<unsigned long>((duration_ms / 100) % 10),
           static_cast<unsigned long>(kMaxRecordSeconds));
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  M5.Display.drawString(duration, M5.Display.width() / 2, 119);

  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.drawString("B: START / STOP RECORD", M5.Display.width() / 2, 157);
  M5.Display.drawString("C: START / STOP PLAY", M5.Display.width() / 2, 181);
  M5.Display.setTextDatum(bottom_center);
  M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
  M5.Display.drawString("A: NEXT PAGE", M5.Display.width() / 2,
                        M5.Display.height() - 10);
}

void stopPlayback()
{
  if (!playback_active) { return; }
  M5.Speaker.stop();
  delay(10);
  M5.Speaker.end();
  playback_active = false;
  Serial.println("[AUDIO] playback stopped");
}

void stopRecording()
{
  if (!recording_active) { return; }

  M5.Mic.end();
  if (recording_started) {
    recorded_samples = std::min(kMaxRecordSamples,
                                recorded_samples + recording_chunk_samples);
  }
  recording_chunk_samples = 0;
  recording_active = false;
  recording_started = false;
  Serial.printf("[AUDIO] recording stopped, samples=%u\n",
                static_cast<unsigned>(recorded_samples));
}

bool checkAudioCodec(const char* stage)
{
  const bool online = M5.In_I2C.scanID(0x18);
  Serial.printf("[AUDIO] ES8311 %s: %s\n", stage, online ? "OK" : "MISSING");
  return online;
}

bool queueRecordingChunk()
{
  recording_chunk_samples = std::min(kRecordChunkSamples,
                                     kMaxRecordSamples - recorded_samples);
  recording_started = false;
  if (!recording_chunk_samples) { return false; }
  if (M5.Mic.record(recording + recorded_samples,
                    recording_chunk_samples, kRecordRate)) {
    return true;
  }
  recording_chunk_samples = 0;
  return false;
}

void startRecording()
{
  if (!recording || recording_active) { return; }
  stopPlayback();
  std::memset(recording, 0, kMaxRecordSamples * sizeof(int16_t));
  recorded_samples = 0;
  M5.Speaker.end();
  delay(20);
  M5.Mic.begin();
  if (!checkAudioCodec("after Mic.begin")) {
    M5.Mic.end();
    return;
  }
  if (!queueRecordingChunk()) {
    Serial.println("[AUDIO] record start failed");
    M5.Mic.end();
    return;
  }
  record_started_ms = millis();
  recording_active = true;
  Serial.println("[AUDIO] recording started");
}

void toggleRecording()
{
  if (recording_active) {
    stopRecording();
  } else {
    startRecording();
  }
  drawAudioPage();
}

void startPlayback()
{
  if (!recording || !recorded_samples || recording_active || playback_active) { return; }
  M5.Mic.end();
  delay(20);
  M5.Speaker.begin();
  M5.Speaker.setVolume(200);
  if (!checkAudioCodec("after Speaker.begin")) {
    M5.Speaker.end();
    return;
  }
  if (!M5.Speaker.playRaw(recording, recorded_samples, kRecordRate,
                          false, UINT32_MAX, -1, true)) {
    Serial.println("[AUDIO] playback start failed");
    M5.Speaker.end();
    return;
  }
  playback_active = true;
  Serial.printf("[AUDIO] playback started, samples=%u\n",
                static_cast<unsigned>(recorded_samples));
}

void togglePlayback()
{
  if (playback_active) {
    stopPlayback();
  } else {
    startPlayback();
  }
  drawAudioPage();
}

void drawPowerPage()
{
  drawTitle("POWER TEST");

  const int32_t vin_raw_mv = M5.Power.getVBUSVoltage();
  const int32_t usb_mv = vin_raw_mv >= 2000 ? vin_raw_mv + 530 : vin_raw_mv;
  const int32_t battery_mv = M5.Power.getBatteryVoltage();
  const auto charge_state = M5.Power.isCharging();
  const bool charging = charge_state == m5::Power_Class::is_charging;
  const uint8_t ioe_input = M5.In_I2C.readRegister8(0x4F, 0x07, 100000);
  const uint8_t power_config = M5.In_I2C.readRegister8(0x6E, 0x06, 100000);
  const bool charge_pin_low = !(ioe_input & (1 << 2));

  char line[40];
  M5.Display.setTextDatum(middle_left);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  snprintf(line, sizeof(line), "USB: %ld mV", static_cast<long>(usb_mv));
  M5.Display.drawString(line, 18, 61);

  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  snprintf(line, sizeof(line), "VIN RAW: %ld mV", static_cast<long>(vin_raw_mv));
  M5.Display.drawString(line, 18, 87);

  M5.Display.setTextSize(2);
  M5.Display.setTextColor(charging ? TFT_GREEN : TFT_WHITE, TFT_BLACK);
  snprintf(line, sizeof(line), "BAT: %ld mV", static_cast<long>(battery_mv));
  M5.Display.drawString(line, 18, 119);

  M5.Display.setTextSize(1);
  M5.Display.setTextColor(charging ? TFT_GREEN : TFT_LIGHTGREY, TFT_BLACK);
  snprintf(line, sizeof(line), "G3:%s  API:%s",
           charge_pin_low ? "LOW" : "HIGH",
           charging ? "CHARGING" : "NOT CHARGING");
  M5.Display.drawString(line, 18, 151);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  snprintf(line, sizeof(line), "CHARGE:%s  PWR_CFG:0x%02X",
           charge_enabled ? "ON" : "OFF", power_config);
  M5.Display.drawString(line, 18, 178);
  M5.Display.setTextDatum(bottom_center);
  M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
  M5.Display.drawString("A:PAGE  B:CHARGE ON/OFF", M5.Display.width() / 2,
                        M5.Display.height() - 10);

  if (millis() >= next_power_log_ms) {
    next_power_log_ms = millis() + 1000;
    Serial.printf("[POWER] vin_raw=%ld usb=%ld bat=%ld ioe_in=0x%02X g3=%s api=%u pwr_cfg=0x%02X\n",
                  static_cast<long>(vin_raw_mv), static_cast<long>(usb_mv),
                  static_cast<long>(battery_mv), ioe_input,
                  charge_pin_low ? "LOW" : "HIGH",
                  static_cast<unsigned>(charge_state), power_config);
  }
}

void stopAudioForPageChange()
{
  if (recording_active) { stopRecording(); }
  if (playback_active) { stopPlayback(); }
}

void drawCurrentPage()
{
  switch (mode) {
    case TestMode::screen: drawScreenPage(); break;
    case TestMode::audio: drawAudioPage(); break;
    case TestMode::power: drawPowerPage(); break;
    default: break;
  }
}

void selectNextMode()
{
  if (mode == TestMode::audio) { stopAudioForPageChange(); }
  mode = static_cast<TestMode>((static_cast<uint8_t>(mode) + 1)
                               % static_cast<uint8_t>(TestMode::count));
  Serial.printf("[PAGE] mode=%u\n", static_cast<unsigned>(mode));
  next_ui_refresh_ms = 0;
  drawCurrentPage();
}

void handleScreenButtons()
{
  if (M5.BtnB.wasClicked()) {
    color_index = (color_index + 1) % (sizeof(kColors) / sizeof(kColors[0]));
    Serial.printf("[DISPLAY] color=%s\n", kColorNames[color_index]);
    drawScreenPage();
  }
  if (M5.BtnC.wasClicked()) {
    brightness_percent = brightness_percent >= 100 ? 20 : brightness_percent + 20;
    M5.Display.setBrightness((brightness_percent * 255 + 50) / 100);
    Serial.printf("[DISPLAY] brightness=%u%%\n", brightness_percent);
    drawScreenPage();
  }
}

void handleAudioButtons()
{
  if (M5.BtnB.wasClicked()) { toggleRecording(); }
  if (M5.BtnC.wasClicked()) { togglePlayback(); }

  if (recording_active) {
    if (M5.Mic.isRecording()) {
      recording_started = true;
    } else if (recording_started) {
      recorded_samples = std::min(kMaxRecordSamples,
                                  recorded_samples + recording_chunk_samples);
      recording_chunk_samples = 0;
      recording_started = false;
      if (recorded_samples >= kMaxRecordSamples) {
        M5.Mic.end();
        recording_active = false;
        Serial.println("[AUDIO] recording buffer full");
        drawAudioPage();
      } else if (!queueRecordingChunk()) {
        M5.Mic.end();
        recording_active = false;
        Serial.println("[AUDIO] record queue failed");
        drawAudioPage();
      }
    }
  }

  if (millis() >= next_ui_refresh_ms) {
    next_ui_refresh_ms = millis() + 250;
    drawAudioPage();
  }
}

void handlePowerButtons()
{
  if (M5.BtnB.wasClicked()) {
    charge_enabled = !charge_enabled;
    M5.Power.setBatteryCharge(charge_enabled);
    Serial.printf("[POWER] charge enable=%u\n", charge_enabled);
    drawPowerPage();
  }
  if (millis() >= next_ui_refresh_ms) {
    next_ui_refresh_ms = millis() + 500;
    drawPowerPage();
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

}  // namespace

void setup()
{
  Serial.begin(115200);
  delay(1500);
  Serial.println("[BOOT] before M5.begin");

  auto cfg = M5.config();
  cfg.internal_rtc = true;
  cfg.internal_imu = true;
  cfg.internal_mic = true;
  cfg.internal_spk = true;
  cfg.output_power = false;
  M5.begin(cfg);

  Serial.println("[BOOT] after M5.begin");
  Serial.printf("[BOARD] expected=%d detected=%d result=%s\n",
                static_cast<int>(m5::board_t::board_M5ChainCaptain),
                static_cast<int>(M5.getBoard()),
                M5.getBoard() == m5::board_t::board_M5ChainCaptain ? "PASS" : "FAIL");
  printI2cDevices();

  M5.Speaker.setVolume(200);
  const bool tone_started = M5.Speaker.tone(1000, 1000);
  delay(50);
  Serial.printf("[AUDIO] boot tone=%s ES8311=%s\n",
                tone_started ? "STARTED" : "FAILED",
                M5.In_I2C.scanID(0x18) ? "OK" : "MISSING");
  delay(1100);
  M5.Speaker.stop();
  M5.Speaker.end();

  recording = static_cast<int16_t*>(heap_caps_malloc(
      kMaxRecordSamples * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  Serial.printf("[AUDIO] buffer=%s samples=%u\n", recording ? "OK" : "FAIL",
                static_cast<unsigned>(kMaxRecordSamples));

  charge_enabled = true;
  M5.Power.setBatteryCharge(charge_enabled);
  M5.Display.setBrightness(255);
  drawCurrentPage();
}

void loop()
{
  M5.update();

  if (mode == TestMode::audio
      && static_cast<int32_t>(millis() - next_audio_codec_log_ms) >= 0) {
    next_audio_codec_log_ms = millis() + 1000;
    checkAudioCodec("periodic");
  }

  if (M5.BtnA.wasClicked()) {
    selectNextMode();
  } else {
    switch (mode) {
      case TestMode::screen: handleScreenButtons(); break;
      case TestMode::audio: handleAudioButtons(); break;
      case TestMode::power: handlePowerButtons(); break;
      default: break;
    }
  }
  delay(1);
}
