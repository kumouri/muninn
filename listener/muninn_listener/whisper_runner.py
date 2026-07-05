"""Transcribe a 16 kHz mono WAV with whisper.cpp.

Default runner shells out to a whisper.cpp binary (most portable on Windows). An optional
pywhispercpp fast path and a mock runner (for tests/CI) are also provided.
"""

from __future__ import annotations

import subprocess
import tempfile
from pathlib import Path
from typing import Protocol


class WhisperRunner(Protocol):
    def transcribe(self, wav_bytes: bytes) -> str: ...


class MockWhisper:
    """Deterministic runner for tests/CI — never touches a model."""

    def __init__(self, text: str = "[mock transcript]") -> None:
        self.text = text
        self.calls = 0

    def transcribe(self, wav_bytes: bytes) -> str:
        self.calls += 1
        return self.text


class SubprocessWhisper:
    """Run the whisper.cpp CLI (`whisper-cli`/`main`) on a temp WAV and read the text out."""

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

    def transcribe(self, wav_bytes: bytes) -> str:
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
                "-nt",  # no timestamps in output
                "-otxt",  # also write <wav>.txt
                *self.extra_args,
            ]
            proc = subprocess.run(cmd, capture_output=True, text=True, check=True)
            txt = wav.with_suffix(".wav.txt")
            if txt.exists():
                return txt.read_text(encoding="utf-8").strip()
            return proc.stdout.strip()


def build_runner(args) -> WhisperRunner:
    """Construct a runner from parsed CLI args (see __main__)."""
    if getattr(args, "mock", False):
        return MockWhisper()
    if getattr(args, "pywhispercpp", False):
        from ._pywhisper import PyWhisper  # lazy: optional dependency

        return PyWhisper(model=args.model, language=args.language)
    return SubprocessWhisper(binary=args.whisper_bin, model=args.model, language=args.language)
