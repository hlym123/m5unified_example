#include <M5Unified.h>
#include <cstdio>
#include <cstring>
#include <esp_heap_caps.h>

namespace {
constexpr uint32_t kSampleRate = 24000;
constexpr uint32_t kMaxRecordSeconds = 10;
constexpr size_t kChannelCount = 2;
constexpr size_t kMaxFrameCount = kSampleRate * kMaxRecordSeconds;
constexpr size_t kMaxSampleCount = kMaxFrameCount * kChannelCount;

enum class MicSource : uint8_t { Mic1, Mic2 };
enum class AudioState : uint8_t { Idle, Recording, Ready, Playing };

struct Button {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
  const char* label;
};

constexpr Button kMic1Button{24, 286, 204, 52, "MIC1"};
constexpr Button kMic2Button{252, 286, 204, 52, "MIC2"};
constexpr Button kRecordButton{24, 370, 204, 62, "REC START"};
constexpr Button kPlayButton{252, 370, 204, 62, "PLAY START"};

int16_t* s_samples = nullptr;
MicSource s_source = MicSource::Mic1;
AudioState s_state = AudioState::Idle;
size_t s_recorded_samples = 0;
size_t s_live_recorded_frames = 0;
uint32_t s_record_started_ms = 0;
uint16_t s_peak = 0;
const char* s_error = nullptr;
size_t s_peak_scanned_frames = 0;
uint32_t s_last_meter_ms = 0;
bool s_audio_probe_valid = false;
bool s_codec_present = false;
bool s_amp_enabled = false;

bool contains(const Button& button, int16_t x, int16_t y) {
  return x >= button.x && x < button.x + button.w && y >= button.y && y < button.y + button.h;
}

const char* sourceName() {
  return s_source == MicSource::Mic1 ? "MIC1" : "MIC2";
}

const char* stateName() {
  switch (s_state) {
    case AudioState::Recording: return "RECORDING";
    case AudioState::Ready: return "READY";
    case AudioState::Playing: return "PLAYING";
    default: return "IDLE";
  }
}

void drawButton(const Button& button, uint16_t color, bool selected, bool enabled) {
  const uint16_t bg = enabled ? color : TFT_DARKGREY;
  const uint16_t fg = enabled ? TFT_WHITE : TFT_LIGHTGREY;
  M5.Display.fillRect(button.x, button.y, button.w, button.h, bg);
  M5.Display.drawRect(button.x, button.y, button.w, button.h, selected ? TFT_YELLOW : TFT_WHITE);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setFont(&fonts::FreeMonoBold9pt7b);
  M5.Display.setTextColor(fg, bg);
  M5.Display.drawString(button.label, button.x + button.w / 2, button.y + button.h / 2);
}

void drawScreen() {
  const int cx = M5.Display.width() / 2;
  char line[64];
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextDatum(top_center);
  M5.Display.setFont(&fonts::FreeMonoBold9pt7b);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.drawString("CoreP4X Audio Test", cx, 26);
  std::snprintf(line, sizeof(line), "source: %s   state: %s", sourceName(), stateName());
  M5.Display.drawString(line, cx, 78);
  const size_t displayed_frames = s_state == AudioState::Recording ? s_live_recorded_frames : s_recorded_samples;
  std::snprintf(line, sizeof(line), "recorded: %lu ms   peak: %u",
                static_cast<unsigned long>((displayed_frames * 1000) / kSampleRate), s_peak);
  M5.Display.drawString(line, cx, 122);
  M5.Display.setFont(&fonts::Font2);
  M5.Display.drawString(s_state == AudioState::Recording ? "Tap REC STOP when finished" : "Choose source, then record and play", cx, 190);
  if (s_error) {
    M5.Display.setTextColor(TFT_RED, TFT_BLACK);
    M5.Display.drawString(s_error, cx, 230);
  }
  if (s_audio_probe_valid) {
    std::snprintf(line, sizeof(line), "ES8311:%s  AMP:%s  I2S:%s",
                  s_codec_present ? "OK" : "FAIL", s_amp_enabled ? "ON" : "OFF",
                  M5.Speaker.isPlaying() ? "PLAY" : "STOP");
    M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
    M5.Display.drawString(line, cx, 252);
  }

  drawButton(kMic1Button, TFT_BLUE, s_source == MicSource::Mic1, s_state != AudioState::Recording && s_state != AudioState::Playing);
  drawButton(kMic2Button, TFT_BLUE, s_source == MicSource::Mic2, s_state != AudioState::Recording && s_state != AudioState::Playing);

  Button record = kRecordButton;
  record.label = s_state == AudioState::Recording ? "REC STOP" : "REC START";
  drawButton(record, s_state == AudioState::Recording ? TFT_RED : TFT_DARKGREEN, false, s_state != AudioState::Playing);

  Button play = kPlayButton;
  play.label = s_state == AudioState::Playing ? "PLAY STOP" : "PLAY START";
  drawButton(play, s_state == AudioState::Playing ? TFT_RED : TFT_DARKCYAN, false,
             s_state == AudioState::Ready || s_state == AudioState::Playing);
}

void configureMicSource() {
  M5.Mic.end();
  auto cfg = M5.Mic.config();
  cfg.input_channel = m5::input_stereo;
  cfg.sample_rate = kSampleRate;
  M5.Mic.config(cfg);
}

void updatePeak(size_t frame_count) {
  if (frame_count > kMaxFrameCount) frame_count = kMaxFrameCount;
  uint16_t window_peak = 0;
  const size_t channel = s_source == MicSource::Mic1 ? 0 : 1;
  for (size_t i = s_peak_scanned_frames; i < frame_count; ++i) {
    const int32_t sample = s_samples[i * kChannelCount + channel];
    const uint32_t magnitude = static_cast<uint32_t>(sample < 0 ? -sample : sample);
    if (magnitude > window_peak) window_peak = static_cast<uint16_t>(magnitude > 32767 ? 32767 : magnitude);
  }
  s_peak = window_peak;
  s_peak_scanned_frames = frame_count;
}

void startRecording() {
  if (!s_samples || s_state == AudioState::Playing) return;
  s_error = nullptr;
  M5.Speaker.stop();
  M5.Speaker.end();
  configureMicSource();
  if (!M5.Mic.begin()) {
    s_error = "MIC BEGIN FAILED";
    Serial.println("audio: mic begin failed");
    drawScreen();
    return;
  }
  if (!M5.Mic.record(s_samples, kMaxSampleCount, kSampleRate, true)) {
    M5.Mic.end();
    s_error = "MIC RECORD FAILED";
    Serial.println("audio: record start failed");
    drawScreen();
    return;
  }
  s_record_started_ms = millis();
  s_recorded_samples = 0;
  s_live_recorded_frames = 0;
  s_peak = 0;
  s_peak_scanned_frames = 0;
  s_last_meter_ms = s_record_started_ms;
  s_state = AudioState::Recording;
  Serial.printf("audio: record start source=%s\n", sourceName());
  drawScreen();
}

void stopRecording() {
  if (s_state != AudioState::Recording) return;
  const uint32_t elapsed_ms = millis() - s_record_started_ms;
  size_t recorded_frames = static_cast<size_t>((static_cast<uint64_t>(elapsed_ms) * kSampleRate) / 1000);
  if (recorded_frames > kMaxFrameCount) recorded_frames = kMaxFrameCount;
  M5.Mic.end();
  updatePeak(recorded_frames);
  const size_t channel = s_source == MicSource::Mic1 ? 0 : 1;
  for (size_t i = 0; i < recorded_frames; ++i) {
    s_samples[i] = s_samples[i * kChannelCount + channel];
  }
  s_recorded_samples = recorded_frames;
  s_live_recorded_frames = recorded_frames;
  s_state = s_recorded_samples ? AudioState::Ready : AudioState::Idle;
  Serial.printf("audio: record stop source=%s samples=%lu peak=%u\n", sourceName(),
                static_cast<unsigned long>(s_recorded_samples), s_peak);
  drawScreen();
}

void startPlaying() {
  if (!s_samples || !s_recorded_samples || s_state == AudioState::Recording) return;
  s_error = nullptr;
  M5.Mic.end();
  if (!M5.Speaker.begin()) {
    s_error = "SPEAKER BEGIN FAILED";
    Serial.println("audio: play start failed");
    drawScreen();
    return;
  }
  M5.Speaker.setVolume(180);
  M5.Speaker.tone(1000, 1500);
  s_codec_present = M5.In_I2C.scanID(0x18, 100000);
  s_amp_enabled = (M5.In_I2C.readRegister8(0x4F, 0x05, 100000) & (1u << 2)) != 0;
  s_audio_probe_valid = true;
  s_state = AudioState::Playing;
  Serial.println("audio: 1 kHz diagnostic tone start");
  drawScreen();
}

void stopPlaying() {
  if (s_state != AudioState::Playing) return;
  M5.Speaker.stop();
  M5.Speaker.end();
  s_state = s_recorded_samples ? AudioState::Ready : AudioState::Idle;
  Serial.println("audio: play stop");
  drawScreen();
}

void handleTouch(int16_t x, int16_t y) {
  if (contains(kMic1Button, x, y) && s_state != AudioState::Recording && s_state != AudioState::Playing) {
    s_source = MicSource::Mic1;
    drawScreen();
  } else if (contains(kMic2Button, x, y) && s_state != AudioState::Recording && s_state != AudioState::Playing) {
    s_source = MicSource::Mic2;
    drawScreen();
  } else if (contains(kRecordButton, x, y)) {
    s_state == AudioState::Recording ? stopRecording() : startRecording();
  } else if (contains(kPlayButton, x, y)) {
    s_state == AudioState::Playing ? stopPlaying() : startPlaying();
  }
}
}

