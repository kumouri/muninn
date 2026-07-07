"""The core processing pipeline: bytes -> frames -> capture segments -> transcript files."""

from __future__ import annotations

import time
from collections.abc import Callable
from pathlib import Path

from . import diarize
from .assembler import Segment, SegmentBuilder
from .meter import MeterReading, parse_meter
from .mic import MicBuffer, mix_two_mono
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
        mic_buffer: MicBuffer | None = None,
        program_delay_ms: float = 150.0,
        clock: Callable[[], float] = time.monotonic,
    ) -> None:
        self._parser = FrameParser()
        self._builder = SegmentBuilder()
        self._runner = runner
        self._out_dir = out_dir
        self._source = source
        self._on_meter = on_meter
        self._mic = mic_buffer
        self._program_delay_ms = program_delay_ms
        self._clock = clock

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

            # Desktop-mic voice: merge the locally-captured mic as the "voice" channel.
            if self._mic is not None and not segment.is_stereo:
                segment = self._merge_desktop_mic(segment)

            segments = self._runner.transcribe(segment.to_wav_bytes())

            labels = None
            if segment.is_stereo:
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

    def _merge_desktop_mic(self, segment: Segment) -> Segment:
        """Attach the aligned desktop-mic slice as the voice (R) channel; program stays L."""
        now = self._clock()
        dur = segment.duration_s
        delay = self._program_delay_ms / 1000.0
        # Program arrives `delay` behind real time, so the matching mic audio was captured earlier.
        voice = self._mic.slice(now - delay - dur, now - delay)
        if not voice:
            return segment  # no mic audio for this window; leave it mono
        program = segment.pcm
        return Segment(
            pcm=mix_two_mono(program, voice),  # whisper hears program + voice
            first_seq=segment.first_seq,
            first_timestamp_ms=segment.first_timestamp_ms,
            left=program,
            right=voice,
        )
