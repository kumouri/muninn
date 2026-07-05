# Muninn wire protocol

The device and the PC listener exchange a stream of **frames**. The same framing is used over every
transport (USB-CDC, Wi-Fi TCP, and SD store-and-forward files). This document is **authoritative** —
the firmware encoder (`firmware/src/transport/`) and the Python decoder (`listener/muninn_listener/
protocol.py`) must match it byte-for-byte. Change both together and bump `PROTOCOL_VERSION`.

- **Current version:** `1`
- **Byte order:** little-endian (native to both ESP32 and x86).
- **Audio format (implied, constant):** 16 kHz, mono, signed 16-bit PCM (`s16le`).

## Frame

```
 offset  size  field          notes
 ------  ----  -------------  ---------------------------------------------
   0      2    magic          ASCII 'M','N'  (0x4D 0x4E)
   2      1    version        PROTOCOL_VERSION (currently 1)
   3      1    type           1=AUDIO  2=CONTROL  3=TEXT_ACK
   4      1    flags          bit0 = CAPTURING (this audio is inside a capture)
   5      1    reserved       0 (reserved for a future header checksum)
   6      4    seq            uint32, monotonically increasing per frame
  10      4    timestamp_ms   uint32, device uptime in milliseconds
  14      2    payload_len    uint16, number of payload bytes that follow
  16   payload_len  payload   see per-type meaning below
```

Header is a fixed **16 bytes**; total frame size is `16 + payload_len`.

## Payload by type

| type | name | direction | payload |
|-----:|------|-----------|---------|
| `1` | `AUDIO` | device → listener | `payload_len` bytes of `s16le` PCM (`sample_count = payload_len / 2`) |
| `2` | `CONTROL` | either | 1 byte control code (see below) |
| `3` | `TEXT_ACK` | listener → device | UTF-8 text of the finished transcript segment |

### Control codes (type = 2)

| code | meaning |
|-----:|---------|
| `0x01` | `CAPTURE_START` — user began a capture segment |
| `0x02` | `CAPTURE_STOP` — user ended a capture segment (listener may finalize + transcribe) |
| `0x03` | `PING` — keep-alive / liveness check |
| `0x04` | `PONG` — reply to PING |

The `CAPTURING` flag on `AUDIO` frames is the ground truth for which samples belong to a capture;
`CAPTURE_START`/`CAPTURE_STOP` are explicit segment boundaries so the listener can finalize promptly
even if the last audio frame was dropped on a lossy transport.

## Framing notes

- **USB-CDC and TCP are reliable, ordered byte streams** — the decoder resynchronizes by scanning for
  the `MN` magic if it ever loses alignment, then validates `version` and a sane `payload_len`
  (≤ 8192) before accepting a frame.
- **Audio chunking:** the device emits ~20 ms of audio per frame (320 samples = 640 bytes at 16 kHz)
  to keep latency low and buffers small.
- **No CRC in v1:** all transports are reliable. The `reserved` byte is kept for a future header
  checksum if a lossy transport (e.g. raw UDP) is added.

## Reference constants

```
MAGIC0 = 0x4D   # 'M'
MAGIC1 = 0x4E   # 'N'
PROTOCOL_VERSION = 1
HEADER_SIZE = 16
SAMPLE_RATE_HZ = 16000
FRAME_MS = 20            # 320 samples / 640 bytes per AUDIO frame

TYPE_AUDIO    = 1
TYPE_CONTROL  = 2
TYPE_TEXT_ACK = 3

FLAG_CAPTURING = 0x01

CTRL_CAPTURE_START = 0x01
CTRL_CAPTURE_STOP  = 0x02
CTRL_PING          = 0x03
CTRL_PONG          = 0x04
```
