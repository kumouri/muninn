# Muninn listener

PC-side service that receives tapped audio from the Muninn device and transcribes it with
whisper.cpp, writing one Markdown transcript per capture.

## Install

```bash
# A 3.11–3.12 venv is recommended (some whisper wheels lag the newest Python).
python -m venv .venv
. .venv/Scripts/activate        # Windows;  source .venv/bin/activate on macOS/Linux
pip install -e "listener[dev]"  # core is stdlib-only; [dev] adds pytest, ruff, pyserial
```

## Run

```bash
# Wireless PC (TCP) — device connects to this machine
python -m muninn_listener --transport tcp --port 5140 \
    --whisper-bin whisper-cli --model models/ggml-base.en.bin

# USB-connected PC (CDC serial) — v1 default device transport
python -m muninn_listener --transport usb --serial-port COM7 \
    --whisper-bin whisper-cli --model models/ggml-base.en.bin

# Smoke test with no model (mock transcription)
python -m muninn_listener --transport tcp --mock
```

### Trigger capture from the PC (M2)

The device honors `CONTROL` frames from the listener, so you can toggle capture without touching the
button:

```bash
# Press Enter to start/stop (portable, no deps)
python -m muninn_listener --transport tcp --hotkey stdin

# A real global hotkey (needs the keyboard package; may require elevated privileges)
pip install "muninn-listener[hotkey]"
python -m muninn_listener --transport tcp --hotkey global --hotkey-combo ctrl+alt+m
```

Transcripts are written to `./transcripts/<YYYY-MM-DD_HHMMSS>.md`.

## Test without hardware

```bash
# Terminal 1
python -m muninn_listener --transport tcp --port 5140 --mock
# Terminal 2 — replay a WAV as if it were the device
python tools/fake_device.py --transport tcp --port 5140 sample.wav
```

### Diarization — "You" vs "Program" (M3b)

If the device streams stereo (program on L, your voice on R — firmware `MUNINN_STEREO_TAP`, or
`fake_device --stereo`), the listener labels each transcript segment by the louder channel:

```bash
# stereo WAV: left = program, right = your voice
python -m muninn_listener --transport tcp --port 5140 --mock
python tools/fake_device.py --transport tcp --port 5140 --stereo session.wav
```

The transcript is written as a labeled script (`**You:** …` / `**Program:** …`). No diarization model
is used — attribution is purely by input channel.

**Desktop-mic voice (M4b).** When the device only forwards *program* audio (the Bluetooth A2DP-sink
front-end), capture your voice from the desktop mic instead and let the listener merge + diarize:

```bash
pip install -e "listener[dev,mic]"
python -m muninn_listener --transport usb --serial-port COM7 --desktop-mic \
    --program-delay-ms 150   # nudge to align the Bluetooth-delayed program with your live mic
```

### Live level meter (M3c)

Add `--meter` to show a live two-channel VU meter (program + voice) from the device's `TYPE_METER`
frames — handy for setting mixer levels before recording:

```bash
python -m muninn_listener --transport tcp --port 5140 --mock --meter
```

## whisper.cpp setup

The default runner shells out to a whisper.cpp CLI binary. Build it from
[whisper.cpp](https://github.com/ggerganov/whisper.cpp) and download a model:

```bash
# in a whisper.cpp checkout
cmake -B build && cmake --build build --config Release
./models/download-ggml-model.sh base.en    # -> models/ggml-base.en.bin
```

Then point the listener at the binary (`--whisper-bin`) and model (`--model`).
Prefer the in-process path? `pip install "muninn-listener[whisper]"` and pass `--pywhispercpp`.

## Layout

| Module | Role |
|--------|------|
| `protocol.py` | wire-protocol codec (mirrors `docs/protocol.md`) |
| `assembler.py` | frames → capture segments → 16 kHz mono WAV |
| `whisper_runner.py` | whisper.cpp via subprocess / pywhispercpp / mock |
| `writer.py` | Markdown transcript output |
| `pipeline.py` | bytes → frames → segments → transcripts |
| `transports/` | `tcp` (asyncio), `usb_cdc` (pyserial) |
| `hotkey.py` | optional PC-side capture toggle |
