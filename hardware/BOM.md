# Bill of Materials & Wiring

## v1 — Mixer line-tap (ESP32-S3 + PCM1808)

| # | Part | Notes |
|---|------|-------|
| 1 | **ESP32-S3-DevKitC-1** (N16R8 — 16 MB flash / 8 MB PSRAM) | Native USB port for CDC transport |
| 1 | **PCM1808** stereo line-in I²S ADC breakout | L = program tap, R = your voice/mic bus |
| 1–2 | 3.5 mm TRS / RCA input jacks | Match your mixer's spare output(s) |
| 1 | Momentary push button | Capture start/stop trigger |
| 1 | WS2812 / addressable RGB LED | Idle = purple `#8e00ff`, capturing = green `#00ff0f` |
| 1 | microSD SPI module *(optional)* | Store-and-forward buffer (M2) |
| 1 | USB-C cable | Connects the device to the PC (tap data over CDC) |

### Wiring (defaults — see `firmware/src/config.h`)

```
PCM1808 (I²S ADC)          ESP32-S3
  SCKI (system clock) ────  GPIO 0   (MCLK, 256·fs)
  BCK  ───────────────────  GPIO 5
  LRCK ───────────────────  GPIO 6
  DOUT ───────────────────  GPIO 7   (ESP32 I²S data-in)
  FMT/MD0/MD1 ── per PCM1808 datasheet for slave I²S, 24-bit
  VCC ── 3V3     GND ── GND

Line inputs:  LINL ◀── program/monitor tap     LINR ◀── your voice/mic bus (line level)

Capture button ──────────── GPIO 4 ──▶ GND   (INPUT_PULLUP, active-low)
Status LED (WS2812 DIN) ──── GPIO 48         (onboard RGB on many S3 DevKitC-1)

microSD (SPI, optional)     ESP32-S3
  CS ── GPIO 10   MOSI ── GPIO 11   SCK ── GPIO 12   MISO ── GPIO 13
```

### Adding your own voice

Muninn sums the two line channels to mono (with per-channel gain in `config.h`). Two ways to get your
voice in:

1. **Two taps (recommended):** program → `LINL`, your mic bus (post-preamp, line level) → `LINR`.
   Raise `MUNINN_GAIN_VOICE_Q8` to emphasize yourself.
2. **One pre-mixed bus:** send an aux/monitor mix that already includes your mic to `LINL`, and set
   `MUNINN_GAIN_VOICE_Q8` to `0`.

> Feed **line level** into the PCM1808, not a raw microphone — your mixer's preamp does the mic gain.

## Alternate boards

Own an audio-codec board (ESP32-A1S AudioKit, LyraT, ESP32-S3-Korvo)? Those integrate a codec
(ES8388/ES8311) with line-in + mic-in, so the PCM1808 isn't needed — the `i2s_line_source` front-end
would target the codec's driver instead. Note which board you have and the pinout adapts.

## Not needed for this design

No DAC, speaker, or Bluetooth audio hardware — Muninn is a side-chain tap and never carries your
monitoring audio. Bluetooth monitoring stays on your existing gear (e.g. the 1Mii B03).
