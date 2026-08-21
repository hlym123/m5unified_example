#include <M5Unified.h>
#include <cstdio>
#include <cstring>
#include <esp_heap_caps.h>

namespace {
constexpr uint32_t kSampleRate = 16000;
constexpr uint32_t kMaxRecordSeconds = 30;
constexpr uint32_t kRecordChunkMs = 100;
constexpr size_t kChannelCount = 2;
constexpr size_t kMaxFrameCount = kSampleRate * kMaxRecordSeconds;
constexpr size_t kMaxSampleCount = kMaxFrameCount * kChannelCount;
constexpr size_t kRecordChunkFrames = (kSampleRate * kRecordChunkMs) / 1000;
constexpr uint8_t kMinVolume = 32;
constexpr uint8_t kMaxVolume = 255;
constexpr uint8_t kVolumeStep = 16;

enum class MicSource : uint8_t { Mic1, Mic2 };
enum class AudioState : uint8_t { Idle, Recording, Ready, Playing };

struct Button {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
  const char* label;
};

constexpr Button kMic1Button{24, 272, 204, 46, "MIC1"};
constexpr Button kMic2Button{252, 272, 204, 46, "MIC2"};
constexpr Button kRecordButton{24, 334, 204, 56, "REC START"};
constexpr Button kPlayButton{252, 334, 204, 56, "PLAY START"};
constexpr Button kVolumeDownButton{24, 418, 96, 46, "VOL-"};
constexpr Button kVolumeUpButton{360, 418, 96, 46, "VOL+"};

int16_t* s_samples = nullptr;
MicSource s_source = MicSource::Mic1;
AudioState s_state = AudioState::Idle;
size_t s_recorded_samples = 0;
size_t s_recorded_frames = 0;
size_t s_live_recorded_frames = 0;
size_t s_recording_chunk_frames = 0;
uint16_t s_peak = 0;
const char* s_error = nullptr;
size_t s_peak_scanned_frames = 0;
uint32_t s_last_meter_ms = 0;
uint8_t s_volume = 180;
bool s_recording_chunk_started = false;
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

void drawCenteredLine(const char* text, int16_t y, const lgfx::IFont* font,
                      uint16_t color) {
  const int cx = M5.Display.width() / 2;
  M5.Display.setTextDatum(top_center);
  M5.Display.setFont(font);
  M5.Display.setTextColor(color, TFT_BLACK);
  M5.Display.setTextPadding(M5.Display.width() - 32);
  M5.Display.drawString(text, cx, y);
  M5.Display.setTextPadding(0);
}

void drawMeter() {
  char line[64];
  const size_t displayed_frames =
      s_state == AudioState::Recording ? s_live_recorded_frames : s_recorded_samples;
  const uint32_t duration_ms =
      static_cast<uint32_t>((displayed_frames * 1000ULL) / kSampleRate);
  std::snprintf(line, sizeof(line), "recorded: %lu.%01lu / %lu s  peak:%u",
                static_cast<unsigned long>(duration_ms / 1000),
                static_cast<unsigned long>((duration_ms / 100) % 10),
                static_cast<unsigned long>(kMaxRecordSeconds), s_peak);
  drawCenteredLine(line, 138, &fonts::FreeMonoBold9pt7b, TFT_WHITE);
}

void drawScreen(bool initialize = false) {
  const int cx = M5.Display.width() / 2;
  char line[80];
  M5.Display.startWrite();
  if (initialize) {
    M5.Display.fillScreen(TFT_BLACK);
    drawCenteredLine("CoreP4X Audio Test", 26, &fonts::FreeMonoBold9pt7b, TFT_WHITE);
  }
  std::snprintf(line, sizeof(line), "source: %s   state: %s", sourceName(), stateName());
  drawCenteredLine(line, 78, &fonts::FreeMonoBold9pt7b, TFT_WHITE);
  std::snprintf(line, sizeof(line), "volume: %u", s_volume);
  drawCenteredLine(line, 104, &fonts::FreeMonoBold9pt7b, TFT_WHITE);
  drawMeter();
  std::snprintf(line, sizeof(line), "%lu Hz stereo capture  max %lu s",
                static_cast<unsigned long>(kSampleRate),
                static_cast<unsigned long>(kMaxRecordSeconds));
  drawCenteredLine(line, 170, &fonts::Font2, TFT_CYAN);
  drawCenteredLine(s_state == AudioState::Recording ? "Tap REC STOP when finished"
                                                     : "Record first, then replay",
                   196, &fonts::Font2, TFT_WHITE);
  drawCenteredLine(s_error ? s_error : "", 228, &fonts::Font2, TFT_RED);
  if (s_audio_probe_valid) {
    std::snprintf(line, sizeof(line), "ES8311:%s  AMP:%s  I2S:%s",
                  s_codec_present ? "OK" : "FAIL", s_amp_enabled ? "ON" : "OFF",
                  M5.Speaker.isPlaying() ? "PLAY" : "STOP");
  } else {
    line[0] = '\0';
  }
  drawCenteredLine(line, 248, &fonts::Font2, TFT_CYAN);

  drawButton(kMic1Button, TFT_BLUE, s_source == MicSource::Mic1, s_state != AudioState::Recording && s_state != AudioState::Playing);
  drawButton(kMic2Button, TFT_BLUE, s_source == MicSource::Mic2, s_state != AudioState::Recording && s_state != AudioState::Playing);

  Button record = kRecordButton;
  record.label = s_state == AudioState::Recording ? "REC STOP" : "REC START";
  drawButton(record, s_state == AudioState::Recording ? TFT_RED : TFT_DARKGREEN, false, s_state != AudioState::Playing);

  Button play = kPlayButton;
  play.label = s_state == AudioState::Playing ? "REPLAY STOP" : "REPLAY START";
  drawButton(play, s_state == AudioState::Playing ? TFT_RED : TFT_DARKCYAN, false,
             s_state == AudioState::Ready || s_state == AudioState::Playing);

  drawButton(kVolumeDownButton, TFT_ORANGE, false, true);
  drawButton(kVolumeUpButton, TFT_ORANGE, false, true);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setFont(&fonts::FreeMonoBold9pt7b);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  std::snprintf(line, sizeof(line), "VOL %u", s_volume);
  M5.Display.drawString(line, cx, kVolumeDownButton.y + kVolumeDownButton.h / 2);
  M5.Display.endWrite();
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

bool queueRecordingChunk() {
  if (s_recorded_frames >= kMaxFrameCount) return false;
  s_recording_chunk_frames = kRecordChunkFrames;
  const size_t remaining_frames = kMaxFrameCount - s_recorded_frames;
  if (s_recording_chunk_frames > remaining_frames) s_recording_chunk_frames = remaining_frames;
  s_recording_chunk_started = false;
  const size_t sample_offset = s_recorded_frames * kChannelCount;
  const size_t sample_count = s_recording_chunk_frames * kChannelCount;
  return M5.Mic.record(s_samples + sample_offset, sample_count, kSampleRate, true);
}

void finishRecordingChunk() {
  s_recorded_frames += s_recording_chunk_frames;
  if (s_recorded_frames > kMaxFrameCount) s_recorded_frames = kMaxFrameCount;
  s_live_recorded_frames = s_recorded_frames;
  updatePeak(s_recorded_frames);
  s_recording_chunk_frames = 0;
  s_recording_chunk_started = false;
}

void preparePlaybackBuffer() {
  const size_t channel = s_source == MicSource::Mic1 ? 0 : 1;
  for (size_t i = 0; i < s_recorded_frames; ++i) {
    s_samples[i] = s_samples[i * kChannelCount + channel];
  }
  s_recorded_samples = s_recorded_frames;
  s_live_recorded_frames = s_recorded_frames;
}

void changeVolume(int delta) {
  int next = static_cast<int>(s_volume) + delta;
  if (next < kMinVolume) next = kMinVolume;
  if (next > kMaxVolume) next = kMaxVolume;
  if (next == s_volume) return;
  s_volume = static_cast<uint8_t>(next);
  if (s_state == AudioState::Playing) {
    M5.Speaker.setVolume(s_volume);
  }
  Serial.printf("audio: volume=%u\n", s_volume);
  drawScreen();
}

void selectSource(MicSource source) {
  if (s_source == source) return;
  s_source = source;
  s_recorded_samples = 0;
  s_recorded_frames = 0;
  s_live_recorded_frames = 0;
  s_peak = 0;
  s_peak_scanned_frames = 0;
  s_state = AudioState::Idle;
  s_error = nullptr;
  Serial.printf("audio: source=%s\n", sourceName());
  drawScreen();
}

void startRecording() {
  if (!s_samples || s_state == AudioState::Playing) return;
  s_error = nullptr;
  std::memset(s_samples, 0, kMaxSampleCount * sizeof(*s_samples));
  M5.Speaker.stop();
  M5.Speaker.end();
  configureMicSource();
  if (!M5.Mic.begin()) {
    s_error = "MIC BEGIN FAILED";
    Serial.println("audio: mic begin failed");
    drawScreen();
    return;
  }
  s_recorded_samples = 0;
  s_recorded_frames = 0;
  s_live_recorded_frames = 0;
  s_recording_chunk_frames = 0;
  s_recording_chunk_started = false;
  s_peak = 0;
  s_peak_scanned_frames = 0;
  s_last_meter_ms = millis();
  if (!queueRecordingChunk()) {
    M5.Mic.end();
    s_error = "MIC RECORD FAILED";
    Serial.println("audio: record start failed");
    drawScreen();
    return;
  }
  s_state = AudioState::Recording;
  Serial.printf("audio: record start source=%s\n", sourceName());
  drawScreen();
}

void stopRecording() {
  if (s_state != AudioState::Recording) return;
  const bool chunk_completed = s_recording_chunk_started && !M5.Mic.isRecording();
  M5.Mic.end();
  if (chunk_completed) {
    finishRecordingChunk();
  } else {
    s_recording_chunk_frames = 0;
    s_recording_chunk_started = false;
  }
  preparePlaybackBuffer();
  s_state = s_recorded_samples ? AudioState::Ready : AudioState::Idle;
  Serial.printf("audio: record stop source=%s samples=%lu ms=%lu peak=%u\n", sourceName(),
                static_cast<unsigned long>(s_recorded_samples),
                static_cast<unsigned long>((s_recorded_samples * 1000) / kSampleRate), s_peak);
  drawScreen();
}

void startPlaying() {
  if (!s_samples || !s_recorded_samples || s_state == AudioState::Recording) return;
  s_error = nullptr;
  M5.Mic.end();
  M5.delay(20);
  if (!M5.Speaker.begin()) {
    s_error = "SPEAKER BEGIN FAILED";
    Serial.println("audio: play start failed");
    drawScreen();
    return;
  }
  M5.Speaker.setVolume(s_volume);
  M5.delay(50);
  if (!M5.Speaker.playRaw(s_samples, s_recorded_samples, kSampleRate, false, 1, -1, true)) {
    s_error = "PLAYBACK FAILED";
    Serial.println("audio: playback start failed");
    drawScreen();
    return;
  }
  s_codec_present = M5.In_I2C.scanID(0x18, 100000);
  s_amp_enabled = (M5.In_I2C.readRegister8(0x4F, 0x05, 100000) & (1u << 2)) != 0;
  s_audio_probe_valid = true;
  s_state = AudioState::Playing;
  Serial.printf("audio: playback start source=%s samples=%lu ms=%lu volume=%u\n", sourceName(),
                static_cast<unsigned long>(s_recorded_samples),
                static_cast<unsigned long>((s_recorded_samples * 1000) / kSampleRate), s_volume);
  drawScreen();
}

void stopPlaying() {
  if (s_state != AudioState::Playing) return;
  M5.Speaker.stop();
  M5.Speaker.end();
  s_state = s_recorded_samples ? AudioState::Ready : AudioState::Idle;
  Serial.println("audio: playback stop");
  drawScreen();
}

void handleTouch(int16_t x, int16_t y) {
  if (contains(kMic1Button, x, y) && s_state != AudioState::Recording && s_state != AudioState::Playing) {
    selectSource(MicSource::Mic1);
  } else if (contains(kMic2Button, x, y) && s_state != AudioState::Recording && s_state != AudioState::Playing) {
    selectSource(MicSource::Mic2);
  } else if (contains(kVolumeDownButton, x, y)) {
    changeVolume(-kVolumeStep);
  } else if (contains(kVolumeUpButton, x, y)) {
    changeVolume(kVolumeStep);
  } else if (contains(kRecordButton, x, y)) {
    s_state == AudioState::Recording ? stopRecording() : startRecording();
  } else if (contains(kPlayButton, x, y)) {
    s_state == AudioState::Playing ? stopPlaying() : startPlaying();
  }
}
}

void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);
  auto cfg = M5.config();
  cfg.internal_mic = true;
  cfg.internal_spk = true;
  M5.begin(cfg);
  M5.Display.setRotation(0);
  s_samples = static_cast<int16_t*>(heap_caps_malloc(kMaxSampleCount * sizeof(*s_samples), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!s_samples) s_samples = static_cast<int16_t*>(heap_caps_malloc(kMaxSampleCount * sizeof(*s_samples), MALLOC_CAP_8BIT));
  if (s_samples) std::memset(s_samples, 0, kMaxSampleCount * sizeof(*s_samples));
  else s_error = "BUFFER ALLOC FAILED";
  Serial.printf("CoreP4X audio: board=%d rate=%luHz max=%lus channels=%lu max_samples=%lu\n",
                static_cast<int>(M5.getBoard()), static_cast<unsigned long>(kSampleRate),
                static_cast<unsigned long>(kMaxRecordSeconds),
                static_cast<unsigned long>(kChannelCount),
                static_cast<unsigned long>(kMaxSampleCount));
  drawScreen(true);
}

void loop() {
  M5.update();
  if (s_state == AudioState::Recording) {
    if (M5.Mic.isRecording()) {
      s_recording_chunk_started = true;
    } else if (s_recording_chunk_started) {
      finishRecordingChunk();
      if (s_recorded_frames >= kMaxFrameCount) {
        stopRecording();
      } else if (!queueRecordingChunk()) {
        M5.Mic.end();
        preparePlaybackBuffer();
        s_error = "MIC QUEUE FAILED";
        s_state = s_recorded_samples ? AudioState::Ready : AudioState::Idle;
        Serial.println("audio: record queue failed");
        drawScreen();
      }
    }
  }
  if (s_state == AudioState::Recording && millis() - s_last_meter_ms >= 200) {
    s_last_meter_ms = millis();
    M5.Display.startWrite();
    drawMeter();
    M5.Display.endWrite();
  }
  if (s_state == AudioState::Playing && !M5.Speaker.isPlaying()) stopPlaying();
  if (M5.Touch.getCount() && M5.Touch.getDetail(0).wasPressed()) {
    const auto& touch = M5.Touch.getDetail(0);
    Serial.printf("audio: touch x=%d y=%d\n", touch.x, touch.y);
    handleTouch(touch.x, touch.y);
  }
}
