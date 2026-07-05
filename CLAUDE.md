# CLAUDE.md — Muninn

Guidance for Claude Code (and humans) working in this repo.

## What this project is

**Muninn** is a side-chain transcription tap: an ESP32-S3 device that reads a spare **line-level tap
from a mixer** (program on L, the user's voice on R), mixes them to mono, and streams it — on command
— to a PC service that transcribes it with whisper.cpp and writes timestamped Markdown transcripts.
It is **not** in the monitoring path; the user keeps hearing audio through their own headphones / 1Mii
B03.

Read [`docs/architecture.md`](docs/architecture.md) first, then [`docs/protocol.md`](docs/protocol.md).

## Monorepo layout

- `firmware/` — PlatformIO project, ESP32-S3 (`env:esp32-s3` = v1). C++ on Arduino/ESP-IDF.
  - `src/audio/` — `AudioSource` interface + front-end (`i2s_line_source`, PCM1808 stereo line-in).
  - `lib/muninn_dsp/` — gain mix (`mix_to_mono_q8`) + 48→16 kHz decimation.
  - `src/transport/` — pluggable sinks: `usb_cdc`, `wifi_tcp`, `sd_store`.
  - `src/capture/` — button/trigger state machine.
  - `test/` — Unity tests, run on the host via `env:native`.
- `listener/` — Python package `muninn_listener` (whisper.cpp host). Transports mirror the firmware.
- `tools/fake_device.py` — replays a WAV to the listener so the PC side can be tested with no hardware.
- `hardware/` — BOM, wiring, pinout. `docs/` — architecture, protocol, roadmap.

## Key design rules

- **The wire protocol is authoritative.** Firmware framing and the Python decoder must agree
  byte-for-byte. If you change one, change the other and bump `PROTOCOL_VERSION`.
- **Whisper runs on the PC, never on the ESP32.** The device only captures, downsamples, and forwards.
- **Audio front-ends are swappable** behind `AudioSource`; don't hard-wire line-in assumptions into
  the DSP/transport/capture layers — alternate sources (mic board, USB-audio tap) reuse all of them.
- Muninn is a **side-chain** device — never in the monitoring path. No DAC/speaker/BT-audio relay.
- v1 uses **no Wi-Fi** by default (tap rides the USB-CDC channel).

## Build & test

```bash
# firmware
cd firmware && pio run -e esp32-s3-usb && pio test -e native
# listener
cd listener && ruff check . && pytest
```

## Conventions

- **Git Flow** (see [`CONTRIBUTING.md`](CONTRIBUTING.md)); default branch `develop`; branch features
  off `develop`.
- **Merge commits only**; never merge on red/pending CI.
- **Markdown is canonical** for docs; other formats are rendered artifacts.
- Brand accents: Kumouri Purple `#8e00ff`, Toxic Green `#00ff0f`.

## After finishing a phase of work

Run `/sync-claude-md` to keep this file and any sub-package docs accurate.