void setup() {
  Serial.begin(115200);
  auto cfg = M5.config();
  cfg.internal_mic = true;
  cfg.internal_spk = true;
  M5.begin(cfg);
  M5.Display.setRotation(0);
  s_samples = static_cast<int16_t*>(heap_caps_malloc(kMaxSampleCount * sizeof(*s_samples), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!s_samples) s_samples = static_cast<int16_t*>(heap_caps_malloc(kMaxSampleCount * sizeof(*s_samples), MALLOC_CAP_8BIT));
  if (s_samples) std::memset(s_samples, 0, kMaxSampleCount * sizeof(*s_samples));
  else s_error = "BUFFER ALLOC FAILED";
  Serial.printf("CoreP4X audio: board=%d max_samples=%lu\n", static_cast<int>(M5.getBoard()),
                static_cast<unsigned long>(kMaxSampleCount));
  drawScreen();
}

void loop() {
  M5.update();
  if (s_state == AudioState::Recording && millis() - s_last_meter_ms >= 200) {
    const uint32_t elapsed_ms = millis() - s_record_started_ms;
    s_live_recorded_frames = static_cast<size_t>((static_cast<uint64_t>(elapsed_ms) * kSampleRate) / 1000);
    if (s_live_recorded_frames > kMaxFrameCount) s_live_recorded_frames = kMaxFrameCount;
    updatePeak(s_live_recorded_frames);
    s_last_meter_ms = millis();
    drawScreen();
    if (s_live_recorded_frames == kMaxFrameCount) stopRecording();
  }
  if (s_state == AudioState::Playing && !M5.Speaker.isPlaying()) stopPlaying();
  if (M5.Touch.getCount() && M5.Touch.getDetail(0).wasPressed()) {
    const auto& touch = M5.Touch.getDetail(0);
    Serial.printf("audio: touch x=%d y=%d\n", touch.x, touch.y);
    handleTouch(touch.x, touch.y);
  }
}
