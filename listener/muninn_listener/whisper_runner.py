"""Transcribe a 16 kHz mono WAV with whisper.cpp, returning timestamped segments.

Segment timestamps are relative to the start of the capture, so they line up with the per-channel
audio the diarizer uses to label "You" vs "Program".

Default runner shells out to a whisper.cpp binary (most portable on Windows). An optional
pywhispercpp fast path and a mock runner (for tests/CI) are also provided.
"""

from __future__ import annotations

import io
import json
import subprocess
import tempfile
import wave
from dataclasses import dataclass
from pathlib import Path
from typing import Protocol


@dataclass
class TranscriptSegment:
    start: float  # seconds from the start of the capture
    end: float
    text: str


class WhisperRunner(Protocol):
    def transcribe(self, wav_bytes: bytes) -> list[TranscriptSegment]: ...


def _wav_duration_s(wav_bytes: bytes) -> float:
    with wave.open(io.BytesIO(wav_bytes), "rb") as w:
        return w.getnframes() / float(w.getframerate() or 1)


class MockWhisper:
    """Deterministic runner for tests/CI — never touches a model.

    Pass `segments` to return an exact timeline, or `text` for a single segment spanning the WAV.
    """

    def __init__(
        self, text: str = "[mock transcript]", segments: list[TranscriptSegment] | None = None
    ) -> None:
        self.text = text
        self._segments = segments
        self.calls = 0

    def transcribe(self, wav_bytes: bytes) -> list[TranscriptSegment]:
        self.calls += 1
        if self._segments is not None:
            return list(self._segments)
        return [TranscriptSegment(0.0, _wav_duration_s(wav_bytes), self.text)]


class SubprocessWhisper:
    """Run the whisper.cpp CLI (`whisper-cli`/`main`) with JSON output and parse segment times."""

    def __init__(
        self,
        binary: str = "whisper-cli",
        model: str = "models/ggml-base.en.bin",
        language: str = "en",
        extra_args: list[str] | None = None,
    ) -> None:
        self.binary = binary
        self.model = model
        self.language = language
        self.extra_args = extra_args or []

    def transcribe(self, wav_bytes: bytes) -> list[TranscriptSegment]:
        with tempfile.TemporaryDirectory() as td:
            wav = Path(td) / "segment.wav"
            wav.write_bytes(wav_bytes)
            cmd = [
                self.binary,
                "-m",
                self.model,
                "-f",
                str(wav),
                "-l",
                self.language,
                "-oj",  # write <wav>.json with per-segment offsets
                *self.extra_args,
            ]
            subprocess.run(cmd, capture_output=True, text=True, check=True)
            js = wav.with_suffix(".wav.json")
            if not js.exists():
                return []
            data = json.loads(js.read_text(encoding="utf-8"))
            out: list[TranscriptSegment] = []
            for seg in data.get("transcription", []):
                off = seg.get("offsets", {})
                out.append(
                    TranscriptSegment(
                        start=off.get("from", 0) / 1000.0,
                        end=off.get("to", 0) / 1000.0,
                        text=seg.get("text", "").strip(),
                    )
                )
            return out


def build_runner(args) -> WhisperRunner:
    """Construct a runner from parsed CLI args (see __main__)."""
    if getattr(args, "mock", False):
        return MockWhisper()
    if getattr(args, "pywhispercpp", False):
        from ._pywhisper import PyWhisper  # lazy: optional dependency

        return PyWhisper(model=args.model, language=args.language)
    return SubprocessWhisper(binary=args.whisper_bin, model=args.model, language=args.language)
