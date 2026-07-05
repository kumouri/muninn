"""Turn a stream of protocol frames into finished capture segments (16 kHz mono WAV bytes)."""

from __future__ import annotations

import io
import wave
from dataclasses import dataclass, field

from . import protocol as p


@dataclass
class Segment:
    pcm: bytes  # s16le, 16 kHz, mono
    first_seq: int
    first_timestamp_ms: int

    @property
    def duration_s(self) -> float:
        return len(self.pcm) / 2 / p.SAMPLE_RATE_HZ

    def to_wav_bytes(self) -> bytes:
        buf = io.BytesIO()
        with wave.open(buf, "wb") as w:
            w.setnchannels(1)
            w.setsampwidth(2)
            w.setframerate(p.SAMPLE_RATE_HZ)
            w.writeframes(self.pcm)
        return buf.getvalue()


@dataclass
class SegmentBuilder:
    """Accumulates audio while capturing; emits a Segment when a capture ends.

    A capture is the run of AUDIO frames carrying FLAG_CAPTURING. CONTROL START/STOP frames are
    honored as explicit boundaries so a dropped final audio frame still finalizes the segment.
    """

    _active: bool = False
    _buf: bytearray = field(default_factory=bytearray)
    _first_seq: int = 0
    _first_ts: int = 0

    def push(self, frame: p.Frame) -> Segment | None:
        if frame.type == p.TYPE_CONTROL and frame.payload:
            code = frame.payload[0]
            if code == p.CTRL_CAPTURE_START:
                self._begin(frame)
            elif code == p.CTRL_CAPTURE_STOP:
                return self._finalize()
            return None

        if frame.type == p.TYPE_AUDIO:
            if frame.capturing:
                if not self._active:
                    self._begin(frame)
                self._buf.extend(frame.payload)
            elif self._active:
                return self._finalize()
        return None

    def _begin(self, frame: p.Frame) -> None:
        self._active = True
        self._buf = bytearray()
        self._first_seq = frame.seq
        self._first_ts = frame.timestamp_ms

    def _finalize(self) -> Segment | None:
        if not self._active:
            return None
        self._active = False
        if not self._buf:
            return None
        return Segment(bytes(self._buf), self._first_seq, self._first_ts)
