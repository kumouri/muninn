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

## M4 — Bluetooth A2DP-sink capture (Ceryce's real setup)

Ceryce taps a **work laptop** (no software can be installed on it) and transcribes on a **desktop**.
Her mixer sums both computers to one analog output → splitter → wired headphones ‖ 1Mii B03+. So the
capture point is the **B03+'s Bluetooth transmit**: Muninn pairs as a silent A2DP **sink**, receives
the full mixer mix (already digital), and forwards it to the desktop — no ADC, no analog round-trip.
(Windows can't natively act as an A2DP sink, so this bridge does a job the desktop can't do alone.)

- **M4a — firmware** ✅ (code; pending hardware): `a2dp_sink_source` on the **original ESP32**
  (`env:esp32`; the S3 has no Classic BT). Silent sink (no DAC — you monitor on wired headphones);
  SBC→PCM → `dsp::resample_linear_mono` to 16 kHz → USB-CDC to the desktop. New chip, same protocol/
  transport/listener.
- **M4b — listener**: capture the **desktop webcam mic** as the voice channel, align it with the
  incoming program stream, and reuse the You/Program diarization + writer.

## M5 — Other front-ends (optional, later)

Thanks to `AudioSource`: on-board I²S mic (standalone recorder), USB-audio tap, model-based
diarization for single-channel sources.

## Chip note

The **S3 line-in path** (M1–M3) and the **original-ESP32 A2DP path** (M4) are different chips: the S3
has native USB + Wi-Fi but **no Classic Bluetooth**; A2DP sink needs the original ESP32. The shared
`AudioSource`/protocol/listener let both coexist — pick the front-end per how you're tapping.
