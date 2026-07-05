"""Muninn wire-protocol codec — mirrors docs/protocol.md (authoritative).

Pure standard library. Must stay byte-for-byte compatible with firmware/lib/muninn_proto.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass

MAGIC = b"MN"
VERSION = 1
HEADER_SIZE = 16
MAX_PAYLOAD = 8192

# Audio format (implied, constant)
SAMPLE_RATE_HZ = 16000
FRAME_MS = 20
FRAME_SAMPLES = SAMPLE_RATE_HZ * FRAME_MS // 1000  # 320

# Frame types
TYPE_AUDIO = 1
TYPE_CONTROL = 2
TYPE_TEXT_ACK = 3
TYPE_METER = 4  # device -> listener: per-channel level for the UI meter

# Flags
FLAG_CAPTURING = 0x01
FLAG_STEREO = 0x02  # AUDIO payload is interleaved stereo s16 (L=program, R=voice) for diarization

# Control codes
CTRL_CAPTURE_START = 0x01
CTRL_CAPTURE_STOP = 0x02
CTRL_PING = 0x03
CTRL_PONG = 0x04

# magic[2], version, type, flags, reserved, seq(u32), timestamp_ms(u32), payload_len(u16)
_HEADER = struct.Struct("<2sBBBBIIH")
assert _HEADER.size == HEADER_SIZE


@dataclass(frozen=True)
class Frame:
    type: int
    flags: int
    seq: int
    timestamp_ms: int
    payload: bytes

    @property
    def capturing(self) -> bool:
        return bool(self.flags & FLAG_CAPTURING)

    @property
    def stereo(self) -> bool:
        return bool(self.flags & FLAG_STEREO)


def encode(
    type: int,
    flags: int,
    seq: int,
    timestamp_ms: int,
    payload: bytes = b"",
) -> bytes:
    """Encode one frame (header + payload)."""
    if len(payload) > MAX_PAYLOAD:
        raise ValueError(f"payload too large: {len(payload)} > {MAX_PAYLOAD}")
    header = _HEADER.pack(
        MAGIC,
        VERSION,
        type & 0xFF,
        flags & 0xFF,
        0,  # reserved
        seq & 0xFFFFFFFF,
        timestamp_ms & 0xFFFFFFFF,
        len(payload),
    )
    return header + payload


class FrameParser:
    """Incremental byte-stream parser. feed(data) -> list[Frame]; resynchronizes on garbage."""

    def __init__(self) -> None:
        self._buf = bytearray()

    def feed(self, data: bytes) -> list[Frame]:
        self._buf.extend(data)
        frames: list[Frame] = []
        while True:
            frame, consumed = self._try_one()
            if consumed == 0:
                break
            if frame is not None:
                frames.append(frame)
            del self._buf[:consumed]
        return frames

    def _try_one(self) -> tuple[Frame | None, int]:
        buf = self._buf
        n = len(buf)
        if n < 1:
            return None, 0
        if buf[0] != MAGIC[0]:
            nxt = buf.find(MAGIC[0], 1)
            return None, (nxt if nxt != -1 else n)  # skip garbage, resync on next 'M'
        if n < 2:
            return None, 0
        if buf[1] != MAGIC[1]:
            return None, 1  # stray 'M', skip it
        if n < HEADER_SIZE:
            return None, 0
        _, version, typ, flags, _reserved, seq, ts, plen = _HEADER.unpack_from(buf, 0)
        if version != VERSION or plen > MAX_PAYLOAD:
            return None, 2  # bad header behind valid magic; skip past it
        total = HEADER_SIZE + plen
        if n < total:
            return None, 0
        payload = bytes(buf[HEADER_SIZE:total])
        return Frame(typ, flags, seq, ts, payload), total
