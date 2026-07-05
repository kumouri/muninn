<!-- Muninn — brand: Kumouri Purple #8e00ff / Toxic Green #00ff0f -->
<h1 align="center">🐦‍⬛ Muninn</h1>

<p align="center">
  <em>Odin's raven <strong>"Memory"</strong> — it flies out, hears everything, and returns to record it.</em>
</p>

<p align="center">
  <img alt="platform" src="https://img.shields.io/badge/platform-ESP32--S3-8e00ff?style=flat-square">
  <img alt="input" src="https://img.shields.io/badge/input-line--tap%20%2B%20voice-8e00ff?style=flat-square&labelColor=333">
  <img alt="listener" src="https://img.shields.io/badge/listener-Python%20%2B%20whisper.cpp-00ff0f?style=flat-square&labelColor=333">
  <img alt="branching" src="https://img.shields.io/badge/branching-Git%20Flow-8e00ff?style=flat-square">
  <img alt="license" src="https://img.shields.io/badge/license-MIT-00ff0f?style=flat-square&labelColor=333">
</p>

---

## What it is

**Muninn** taps a **line-level feed from your mixer**, mixes in **your own voice**, and — **on command** —
streams it to a companion PC service that transcribes it with
[whisper.cpp](https://github.com/ggerganov/whisper.cpp) and saves timestamped text.

It sits **off to the side of your signal chain**, not in your monitoring path: you keep hearing audio
through your own wired headphones (or a 1Mii B03 → Bluetooth headphones) exactly as you do now. Muninn
just takes a spare tap and listens.

```
mixer ─┬─▶ your monitoring (wired / 1Mii B03 → BT)      ← unchanged
       │
       ├─ program tap ─▶ [ Muninn: ESP32-S3 + PCM1808 ] ─▶ [ PC listener + whisper.cpp ]
       └─ your voice  ─▶        (mix L+R → 16 kHz mono)          └─▶ transcripts/2026-07-05_142230.md
```

## How it works

| Stage | v1 |
|-------|----|
| **Input** | Stereo **line-in** via a PCM1808 I²S ADC: **L = program/monitor tap**, **R = your voice/mic bus** |
| **Mix** | Sum L+R to mono with per-channel gain — push your own voice up to taste (`config.h`) |
| **Downsample** | 48 kHz → **16 kHz mono** (all Whisper needs) |
| **Transport** | To the PC over **USB-CDC** (default), **Wi-Fi TCP**, or buffered to **SD** |
| **Capture** | A **button** on the device (or a PC hotkey) marks what to transcribe |
| **Transcribe** | The **Python listener** runs whisper.cpp and writes Markdown transcripts |

Optionally stream **stereo** (program + your voice on separate channels) and the listener labels
the transcript **"You" vs "Program"** by input channel — no diarization model needed. Capture can be
triggered by the device **button**, a **PC hotkey**, or **voice activity (VAD)**; the status LED is a
one-pixel VU meter.

The audio front-end is **pluggable** (`firmware/src/audio/AudioSource`) — the wire protocol,
transports, capture logic, and the entire listener are independent of it. See
[`docs/architecture.md`](docs/architecture.md) and [`docs/roadmap.md`](docs/roadmap.md).

## Repository layout

```
firmware/   PlatformIO project for the ESP32-S3 + PCM1808 tap
listener/   Python service that receives the tap and runs whisper.cpp
tools/      fake_device.py — replay a WAV to the listener with no hardware
hardware/   bill of materials, wiring, pinout
docs/        architecture, wire protocol, roadmap
```

## Quick start

### PC listener (no hardware needed)

```bash
# A 3.11–3.12 venv is recommended (whisper wheels lag the newest Python)
python -m venv .venv && . .venv/Scripts/activate   # Windows
pip install -e "listener[dev]"

# Terminal 1 — start the listener (mock model, writes to ./transcripts)
python -m muninn_listener --transport tcp --port 5140 --mock

# Terminal 2 — pretend to be the device: stream a WAV in
python tools/fake_device.py --transport tcp --port 5140 sample.wav
```

A transcript appears under `transcripts/`. Swap `--mock` for a real whisper.cpp binary + model —
see [`listener/README.md`](listener/README.md).

### Firmware

```bash
cd firmware
pio run -e esp32-s3               # build v1
pio run -e esp32-s3 -t upload
pio test -e native                # host-side unit tests (DSP mix/resample + protocol framing)
```

## Documentation

- [Architecture](docs/architecture.md)
- [Wire protocol](docs/protocol.md)
- [Roadmap & milestones](docs/roadmap.md)
- [Bill of materials & wiring](hardware/BOM.md)
- [Contributing & Git Flow](CONTRIBUTING.md)

## License

[MIT](LICENSE) © 2026 Ceryce Armstrong
