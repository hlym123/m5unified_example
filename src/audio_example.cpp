// #include "M5Unified.h"
// #include <esp_heap_caps.h>

// // BtnPWR: record 5 s mono, then play once (mic / speaker mutually exclusive).

// static constexpr uint32_t kSampleRate = 16000;
// static constexpr uint32_t kRecordSeconds = 5;
// static constexpr size_t kSampleCount = kSampleRate * kRecordSeconds;

// static int16_t *s_rec_buf = nullptr;
// static bool s_busy = false;

// static void *allocSamples(size_t n) {
//     const size_t bytes = n * sizeof(int16_t);
//     void *p = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
//     if (!p) {
//         p = heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
//     }
//     return p;
// }

// void setup(void) {
//     M5.begin();
//     printf("board=%d\n", (int)M5.getBoard());

//     M5.Lcd.setRotation(1);
//     M5.Lcd.setTextDatum(middle_center);
//     M5.Lcd.setTextColor(WHITE);
//     M5.Lcd.setFont(&fonts::FreeMonoBold9pt7b);

//     s_rec_buf = (int16_t *)allocSamples(kSampleCount);
//     if (!s_rec_buf) {
//         M5.Lcd.drawString("malloc failed", M5.Lcd.width() / 2, M5.Lcd.height() / 2);
//         return;
//     }
//     memset(s_rec_buf, 0, kSampleCount * sizeof(int16_t));

//     M5.Speaker.setVolume(200);
//     M5.Speaker.end();
//     M5.Mic.begin();

//     const int cx = M5.Lcd.width() / 2;
//     M5.Lcd.drawString("BtnPWR: 5s REC", cx, 40);
//     M5.Lcd.drawString("then PLAY", cx, 70);
//     printf("board=%d  samples=%u\n", (int)M5.getBoard(), (unsigned)kSampleCount);
// }

// void loop(void) {
//     M5.update();

//     if (!s_rec_buf || s_busy) {
//         return;
//     }

//     if (!M5.BtnPWR.wasClicked()) {
//         return;
//     }

//     s_busy = true;
//     const int cx = M5.Lcd.width() / 2;

//     while (M5.Mic.isRecording()) {
//         M5.update();
//         M5.delay(1);
//     }

//     M5.Lcd.fillScreen(BLACK);
//     M5.Lcd.setTextColor(YELLOW);
//     M5.Lcd.drawString("Recording...", cx, M5.Lcd.height() / 2);
//     M5.Lcd.display();

//     if (!M5.Mic.record(s_rec_buf, kSampleCount, kSampleRate)) {
//         M5.Lcd.fillScreen(BLACK);
//         M5.Lcd.setTextColor(RED);
//         M5.Lcd.drawString("record() failed", cx, M5.Lcd.height() / 2);
//         M5.Lcd.display();
//         s_busy = false;
//         return;
//     }

//     // isRecording() stays 0 briefly until mic_task picks up the buffer; if we
//     // only "while (isRecording())" we exit immediately and skip the real wait.
//     {
//         uint32_t t0 = millis();
//         while (M5.Mic.isRecording() == 0 && millis() - t0 < 2000) {
//             M5.update();
//             M5.delay(1);
//         }
//         if (M5.Mic.isRecording() == 0) {
//             M5.Lcd.fillScreen(BLACK);
//             M5.Lcd.setTextColor(RED);
//             M5.Lcd.drawString("mic start timeout", cx, M5.Lcd.height() / 2);
//             M5.Lcd.display();
//             s_busy = false;
//             return;
//         }
//     }

//     while (M5.Mic.isRecording()) {
//         M5.update();
//         M5.delay(1);
//     }

//     M5.Lcd.fillScreen(BLACK);
//     M5.Lcd.setTextColor(GREEN);
//     M5.Lcd.drawString("Playing...", cx, M5.Lcd.height() / 2);
//     M5.Lcd.display();

//     M5.Mic.end();
//     M5.Speaker.begin();
//     M5.Speaker.playRaw(s_rec_buf, kSampleCount, kSampleRate, false, 1, 0);
//     while (M5.Speaker.isPlaying()) {
//         M5.update();
//         M5.delay(1);
//     }
//     M5.Speaker.end();
//     M5.Mic.begin();

//     M5.Lcd.fillScreen(BLACK);
//     M5.Lcd.setTextColor(WHITE);
//     M5.Lcd.drawString("BtnPWR: 5s REC", cx, 40);
//     M5.Lcd.drawString("then PLAY", cx, 70);

//     s_busy = false;
// }
