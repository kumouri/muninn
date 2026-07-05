#!/usr/bin/env python3
"""Pretend to be the Muninn device: stream a WAV file to the listener as protocol frames.

Lets you exercise the whole PC side (transport -> parser -> segment -> whisper -> transcript)
with no hardware. Pure standard library (no numpy/audioop).

    python tools/fake_device.py --transport tcp --port 5140 sample.wav
    python tools/fake_device.py --transport tcp --port 5140 stereo.wav --stereo   # diarization

With --stereo, the two WAV channels are sent as L=program, R=voice (a mono WAV is duplicated).
"""

from __future__ import annotations

import argparse
import array
import socket
import sys
import time
import wave
from pathlib import Path

# Make `muninn_listener` importable when run from a source checkout.
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "listener"))

from muninn_listener import protocol as p  # noqa: E402


def _resample_mono(samples: array.array, in_rate: int) -> array.array:
    """Linear-resample a mono int16 array to 16 kHz."""
    if in_rate == p.SAMPLE_RATE_HZ or len(samples) == 0:
        return samples
    out_len = int(len(samples) * p.SAMPLE_RATE_HZ / in_rate)
    res = array.array("h", bytes(2 * out_len))
    step = in_rate / p.SAMPLE_RATE_HZ
    for i in range(out_len):
        src = i * step
        j = int(src)
        frac = src - j
        a = samples[j]
        b = samples[j + 1] if j + 1 < len(samples) else samples[j]
        res[i] = int(a + (b - a) * frac)
    return res


def _load_channels_16k(path: str) -> list[array.array]:
    """Load a WAV and return a list of per-channel int16 arrays resampled to 16 kHz."""
    with wave.open(path, "rb") as w:
        ch = w.getnchannels()
        width = w.getsampwidth()
        rate = w.getframerate()
        raw = w.readframes(w.getnframes())
    if width != 2:
        raise SystemExit(f"{path}: need 16-bit PCM WAV (got sampwidth={width})")

    interleaved = array.array("h")
    interleaved.frombytes(raw)
    if sys.byteorder == "big":
        interleaved.byteswap()

    channels = []
    for c in range(ch):
        chan = array.array("h", interleaved[c::ch])
        channels.append(_resample_mono(chan, rate))
    return channels


def _to_mono_bytes(channels: list[array.array]) -> bytes:
    if len(channels) == 1:
        out = channels[0]
    else:
        n = min(len(c) for c in channels)
        out = array.array("h", bytes(2 * n))
        for i in range(n):
            out[i] = sum(c[i] for c in channels) // len(channels)
    if sys.byteorder == "big":
        out = array.array("h", out)
        out.byteswap()
    return out.tobytes()


def _to_stereo_bytes(channels: list[array.array]) -> bytes:
    left = channels[0]
    right = channels[1] if len(channels) > 1 else channels[0]
    n = min(len(left), len(right))
    out = array.array("h", bytes(4 * n))
    for i in range(n):
        out[2 * i] = left[i]
        out[2 * i + 1] = right[i]
    if sys.byteorder == "big":
        out.byteswap()
    return out.tobytes()


def _frames_for(pcm: bytes, stereo: bool):
    """Yield (bytes, is_audio) frames: START, audio chunks (capturing), STOP."""
    seq = 0
    yield p.encode(p.TYPE_CONTROL, 0, seq, 0, bytes([p.CTRL_CAPTURE_START])), False
    seq += 1

    flags = p.FLAG_CAPTURING | (p.FLAG_STEREO if stereo else 0)
    bytes_per_sample_frame = 4 if stereo else 2  # stereo = 2ch * 2 bytes
    chunk = p.FRAME_SAMPLES * bytes_per_sample_frame  # ~20 ms
    for off in range(0, len(pcm), chunk):
        yield p.encode(p.TYPE_AUDIO, flags, seq, off, pcm[off : off + chunk]), True
        seq += 1

    yield p.encode(p.TYPE_CONTROL, 0, seq, 0, bytes([p.CTRL_CAPTURE_STOP])), False


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("wav", help="path to a WAV file")
    ap.add_argument("--transport", choices=["tcp"], default="tcp")
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=5140)
    ap.add_argument(
        "--stereo", action="store_true", help="send L=program, R=voice for diarization"
    )
    ap.add_argument(
        "--realtime", action="store_true", help="pace at ~20 ms/frame like real audio"
    )
    args = ap.parse_args(argv)

    channels = _load_channels_16k(args.wav)
    pcm = _to_stereo_bytes(channels) if args.stereo else _to_mono_bytes(channels)
    frame_bytes = 4 if args.stereo else 2
    dur = len(pcm) / frame_bytes / p.SAMPLE_RATE_HZ
    mode = "stereo" if args.stereo else "mono"
    print(
        f"[fake-device] {args.wav}: {dur:.1f}s {mode} 16 kHz -> {args.host}:{args.port}"
    )

    sock = socket.create_connection((args.host, args.port))
    try:
        for frame, is_audio in _frames_for(pcm, args.stereo):
            sock.sendall(frame)
            if args.realtime and is_audio:
                time.sleep(p.FRAME_MS / 1000)
    finally:
        sock.close()
    print("[fake-device] done — capture sent.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
