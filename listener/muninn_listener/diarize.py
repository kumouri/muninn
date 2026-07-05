"""Channel-based diarization: label each transcript segment "You" / "Program" / "Both".

Muninn already separates the speakers by input channel (program = L, your voice = R), so instead
of a diarization model we attribute each whisper segment to whichever channel carried more energy
during that segment's time window.
"""

from __future__ import annotations

import array

from .whisper_runner import TranscriptSegment

LABEL_PROGRAM = "Program"
LABEL_YOU = "You"
LABEL_BOTH = "Both"


def _energy(pcm: bytes, start_sample: int, end_sample: int) -> float:
    a = array.array("h")
    a.frombytes(pcm)
    lo = max(0, start_sample)
    hi = min(len(a), end_sample)
    if hi <= lo:
        return 0.0
    # Sum of squares; scaled down to keep the numbers modest.
    return sum(float(x) * x for x in a[lo:hi]) / (hi - lo)


def label_segments(
    segments: list[TranscriptSegment],
    left_pcm: bytes,
    right_pcm: bytes,
    sample_rate: int,
    margin: float = 1.3,
) -> list[str]:
    """Return a label per transcript segment. `left`=program, `right`=your voice."""
    labels: list[str] = []
    for seg in segments:
        a = int(max(0.0, seg.start) * sample_rate)
        b = int(max(seg.start, seg.end) * sample_rate)
        prog = _energy(left_pcm, a, b)
        you = _energy(right_pcm, a, b)
        labels.append(_label(prog, you, margin))
    return labels


def _label(prog: float, you: float, margin: float) -> str:
    if prog <= 0 and you <= 0:
        return LABEL_BOTH
    if prog >= you * margin:
        return LABEL_PROGRAM
    if you >= prog * margin:
        return LABEL_YOU
    return LABEL_BOTH
