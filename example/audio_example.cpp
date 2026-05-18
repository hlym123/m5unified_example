#include "M5Unified.h"
#include <cstring>
#include <esp_heap_caps.h>

// BtnA: record 5 s mono, then play once (mic / speaker mutually exclusive).
// 44100 Hz matches StickS3 / StopWatch internal speaker default in M5Unified.
static constexpr uint32_t K_SAMPLE_RATE = 44100;
static constexpr uint32_t K_RECORD_SECONDS = 5;
static constexpr size_t K_SAMPLE_COUNT = K_SAMPLE_RATE * K_RECORD_SECONDS;

static int16_t *s_rec_buf = nullptr;
static bool s_busy = false;

static void *allocSamples(size_t n) {
    const size_t bytes = n * sizeof(int16_t);
    void *p = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) {
        p = heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
    }
    return p;
}

void setup(void) {
    M5.begin();
    printf("board=%d\n", (int)M5.getBoard());

    M5.Lcd.setRotation(0);
    M5.Lcd.setTextDatum(middle_center);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setFont(&fonts::FreeMonoBold9pt7b);

    s_rec_buf = (int16_t *)allocSamples(K_SAMPLE_COUNT);
    if (!s_rec_buf) {
        M5.Lcd.drawString("malloc failed", M5.Lcd.width() / 2, M5.Lcd.height() / 2);
        return;
    }
    memset(s_rec_buf, 0, K_SAMPLE_COUNT * sizeof(int16_t));

    // Do not call Speaker.end() at boot: it always runs the power-off callback and
    // can leave ES8311 / IOE1 audio path off in a state the mic callback does not fully undo.
    M5.Mic.begin();

    const int cx = M5.Lcd.width() / 2;
    M5.Lcd.drawString("BtnA: 5s REC", cx, 40);
    M5.Lcd.drawString("then PLAY", cx, 70);
    printf("board=%d  samples=%u\n", (int)M5.getBoard(), (unsigned)K_SAMPLE_COUNT);
}

void loop(void) {
    M5.update();

    if (!s_rec_buf || s_busy) {
        return;
    }

    if (!M5.BtnA.wasClicked()) {
        return;
    }

    s_busy = true;
    const int cx = M5.Lcd.width() / 2;

    while (M5.Mic.isRecording()) {
        M5.update();
        M5.delay(1);
    }

    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextColor(YELLOW);
    M5.Lcd.drawString("Recording...", cx, M5.Lcd.height() / 2);
    M5.Lcd.display();

    M5.Speaker.end();
    M5.delay(20);
    M5.Mic.begin();

    if (!M5.Mic.record(s_rec_buf, K_SAMPLE_COUNT, K_SAMPLE_RATE)) {
        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.setTextColor(RED);
        M5.Lcd.drawString("record() failed", cx, M5.Lcd.height() / 2);
        M5.Lcd.display();
        s_busy = false;
        return;
    }

    {
        uint32_t t0 = millis();
        while (M5.Mic.isRecording() == 0 && millis() - t0 < 2000) {
            M5.update();
            M5.delay(1);
        }
        if (M5.Mic.isRecording() == 0) {
            M5.Lcd.fillScreen(BLACK);
            M5.Lcd.setTextColor(RED);
            M5.Lcd.drawString("mic start timeout", cx, M5.Lcd.height() / 2);
            M5.Lcd.display();
            s_busy = false;
            return;
        }
    }

    while (M5.Mic.isRecording()) {
        M5.update();
        M5.delay(1);
    }

    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextColor(GREEN);
    M5.Lcd.drawString("Playing...", cx, M5.Lcd.height() / 2);
    M5.Lcd.display();

    M5.Mic.end();
    M5.delay(20);
    M5.Speaker.begin();
    M5.Speaker.setVolume(220);
    M5.delay(50);
    M5.Speaker.playRaw(s_rec_buf, K_SAMPLE_COUNT, K_SAMPLE_RATE, false, 1, -1);
    while (M5.Speaker.isPlaying()) {
        M5.update();
        M5.delay(1);
    }
    M5.Speaker.end();
    M5.Mic.begin();

    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.drawString("BtnA: 5s REC", cx, 40);
    M5.Lcd.drawString("then PLAY", cx, 70);

    s_busy = false;
}
