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

## M2 — Wireless transport + on-board buffer + PC trigger ✅ (code; pending hardware bring-up)

- **Wi-Fi TCP** transport (`env:esp32-s3-wifi`) so the listener PC needn't be the USB host, with
  throttled reconnect.
- **SD store-and-forward** (`StoreAndForwardTransport` + `SdStore`): when the primary link is down,
  frames are buffered to the SD card and replayed in order once it recovers.
- PC-side **capture trigger** (`--hotkey stdin|global`): the listener sends `CONTROL` frames back to
  the device, which honors inbound `CAPTURE_START`/`STOP` (`handleInbound`) — capture without
  reaching for the button.

Transport is chosen at build time via `MUNINN_TRANSPORT` in `config.h`. All three transports and the
remote-control path build in CI; on-hardware verification waits on the PCM1808.

## M3a — Metering + VAD ✅ (code; pending hardware bring-up)

- **Per-channel level metering + clip** (`dsp::measure_levels`): the status LED is a one-pixel VU
  meter — brightness follows the input peak, and it flashes red on clip. `MUNINN_METER_ENABLE`.
- **Voice-activated capture** (`muninn_vad::Vad`): an energy gate with hangover auto-starts/stops
  capture from the voice channel. Selectable via `MUNINN_CAPTURE_MODE` (button / VAD / both).

## M3b — Channel-based diarization ✅ (code; pending hardware bring-up)

Muninn has the two speakers on separate input channels (program vs your voice), so it labels the
transcript **without a diarization model**: attribute each whisper segment to the channel that was
louder during it ("You" / "Program" / "Both").

- Protocol `FLAG_STEREO` + firmware `MUNINN_STEREO_TAP` (`env:esp32-s3-stereo`) stream interleaved
  program (L) + voice (R) at 16 kHz (`dsp::downsample_48k_to_16k_stereo`).
- Listener keeps L/R per capture, downmixes for whisper, and `diarize.label_segments` attributes
  each timestamped segment; the transcript is written as a labeled script.
- `fake_device --stereo` streams a stereo WAV, so the whole path is tested with no hardware.

A model-based pass (e.g. pyannote) remains a future option for single-channel sources.

## M3c — Meter UI + enclosure ✅ (code; pending hardware bring-up)

- **Per-channel meter to the listener UI**: the device sends `TYPE_METER` frames periodically
  (`MUNINN_METER_REPORT_MS`) with program/voice peaks + clip; `muninn_listener --meter` shows a live
  two-channel terminal VU meter so you can set levels before recording.
- **Enclosure**: a parametric two-part OpenSCAD case with panel cutouts (USB-C, 2× 3.5 mm jacks,
  button, LED pipe) — `hardware/enclosure/muninn_case.scad` + [`hardware/ENCLOSURE.md`](../hardware/ENCLOSURE.md).

## M4 — Alternate front-ends (optional)

Thanks to `AudioSource`, other capture sources can be added without touching the pipeline:

- **On-board mic** board (I²S MEMS) for a standalone room recorder.
- **USB-audio tap** for a purely digital capture from a computer.

## Notes

- Muninn is a **side-chain** device — it is never in your monitoring path. Bluetooth monitoring is
  handled by your existing gear (e.g. the 1Mii B03), so there is no Bluetooth audio relay in scope.
- The chip is the **ESP32-S3** throughout (native USB + Wi-Fi + PSRAM). No original-ESP32 Classic-BT
  path is needed for this design.
