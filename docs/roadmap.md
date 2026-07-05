# Roadmap

Muninn is built around a **swappable audio front-end** (`AudioSource`). The DSP, capture, transport,
wire protocol, and the entire PC listener are shared — only the capture source changes.

## M0 — Scaffolding ✅

Git Flow monorepo, docs, wire protocol, PlatformIO firmware skeleton, Python listener, `fake_device`
loopback harness, CI. The listener + `fake_device.py` produce a real transcript with **no hardware**.

## M1 — Mixer line-tap + voice mix (v1)

**Hardware:** ESP32-S3 + PCM1808 stereo line-in ADC + button.

- Read a stereo line tap: **L = program/monitor**, **R = your voice/mic bus** off the mixer.
- Mix to mono with per-channel gain (`config.h`) — push your own voice up to taste.
- Downsample to 16 kHz mono; send over **USB-CDC** to the listener; transcribe on button press.

Exit criteria: feed a mixer tap + voice, press capture, get a correct transcript that includes your
own voice. You keep monitoring through your own headphones / 1Mii B03 — Muninn never touches it.

## M2 — Wireless transport + on-board buffer + PC trigger

- **Wi-Fi TCP** transport so the listener PC needn't be the USB host.
- **SD store-and-forward**: buffer 16 kHz WAV segments on-device, upload when a listener is reachable.
- PC-side **global hotkey** to trigger capture without reaching for the device.

## M3 — Capture quality & ergonomics

- Per-channel level metering / clip indication on the status LED.
- Optional **speaker diarization** in the listener (whisper.cpp doesn't diarize; would add a
  diarization pass) so "you" vs "program" are labelled in the transcript.
- Voice-activated capture (VAD) as an alternative trigger; enclosure.

## M4 — Alternate front-ends (optional)

Thanks to `AudioSource`, other capture sources can be added without touching the pipeline:

- **On-board mic** board (I²S MEMS) for a standalone room recorder.
- **USB-audio tap** for a purely digital capture from a computer.

## Notes

- Muninn is a **side-chain** device — it is never in your monitoring path. Bluetooth monitoring is
  handled by your existing gear (e.g. the 1Mii B03), so there is no Bluetooth audio relay in scope.
- The chip is the **ESP32-S3** throughout (native USB + Wi-Fi + PSRAM). No original-ESP32 Classic-BT
  path is needed for this design.
