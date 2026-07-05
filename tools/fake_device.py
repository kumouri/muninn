#!/usr/bin/env python3
"""Pretend to be the Muninn device: stream a WAV file to the listener as protocol frames.

Lets you exercise the whole PC side (transport -> parser -> segment -> whisper -> transcript)
with no hardware. Pure standard library (no numpy/audioop).

Examples:
    python tools/fake_device.py --transport tcp --host 127.0.0.1 --port 5140 sample.wav
    python tools/fake_device.py --transport tcp sample.wav --realtime
"""

from __future__ import annotations

import argparse
import socket
import sys
import time
import wave
from pathlib import Path

# Make `muninn_listener` importable when run from a source checkout.
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "listener"))

from muninn_listener import protocol as p  # noqa: E402


def _read_wav_16k_mono(path: str) -> bytes:
    """Load a WAV and return s16le, 16 kHz, mono PCM (downmix + linear resample, stdlib only)."""
    with wave.open(path, "rb") as w:
        ch = w.getnchannels()
        width = w.getsampwidth()
        rate = w.getframerate()
        n = w.getnframes()
        raw = w.readframes(n)
    if width != 2:
        raise SystemExit(f"{path}: need 16-bit PCM WAV (got sampwidth={width})")

    import array

    samples = array.array("h")
    samples.frombytes(raw)
    if sys.byteorder == "big":
        samples.byteswap()

    # Downmix to mono.
    if ch > 1:
        mono = array.array("h", [0]) * (len(samples) // ch)
        for i in range(len(mono)):
            acc = 0
            for c in range(ch):
                acc += samples[i * ch + c]
            mono[i] = acc // ch
    else:
        mono = samples

    # Linear-resample to 16 kHz.
    if rate != p.SAMPLE_RATE_HZ:
        out_len = int(len(mono) * p.SAMPLE_RATE_HZ / rate)
        res = array.array("h", [0]) * out_len
        step = rate / p.SAMPLE_RATE_HZ
        for i in range(out_len):
            src = i * step
            j = int(src)
            frac = src - j
            a = mono[j]
            b = mono[j + 1] if j + 1 < len(mono) else mono[j]
            res[i] = int(a + (b - a) * frac)
        mono = res

    if sys.byteorder == "big":
        mono.byteswap()
    return mono.tobytes()


def _frames_for(pcm: bytes, realtime: bool):
    """Yield (bytes, is_audio) frames: START, audio chunks (capturing), STOP."""
    seq = 0
    yield p.encode(p.TYPE_CONTROL, 0, seq, 0, bytes([p.CTRL_CAPTURE_START])), False
    seq += 1

    chunk = p.FRAME_SAMPLES * 2  # 320 samples * 2 bytes
    for off in range(0, len(pcm), chunk):
        block = pcm[off : off + chunk]
        yield p.encode(p.TYPE_AUDIO, p.FLAG_CAPTURING, seq, off, block), True
        seq += 1

    yield p.encode(p.TYPE_CONTROL, 0, seq, 0, bytes([p.CTRL_CAPTURE_STOP])), False


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("wav", help="path to a WAV file")
    ap.add_argument("--transport", choices=["tcp"], default="tcp")
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=5140)
    ap.add_argument(
        "--realtime", action="store_true", help="pace at ~20 ms/frame like real audio"
    )
    args = ap.parse_args(argv)

    pcm = _read_wav_16k_mono(args.wav)
    dur = len(pcm) / 2 / p.SAMPLE_RATE_HZ
    print(
        f"[fake-device] {args.wav}: {dur:.1f}s of 16 kHz mono -> {args.host}:{args.port}"
    )

    sock = socket.create_connection((args.host, args.port))
    try:
        for frame, is_audio in _frames_for(pcm, args.realtime):
            sock.sendall(frame)
            if args.realtime and is_audio:
                time.sleep(p.FRAME_MS / 1000)
    finally:
        sock.close()
    print("[fake-device] done — capture sent.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
