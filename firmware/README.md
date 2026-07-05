# Muninn firmware

PlatformIO project for the Muninn line-tap device.

## Environments

| Env | Target | Purpose |
|-----|--------|---------|
| `esp32-s3` | ESP32-S3-DevKitC-1 | **v1** — PCM1808 stereo line-in tap + voice mix (default) |
| `native` | host | Unit tests for the portable `lib/` modules |

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
- `lib/muninn_dsp/` — `mix_to_mono_q8` (program + voice, per-channel gain) + 48→16 kHz decimation.
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
