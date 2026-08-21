#include <algorithm>
#include <cstdint>
#include <cstdlib>

#include <M5Unified.h>

constexpr uint16_t kColors[] = {
  TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE, TFT_BLACK,
};
constexpr const char* kColorNames[] = {
  "RED", "GREEN", "BLUE", "WHITE", "BLACK",
};

size_t color_index;
uint32_t next_color_ms;
uint32_t next_log_ms;
uint32_t next_mic_ms;
uint32_t next_audio_ms;
bool touch_was_pressed;
int32_t last_touch_x = -1;
int32_t last_touch_y = -1;

const char* board_name(m5::board_t board)
{
  return board == m5::board_t::board_M5CoreP4X ? "M5Stack CoreP4X" : "Unknown";
}

void dump_ioe(const char* stage)
{
  static constexpr uint8_t kRegisters[] = {
    0x03, 0x04, 0x05, 0x06, 0x13, 0x14, 0x1B, 0x1C, 0x25, 0x26,
  };
  Serial.printf("[M5IOE1] stage=%s", stage);
  for (const auto reg : kRegisters) {
    const uint8_t value = M5.In_I2C.readRegister8(0x4F, reg, 100000);
    Serial.printf(" reg%02x=%02x", reg, value);
  }
  Serial.printf("\n");
}

void dump_pm1(const char* stage)
{
  const uint8_t power_config = M5.In_I2C.readRegister8(0x6E, 0x06, 100000);
  const uint8_t vout_low = M5.In_I2C.readRegister8(0x6E, 0x26, 100000);
  const uint8_t vout_high = M5.In_I2C.readRegister8(0x6E, 0x27, 100000);
  const uint16_t vout_mv = vout_low | (static_cast<uint16_t>(vout_high) << 8);
  Serial.printf("[M5PM1] stage=%s pwr_cfg=%02x boost=%s vout=%umV\n",
              stage, power_config,
              (power_config & (1u << 3)) ? "ON" : "OFF", vout_mv);
}

void draw_color()
{
  const uint16_t bg = kColors[color_index];
  const uint16_t fg = (bg == TFT_WHITE || bg == TFT_GREEN) ? TFT_BLACK : TFT_WHITE;
  M5.Display.fillScreen(bg);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(fg, bg);
  M5.Display.setTextSize(2);
  M5.Display.drawString(kColorNames[color_index], M5.Display.width() / 2,
                        M5.Display.height() / 2 - 20);
  M5.Display.setTextSize(1);
  M5.Display.drawString("M5Stack CoreP4X validation", M5.Display.width() / 2,
                        M5.Display.height() / 2 + 16);
}

void scan_i2c()
{
  Serial.printf("[I2C] scan internal bus\n");
  bool found[0x78] = {};
  M5.In_I2C.scanID(found);
  for (uint8_t addr = 8; addr < 0x78; ++addr) {
    if (found[addr]) {
      Serial.printf("[I2C] 0x%02x\n", addr);
    }
  }
}

bool wait_for_recording()
{
  const uint32_t start_ms = m5gfx::millis();
  while (M5.Mic.isRecording() == 0 && m5gfx::millis() - start_ms < 2000) {
    M5.delay(1);
  }
  if (M5.Mic.isRecording() == 0) {
    return false;
  }
  while (M5.Mic.isRecording()) {
    M5.delay(1);
  }
  return true;
}

void capture_mic(const char* stage)
{
  const bool mic_ok = M5.Mic.begin();
  if (!mic_ok) {
    Serial.printf("[MIC] stage=%s begin=FAIL\n", stage);
    return;
  }

  static int16_t samples[9600];
  std::fill(samples, samples + 9600, 0);
  const bool queued = M5.Mic.record(samples, 9600, 24000, true);
  const bool completed = queued && wait_for_recording();

  int peak_left = 0;
  int peak_right = 0;
  if (completed) {
    for (size_t i = 0; i < 9600; i += 2) {
      peak_left = std::max(peak_left, std::abs(static_cast<int>(samples[i])));
      peak_right = std::max(peak_right, std::abs(static_cast<int>(samples[i + 1])));
    }
  }
  Serial.printf("[MIC] stage=%s record=%s left=%d right=%d\n",
              stage, completed ? "OK" : "FAIL", peak_left, peak_right);
  M5.Mic.end();
}

