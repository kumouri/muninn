"""Optional pywhispercpp fast path (in-process, no subprocess). Requires `pip install pywhispercpp`.

Kept separate so the core listener has zero third-party dependencies.
"""

from __future__ import annotations

import io
import wave


class PyWhisper:
    def __init__(self, model: str = "base.en", language: str = "en") -> None:
        from pywhispercpp.model import Model  # imported lazily; optional dependency

        self._model = Model(model)
        self._language = language

    def transcribe(self, wav_bytes: bytes) -> str:
        import numpy as np  # pywhispercpp already pulls numpy

        with wave.open(io.BytesIO(wav_bytes), "rb") as w:
            frames = w.readframes(w.getnframes())
        audio = np.frombuffer(frames, dtype=np.int16).astype(np.float32) / 32768.0
        segments = self._model.transcribe(audio, language=self._language)
        return " ".join(s.text.strip() for s in segments).strip()
