"""The core processing pipeline: bytes -> frames -> capture segments -> transcript files."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

from . import diarize
from .assembler import SegmentBuilder
from .meter import MeterReading, parse_meter
from .protocol import SAMPLE_RATE_HZ, TYPE_METER, FrameParser
from .whisper_runner import WhisperRunner
from .writer import write_transcript


class Pipeline:
    def __init__(
        self,
        runner: WhisperRunner,
        out_dir: str | Path,
        source: str = "muninn",
        on_meter: Callable[[MeterReading], None] | None = None,
    ) -> None:
        self._parser = FrameParser()
        self._builder = SegmentBuilder()
        self._runner = runner
        self._out_dir = out_dir
        self._source = source
        self._on_meter = on_meter

    def feed(self, data: bytes) -> list[Path]:
        """Feed raw transport bytes; return paths of transcripts written from completed captures."""
        written: list[Path] = []
        for frame in self._parser.feed(data):
            if frame.type == TYPE_METER:
                if self._on_meter is not None:
                    self._on_meter(parse_meter(frame.payload))
                continue
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
