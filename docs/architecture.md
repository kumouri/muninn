# Architecture

## Concept

Muninn is a **side-chain transcription tap**. It takes a spare line-level output from a mixer,
mixes in the user's own voice, and — when the user triggers a capture — streams it to a PC service
that transcribes it with whisper.cpp. It is **not** in the monitoring path: the user keeps hearing
audio through their own headphones (wired, or a 1Mii B03 → Bluetooth), untouched.

```
 mixer ─┬─▶ user monitoring (wired HP / 1Mii B03 → BT)                      ← untouched
        │
        ├─ program tap ─┐
        │               ├─▶ PCM1808 stereo line ADC ─I²S─▶ ESP32-S3
        └─ voice bus  ──┘                                    │
                                                             │ mix L+R (gain) → mono
                                                             │ 48k → 16k mono s16le
                                                     capture gate (button / hotkey)
                                                             │
                                              transport ─────┤ USB-CDC (default) │ Wi-Fi TCP │ SD
                                                             ▼
                                       ┌──────── PC listener (Python) ────────┐
                                       │  reassemble frames → WAV segment      │
                                       │  whisper.cpp → text                   │
                                       │  writer → transcripts/<ts>.md         │
                                       └───────────────────────────────────────┘
```

## Why these choices

- **Side-chain tap, not pass-through.** The user already monitors through their own rig, so Muninn
  never has to carry or reconstruct the audio it captures — no DAC, no speaker, no relay. It just
  reads a spare mixer output. This is simpler and higher-fidelity than intercepting the output path.
- **Stereo line-in = program + voice.** A PCM1808 gives two clean line channels. Channel L takes the
  program/monitor tap; channel R takes the user's voice/mic bus off the mixer. Muninn sums them (with
  per-channel gain) so the transcript includes both sides — including the user's own voice, which an
  output-only tap would miss.
- **Whisper on the PC, not the ESP32.** The PC can run any model quickly; the device only mixes,
  downsamples, and forwards.
- **16 kHz mono.** Whisper works at 16 kHz mono; downsampling on-device also cuts the link bandwidth.
- **ESP32-S3.** Native USB (so the tap data rides a plain USB-CDC serial link with no radio),
  8 MB PSRAM for buffering, and Wi-Fi when a wireless link is wanted.

## Layers (firmware)

| Layer | Interface | v1 implementation | Notes |
|-------|-----------|-------------------|-------|
| Audio source | `AudioSource` | `i2s_line_source` | reads PCM1808 stereo line-in, fires the tap |
| DSP | `mix_to_mono_q8` + `downsample_48k_to_16k` | gain mix then decimate | pure, unit-tested on host |
| Capture | `CaptureTrigger` | button state machine | toggles the `capturing` flag in frames |
| Transport | `Transport` (sink) | `usb_cdc` | `wifi_tcp`, `sd_store` also provided |

Because the front-end sits behind `AudioSource`, a different capture source (a mic board, a USB-audio
tap, etc.) can be added later without touching DSP, capture, transport, or the listener.

## Layers (listener)

- `transports/` — `tcp` (asyncio) and `usb_cdc` (pyserial) receivers; decode the [wire
  protocol](protocol.md) into `(seq, timestamp, capturing, pcm)` frames.
- `whisper_runner.py` — feeds a completed capture segment (16 kHz mono WAV) to whisper.cpp
  (`subprocess` by default, `pywhispercpp` optional, `MockWhisper` for tests).
- `writer.py` — writes `transcripts/<YYYY-MM-DD_HHMMSS>.md` with a header and the text.
- `hotkey.py` — optional PC-side capture toggle.

## Data flow of a capture

1. User presses the button → firmware sets `capturing=1` on outgoing frames and sends
   `CAPTURE_START`.
2. Listener accumulates the mixed 16 kHz mono PCM for the segment.
3. User presses again → `CAPTURE_STOP`; listener finalizes the segment.
4. `whisper_runner` transcribes; `writer` creates the transcript file.
5. Listener may send a `TEXT_ACK` back (device could flash the LED green).

## Three "save" targets, one listener

The original three destinations are just **transports to the same listener**:

- **USB-connected PC** → `usb_cdc` (v1 default; the S3 is already plugged in).
- **Wireless PC** → `wifi_tcp`.
- **On-board** → `sd_store` buffers 16 kHz WAV segments for later upload (store-and-forward).

Text is always produced by the PC listener.
