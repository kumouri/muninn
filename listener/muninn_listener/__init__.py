"""Muninn listener — receives tapped audio from the device and transcribes it with whisper.cpp."""

from .pipeline import Pipeline
from .protocol import Frame, FrameParser, encode
from .whisper_runner import MockWhisper, SubprocessWhisper

__version__ = "0.1.0"

__all__ = ["Pipeline", "Frame", "FrameParser", "encode", "MockWhisper", "SubprocessWhisper"]
