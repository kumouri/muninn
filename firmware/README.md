# Muninn firmware

PlatformIO project for the Muninn line-tap device.

## Environments

| Env | Target | Purpose |
|-----|--------|---------|
| `esp32-s3` | ESP32-S3-DevKitC-1 | **v1** — PCM1808 line-in tap + voice mix, USB-CDC (default) |
| `esp32-s3-wifi` | ESP32-S3-DevKitC-1 | M2 — same, Wi-Fi TCP + SD store-and-forward |
| `esp32-s3-stereo` | ESP32-S3-DevKitC-1 | M3b — stereo (program/voice) diarization tap |
| `esp32` | **original ESP32** | M4 — Bluetooth **A2DP sink** capturing the B03+ mixer mix |
| `native` | host | Unit tests for the portable `lib/` modules |

The `esp32` env is a **different chip** (original ESP32) — A2DP sink needs Classic Bluetooth, which
the S3 lacks. `MUNINN_FRONTEND` in `config.h` selects the audio front-end (`I2S_LINE` vs `A2DP_SINK`).

Transport is selected at build time via `MUNINN_TRANSPORT` in `src/config.h` (USB-CDC / Wi-Fi TCP /
SD). The `esp32-s3-wifi` env sets it to Wi-Fi; Wi-Fi credentials + listener host go in `src/secrets.h`
(see the `.example`). A PC hotkey can drive capture remotely — the device honors inbound `CONTROL`
frames (`handleInbound` in `src/main.cpp`).

Capture trigger is selectable via `MUNINN_CAPTURE_MODE` (button / VAD / both). In VAD mode the device
auto-captures from the voice channel's level (`MUNINN_VAD_THRESHOLD` / `MUNINN_VAD_HANG_MS`). The
status LED doubles as a one-pixel VU meter (`MUNINN_METER_ENABLE`), flashing red on clip.

Set `MUNINN_STEREO_TAP=1` (`env:esp32-s3-stereo`) to stream program (L) + voice (R) separately so the
listener can label "You" vs "Program"; the default mixes them to mono (half the bandwidth).

The device also sends per-channel `METER` frames every `MUNINN_METER_REPORT_MS` (0 = off); run the
listener with `--meter` for a live VU meter to set levels before recording.

## Build & test

```bash
pio run -e esp32-s3                # build v1
pio run -e esp32-s3 -t upload      # flash (plug into the S3's native USB port)
pio device monitor                 # serial

pio test -e native                 # host unit tests (DSP mix/resample + protocol framing)
```

> Building the device env downloads the Espressif toolchain on first run (`pio pkg install`).
> The `native` env needs a host C++ compiler (gcc/clang/MSVC).

## Structure

- `lib/muninn_proto/` — wire-protocol codec (portable, matches [`docs/protocol.md`](../docs/protocol.md)).
- `lib/muninn_dsp/` — `mix_to_mono_q8` (program + voice, per-channel gain), 48→16 kHz decimation,
  and `measure_levels` (per-channel peak + clip, for the LED meter).
- `lib/muninn_vad/` — `Vad`: energy gate with hangover for voice-activated capture.
- `src/audio/` — `AudioSource` front-end: `i2s_line_source` (PCM1808 stereo line-in).
- `src/transport/` — `usb_cdc` (v1), `wifi_tcp`, `sd_store`.
- `src/capture/` — debounced button trigger.
- `src/config.h` — pins, rates, **mix gains**, transport selection. Wi-Fi creds go in `src/secrets.h`
  (see the `.example`).
- `test/` — Unity suites (`test_proto`, `test_dsp`).

## Adding your voice

Feed the program tap to the PCM1808's left line-in and your (line-level) voice bus to the right.
`onTap()` in `src/main.cpp` mixes them with `MUNINN_GAIN_PROGRAM_Q8` / `MUNINN_GAIN_VOICE_Q8` from
`config.h` — raise the voice gain to emphasize yourself. See [`hardware/BOM.md`](../hardware/BOM.md).