void test_audio()
{
  M5.Mic.end();
  const bool speaker_ok = M5.Speaker.begin();
  Serial.printf("[SPEAKER] begin=%s\n", speaker_ok ? "OK" : "FAIL");
  if (speaker_ok) {
    M5.Speaker.setVolume(96);
    const bool tone_ok = M5.Speaker.tone(1000, 350);
    Serial.printf("[SPEAKER] tone=%s\n", tone_ok ? "OK" : "FAIL");
    M5.delay(450);
    M5.Speaker.stop();
    M5.Speaker.end();
  }

  capture_mic("startup");
  dump_ioe("audio_done");
  scan_i2c();
}

void setup()
{
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);
  auto cfg = M5.config();
  cfg.internal_spk = true;
  cfg.internal_mic = true;
  M5.begin(cfg);

  const auto board = M5.getBoard();
  const auto display_board = M5.Display.getBoard();
  const bool board_ok = board == m5::board_t::board_M5CoreP4X
                     && display_board == m5::board_t::board_M5CoreP4X;
  Serial.printf("[BOARD] name=%s id=%d display_id=%d expected=%d result=%s\n",
              board_name(board), static_cast<int>(board),
              static_cast<int>(display_board),
              static_cast<int>(m5::board_t::board_M5CoreP4X),
              board_ok ? "OK" : "FAIL");
  Serial.printf("[DISPLAY] %dx%d\n", static_cast<int>(M5.Display.width()),
              static_cast<int>(M5.Display.height()));

  dump_pm1("begin");
  dump_ioe("begin");
  M5.Display.setBrightness(8);
  dump_ioe("brightness48");
  draw_color();
  scan_i2c();

  Serial.printf("[IMU] begin=%s\n", M5.Imu.begin() ? "OK" : "FAIL");
  test_audio();

  next_color_ms = m5gfx::millis() + 2000;
  next_log_ms = m5gfx::millis() + 1000;
  next_mic_ms = m5gfx::millis() + 3000;
  next_audio_ms = m5gfx::millis() + 10000;
}

void loop()
{
  M5.update();
  const uint32_t now = m5gfx::millis();

  if (now >= next_color_ms) {
    next_color_ms = now + 2000;
    color_index = (color_index + 1) % (sizeof(kColors) / sizeof(kColors[0]));
    draw_color();
  }

  const auto touch = M5.Touch.getDetail();
  if (touch.isPressed()) {
    M5.Display.fillCircle(touch.x, touch.y, 4, TFT_CYAN);
    if (!touch_was_pressed || std::abs(touch.x - last_touch_x) >= 3
                           || std::abs(touch.y - last_touch_y) >= 3) {
      Serial.printf("[TOUCH] state=pressed x=%d y=%d\n", touch.x, touch.y);
      last_touch_x = touch.x;
      last_touch_y = touch.y;
    }
  } else if (touch_was_pressed) {
    Serial.printf("[TOUCH] state=released x=%ld y=%ld\n",
                static_cast<long>(last_touch_x), static_cast<long>(last_touch_y));
  }
  touch_was_pressed = touch.isPressed();

  if (now >= next_log_ms) {
    next_log_ms = now + 1000;
    float ax = 0;
    float ay = 0;
    float az = 0;
    if (M5.Imu.getAccel(&ax, &ay, &az)) {
      Serial.printf("[IMU] accel x=%.3f y=%.3f z=%.3f\n", ax, ay, az);
    }
  }

  if (now >= next_mic_ms) {
    next_mic_ms = now + 3000;
    capture_mic("periodic");
  }

  if (now >= next_audio_ms) {
    next_audio_ms = now + 10000;
    test_audio();
  }

  M5.delay(10);
}
