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

## M4 — Bluetooth A2DP-sink variant (different chip)

For tapping the **B03+ over Bluetooth** (the full mixer mix, no ADC), the audio front-end is a silent
A2DP sink — which requires **Classic Bluetooth**, so it uses the **original ESP32** (not the S3):

| # | Part | Notes |
|---|------|-------|
| 1 | **Original ESP32** dev board (ESP32-WROOM-32 / DevKitC / WROVER) | Classic BT for A2DP sink; the S3 is BLE-only |
| 1 | WS2812 / addressable RGB LED | Status/meter (GPIO2 default on this board) |
| 1 | Momentary push button | Capture trigger (GPIO4) |
| 1 | USB cable | UART-bridge serial to the desktop (data out) |

No PCM1808, no DAC, no jacks — audio comes in over Bluetooth and out over USB. Voice is added on the
desktop (webcam mic). Build with `pio run -e esp32`.

## Enclosure

A parametric 3D-printable case (with panel cutouts for USB-C, the two jacks, the button, and the LED
light-pipe) lives in [`ENCLOSURE.md`](ENCLOSURE.md) + [`enclosure/muninn_case.scad`](enclosure/muninn_case.scad).

## Not needed for this design

No DAC, speaker, or Bluetooth audio hardware — Muninn is a side-chain tap and never carries your
monitoring audio. Bluetooth monitoring stays on your existing gear (e.g. the 1Mii B03).
