// Muninn firmware configuration — pin map, audio rates, mix, feature toggles.
// Copy secrets (Wi-Fi creds) into src/secrets.h (git-ignored); see secrets.h.example.
#pragma once

// ── Audio front-end selection ───────────────────────────────────────────────────
//   I2S_LINE  = PCM1808 stereo line-in on the ESP32-S3 (default).
//   A2DP_SINK = Bluetooth A2DP sink on the ORIGINAL ESP32 (Classic BT; captures the B03+ stream).
#define MUNINN_FRONTEND_I2S_LINE  0
#define MUNINN_FRONTEND_A2DP_SINK 1
#ifndef MUNINN_FRONTEND
#define MUNINN_FRONTEND MUNINN_FRONTEND_I2S_LINE
#endif

// A2DP sink (original ESP32): Bluetooth name shown to the B03+, and the SBC sample rate it sends.
#define MUNINN_A2DP_NAME "Muninn"
#define MUNINN_A2DP_SAMPLE_RATE 44100

// ── Audio input: PCM1808 stereo line-in via I2S ─────────────────────────────────
// The device taps line-level audio from your mixer. It is NOT in your monitoring path —
// you keep hearing audio through your own wired headphones / 1Mii B03. Two channels:
//   L (ch0) = program / monitor tap     R (ch1) = your voice / mic bus
#define MUNINN_ADC_SAMPLE_RATE_HZ 48000
#define MUNINN_ADC_CHANNELS       2
#define MUNINN_TAP_SAMPLE_RATE_HZ 16000   // == muninn::proto::SAMPLE_RATE_HZ

// Diarization tap (M3b): send interleaved stereo (program L + voice R) instead of the mono mix, so
// the listener can label "You" vs "Program". Off by default (mono is half the bandwidth).
#ifndef MUNINN_STEREO_TAP
#define MUNINN_STEREO_TAP 0
#endif

// ── I2S pins to the PCM1808 (ESP32-S3 is I2S master) ────────────────────────────
#define PIN_I2S_MCLK 0     // -> PCM1808 SCKI (system clock, 256*fs)
#define PIN_I2S_BCK  5     // -> PCM1808 BCK
#define PIN_I2S_LRCK 6     // -> PCM1808 LRCK
#define PIN_I2S_DIN  7     // <- PCM1808 DOUT

// ── Mix: program + your voice -> mono (Q8 fixed point, 256 = unity, 128 = -6 dB) ──
// Defaults sum both at -6 dB (clip-safe, both present). Raise MUNINN_GAIN_VOICE_Q8 to
// push your own voice up in the transcript. This is the "add in my own voice" control.
#define MUNINN_GAIN_PROGRAM_Q8 128
#define MUNINN_GAIN_VOICE_Q8   128

// ── Capture button (active-low, INPUT_PULLUP) ───────────────────────────────────
#define PIN_CAPTURE_BUTTON 4
#define CAPTURE_DEBOUNCE_MS 40

// ── Status LED (WS2812) ─────────────────────────────────────────────────────────
// GPIO48 is the S3 DevKitC-1 onboard RGB; the original ESP32 has no GPIO48, so use a low GPIO there.
#ifndef PIN_STATUS_LED
#if MUNINN_FRONTEND == MUNINN_FRONTEND_A2DP_SINK
#define PIN_STATUS_LED 2
#else
#define PIN_STATUS_LED 48
#endif
#endif
#define LED_COLOR_IDLE      0x8E00FF   // Kumouri Purple
#define LED_COLOR_CAPTURING 0x00FF0F   // Toxic Green
// Metering: modulate LED brightness with the input peak; flash red on clip. Set to 0 for a steady LED.
#define MUNINN_METER_ENABLE 1
#define MUNINN_METER_FULL_SCALE 6000   // peak that maps to full LED brightness
#define MUNINN_CLIP_THRESHOLD 32000    // |sample| >= this counts as clipping
// Send per-channel level to the listener UI every N ms (M3c). 0 = off.
#ifndef MUNINN_METER_REPORT_MS
#define MUNINN_METER_REPORT_MS 100
#endif

// ── Capture trigger mode ────────────────────────────────────────────────────────
#define MUNINN_CAPTURE_BUTTON 0
#define MUNINN_CAPTURE_VAD    1
#define MUNINN_CAPTURE_BOTH   2
#ifndef MUNINN_CAPTURE_MODE
#define MUNINN_CAPTURE_MODE MUNINN_CAPTURE_BUTTON
#endif
// VAD (voice-activated capture): auto start/stop from the voice channel's level.
#define MUNINN_VAD_THRESHOLD 2500   // peak level considered "voice"
#define MUNINN_VAD_HANG_MS   800    // trailing silence before auto-stop

// ── microSD (SPI, optional store-and-forward) ───────────────────────────────────
#define PIN_SD_CS   10
#define PIN_SD_MOSI 11
#define PIN_SD_SCK  12
#define PIN_SD_MISO 13

// ── Transport selection (USB-CDC needs no radio; Wi-Fi is the wireless option) ──
#define MUNINN_TRANSPORT_USB_CDC 0
#define MUNINN_TRANSPORT_WIFI_TCP 1
#define MUNINN_TRANSPORT_SD 2
#ifndef MUNINN_TRANSPORT
#define MUNINN_TRANSPORT MUNINN_TRANSPORT_USB_CDC
#endif
#define MUNINN_LISTENER_TCP_PORT 5140
// Listener host for the Wi-Fi transport. Override in src/secrets.h (see .example).
#ifndef MUNINN_LISTENER_HOST
#define MUNINN_LISTENER_HOST "192.168.1.50"
#endif
