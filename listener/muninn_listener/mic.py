"""Desktop webcam-mic capture — the voice channel when the device only forwards program audio.

The device (A2DP sink) sends the mixer mix as mono "program". Your voice is captured *locally* on
the desktop webcam mic into a rolling, timestamped buffer; when a capture finalizes, the pipeline
pulls the aligned slice and diarizes program vs voice.

`MicBuffer` is pure standard library (unit-tested). `DesktopMic` wraps `sounddevice` (optional dep)
to feed it live audio.
"""

from __future__ import annotations

import array
import threading

from .protocol import SAMPLE_RATE_HZ


class MicBuffer:
    """Rolling buffer of recent mic audio (s16le mono @ sample_rate), indexed by wall-clock time."""

    def __init__(self, sample_rate: int = SAMPLE_RATE_HZ, max_seconds: float = 60.0) -> None:
        self.sr = sample_rate
        self._buf = bytearray()
        self._start_t: float | None = None  # wall time of buf[0]
        self._max_bytes = int(max_seconds * sample_rate) * 2
        self._lock = threading.Lock()

    def add(self, pcm: bytes, start_time: float) -> None:
        """Append a block; `start_time` is the wall-clock time of its first sample."""
        with self._lock:
            if self._start_t is None:
                self._start_t = start_time
            self._buf.extend(pcm)
            if len(self._buf) > self._max_bytes:
                drop = len(self._buf) - self._max_bytes
                drop -= drop % 2  # keep sample alignment
                del self._buf[:drop]
                self._start_t += (drop // 2) / self.sr

    def slice(self, t0: float, t1: float) -> bytes:
        """Return the s16le mono audio captured in [t0, t1) wall-clock seconds (clamped)."""
        with self._lock:
            if self._start_t is None or t1 <= t0:
                return b""
            a = int((t0 - self._start_t) * self.sr) * 2
            b = int((t1 - self._start_t) * self.sr) * 2
            a = max(0, min(a, len(self._buf)))
            b = max(a, min(b, len(self._buf)))
            return bytes(self._buf[a:b])


class DesktopMic:  # pragma: no cover - needs a real audio device
    """Feeds a MicBuffer from the default input device via sounddevice (optional dependency)."""

    def __init__(self, buffer: MicBuffer, device=None, clock=None) -> None:
        import time

        self._buf = buffer
        self._device = device
        self._clock = clock or time.monotonic
        self._t_ref: float | None = None
        self._samples = 0
        self._stream = None

    def start(self) -> None:
        try:
            import sounddevice as sd
        except ImportError as e:
            raise SystemExit(
                "desktop mic needs sounddevice: pip install 'muninn-listener[mic]'"
            ) from e

        def cb(indata, frames, time_info, status):
            if self._t_ref is None:
                self._t_ref = self._clock()
            start_time = self._t_ref + self._samples / self._buf.sr
            self._samples += frames
            self._buf.add(bytes(indata), start_time)

        self._stream = sd.RawInputStream(
            samplerate=self._buf.sr,
            channels=1,
            dtype="int16",
            device=self._device,
            callback=cb,
        )
        self._stream.start()

    def stop(self) -> None:
        if self._stream is not None:
            self._stream.stop()
            self._stream.close()


def mix_two_mono(a: bytes, b: bytes) -> bytes:
    """Average two s16le mono buffers to the shorter length (for the whisper input)."""
    aa = array.array("h")
    aa.frombytes(a)
    bb = array.array("h")
    bb.frombytes(b)
    n = min(len(aa), len(bb))
    out = array.array("h", bytes(2 * n))
    for i in range(n):
        out[i] = (aa[i] + bb[i]) // 2
    return out.tobytes()
