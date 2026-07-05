"""The core processing pipeline: bytes -> frames -> capture segments -> transcript files."""

from __future__ import annotations

from pathlib import Path

from . import diarize
from .assembler import SegmentBuilder
from .protocol import SAMPLE_RATE_HZ, FrameParser
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
            segments = self._runner.transcribe(segment.to_wav_bytes())

            labels = None
            if segment.is_stereo:
                # Attribute each transcript segment to the louder input channel.
                labels = diarize.label_segments(
                    segments, segment.left, segment.right, SAMPLE_RATE_HZ
                )

            path = write_transcript(
                self._out_dir,
                segments,
                labels=labels,
                duration_s=segment.duration_s,
                source=self._source,
            )
            written.append(path)
        return written
