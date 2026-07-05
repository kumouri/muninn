"""The core processing pipeline: bytes -> frames -> capture segments -> transcript files."""

from __future__ import annotations

from pathlib import Path

from .assembler import SegmentBuilder
from .protocol import FrameParser
from .whisper_runner import WhisperRunner
from .writer import write_transcript


class Pipeline:
    def __init__(self, runner: WhisperRunner, out_dir: str | Path, source: str = "muninn") -> None:
        self._parser = FrameParser()
        self._builder = SegmentBuilder()
        self._runner = runner
        self._out_dir = out_dir
        self._source = source

    def feed(self, data: bytes) -> list[Path]:
        """Feed raw transport bytes; return paths of transcripts written from completed captures."""
        written: list[Path] = []
        for frame in self._parser.feed(data):
            segment = self._builder.push(frame)
            if segment is None:
                continue
            text = self._runner.transcribe(segment.to_wav_bytes())
            path = write_transcript(
                self._out_dir, text, duration_s=segment.duration_s, source=self._source
            )
            written.append(path)
        return written
