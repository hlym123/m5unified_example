#include <M5Unified.h>
#include <climits>
#include <cstdio>

namespace {
struct Button {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
  const char* label;
  uint16_t frequency;
};

constexpr Button kButtons[] = {
  {24, 140, 204, 68, "250 Hz", 250},
  {252, 140, 204, 68, "500 Hz", 500},
  {24, 230, 204, 68, "1000 Hz", 1000},
  {252, 230, 204, 68, "2000 Hz", 2000},
};
constexpr Button kStopButton{138, 330, 204, 62, "STOP", 0};

uint16_t s_frequency = 0;
bool s_speaker_ready = false;
bool s_codec_present = false;
bool s_amp_enabled = false;
bool s_init_attempted = false;

constexpr uint8_t kM5IoeAddress = 0x4F;
constexpr uint8_t kEs8311Address = 0x18;
constexpr uint8_t kAudioPowerMask = 1u << 0;  // M5IOE1 G1
constexpr uint8_t kAmpEnableMask = 1u << 2;   // M5IOE1 G3

constexpr uint8_t kEs8311Init[][2] = {
  {0x0D, 0xFA}, {0x44, 0x08}, {0x44, 0x08}, {0x01, 0x30},
  {0x02, 0x00}, {0x03, 0x10}, {0x16, 0x24}, {0x04, 0x10},
  {0x05, 0x00}, {0x0B, 0x00}, {0x0C, 0x00}, {0x10, 0x1F},
  {0x11, 0x7F}, {0x00, 0x80}, {0x01, 0x3F}, {0x06, 0x03},
  {0x13, 0x10}, {0x1B, 0x0A}, {0x1C, 0x6A}, {0x44, 0x58},
  {0x09, 0x00}, {0x17, 0xBF}, {0x0E, 0x02}, {0x12, 0x00},
  {0x14, 0x1A}, {0x0D, 0x01}, {0x15, 0x40}, {0x37, 0x08},
  {0x45, 0x00}, {0x07, 0x00}, {0x08, 0xFF}, {0x32, 0xBF},
};

bool contains(const Button& button, int16_t x, int16_t y) {
  return x >= button.x && x < button.x + button.w && y >= button.y && y < button.y + button.h;
}

void probeAudioPath() {
  s_codec_present = M5.In_I2C.scanID(kEs8311Address, 100000);
  s_amp_enabled =
      (M5.In_I2C.readRegister8(kM5IoeAddress, 0x05, 100000) & kAmpEnableMask) != 0;
}

bool initSpeakerPath() {
  if (s_speaker_ready) return true;
  s_init_attempted = true;

  const uint8_t audio_mask = kAudioPowerMask | kAmpEnableMask;
  M5.In_I2C.bitOn(kM5IoeAddress, 0x05, audio_mask, 100000);
  M5.In_I2C.bitOff(kM5IoeAddress, 0x09, audio_mask, 100000);
  M5.In_I2C.bitOff(kM5IoeAddress, 0x0B, audio_mask, 100000);
  M5.In_I2C.bitOff(kM5IoeAddress, 0x13, audio_mask, 100000);
  M5.In_I2C.bitOn(kM5IoeAddress, 0x03, audio_mask, 100000);
  delay(20);

  auto speaker_cfg = M5.Speaker.config();
  speaker_cfg.pin_mck = GPIO_NUM_2;
  speaker_cfg.pin_bck = GPIO_NUM_6;
  speaker_cfg.pin_ws = GPIO_NUM_4;
  speaker_cfg.pin_data_out = GPIO_NUM_3;
  speaker_cfg.sample_rate = 24000;
  speaker_cfg.mclk_multiple = 256;
  speaker_cfg.i2s_port = I2S_NUM_0;
  speaker_cfg.magnification = 4;
  speaker_cfg.stereo = true;
  M5.Speaker.config(speaker_cfg);

  // I2S must be running so ES8311 sees MCLK while its registers are configured.
  if (!M5.Speaker.begin()) {
    probeAudioPath();
    return false;
  }
  delay(20);

  bool codec_ok = true;
  for (const auto& reg : kEs8311Init) {
    codec_ok = M5.In_I2C.writeRegister8(
                   kEs8311Address, reg[0], reg[1], 100000) &&
               codec_ok;
  }
  M5.Speaker.setVolume(220);
  probeAudioPath();
  s_speaker_ready = codec_ok && s_codec_present && s_amp_enabled;
  return s_speaker_ready;
}

void drawButton(const Button& button, uint16_t color, bool selected) {
  M5.Display.fillRect(button.x, button.y, button.w, button.h, color);
  M5.Display.drawRect(button.x, button.y, button.w, button.h, selected ? TFT_YELLOW : TFT_WHITE);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setFont(&fonts::FreeMonoBold9pt7b);
  M5.Display.setTextColor(TFT_WHITE, color);
  M5.Display.drawString(button.label, button.x + button.w / 2, button.y + button.h / 2);
}

void drawScreen() {
  char line[80];
  const int16_t cx = M5.Display.width() / 2;
  const bool playing = M5.Speaker.isPlaying();

  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextDatum(top_center);
  M5.Display.setFont(&fonts::FreeMonoBold9pt7b);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.drawString("CoreP4X Speaker Tone", cx, 22);

  std::snprintf(line, sizeof(line), "frequency: %u Hz   %s",
                s_frequency, playing ? "PLAYING" : "STOPPED");
  M5.Display.drawString(line, cx, 70);

  M5.Display.setFont(&fonts::Font2);
  const char* speaker_status = s_speaker_ready ? "OK" : (s_init_attempted ? "FAIL" : "WAIT");
  std::snprintf(line, sizeof(line), "SPK:%s  ES8311:%s  AMP:%s  I2S:%s",
                speaker_status,
                s_codec_present ? "OK" : "FAIL",
                s_amp_enabled ? "ON" : "OFF",
                playing ? "PLAY" : "STOP");
  M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
  M5.Display.drawString(line, cx, 108);

  for (const auto& button : kButtons) {
    drawButton(button, TFT_DARKCYAN, s_frequency == button.frequency && playing);
  }
  drawButton(kStopButton, TFT_RED, !playing);
}

void startTone(uint16_t frequency) {
  if (!initSpeakerPath()) {
    Serial.printf("speaker: init failed codec=%d amp=%d\n", s_codec_present, s_amp_enabled);
    drawScreen();
    return;
  }
  s_frequency = frequency;
  M5.Speaker.tone(frequency, UINT32_MAX, 0, true);
  delay(20);
  probeAudioPath();
  Serial.printf("speaker: tone=%u playing=%d codec=%d amp=%d\n",
                frequency, M5.Speaker.isPlaying(), s_codec_present, s_amp_enabled);
  drawScreen();
}

void stopTone() {
  M5.Speaker.stop(0);
  s_frequency = 0;
  delay(20);
  probeAudioPath();
  Serial.printf("speaker: stop playing=%d codec=%d amp=%d\n",
                M5.Speaker.isPlaying(), s_codec_present, s_amp_enabled);
  drawScreen();
}
}

void setup() {
  Serial.begin(115200);
  auto cfg = M5.config();
  cfg.internal_mic = false;
  cfg.internal_spk = false;
  M5.begin(cfg);
  M5.Display.setRotation(0);

  Serial.printf("CoreP4X speaker: board=%d deferred=%d codec=%d amp=%d\n",
                static_cast<int>(M5.getBoard()), s_speaker_ready, s_codec_present, s_amp_enabled);
  drawScreen();
}

void loop() {
  M5.update();
  if (!M5.Touch.getCount() || !M5.Touch.getDetail(0).wasPressed()) return;

  const auto& touch = M5.Touch.getDetail(0);
  Serial.printf("speaker: touch x=%d y=%d\n", touch.x, touch.y);
  for (const auto& button : kButtons) {
    if (contains(button, touch.x, touch.y)) {
      startTone(button.frequency);
      return;
    }
  }
  if (contains(kStopButton, touch.x, touch.y)) stopTone();
}
