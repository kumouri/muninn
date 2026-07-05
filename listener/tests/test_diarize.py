import array

from muninn_listener.diarize import LABEL_BOTH, LABEL_PROGRAM, LABEL_YOU, label_segments
from muninn_listener.whisper_runner import TranscriptSegment

SR = 16000


def _pcm(vals):
    return array.array("h", vals).tobytes()


def test_label_you_when_voice_channel_louder():
    left = _pcm([10] * SR)  # program quiet
    right = _pcm([5000] * SR)  # your voice loud
    labels = label_segments([TranscriptSegment(0.0, 1.0, "hi")], left, right, SR)
    assert labels == [LABEL_YOU]


def test_label_program_when_program_channel_louder():
    left = _pcm([5000] * SR)
    right = _pcm([10] * SR)
    labels = label_segments([TranscriptSegment(0.0, 1.0, "music")], left, right, SR)
    assert labels == [LABEL_PROGRAM]


def test_label_both_when_similar():
    left = _pcm([1000] * SR)
    right = _pcm([1000] * SR)
    labels = label_segments([TranscriptSegment(0.0, 1.0, "overlap")], left, right, SR)
    assert labels == [LABEL_BOTH]


def test_labels_are_per_segment_window():
    # First second program-dominant, second second voice-dominant.
    left = _pcm([5000] * SR + [10] * SR)
    right = _pcm([10] * SR + [5000] * SR)
    segs = [TranscriptSegment(0.0, 1.0, "a"), TranscriptSegment(1.0, 2.0, "b")]
    assert label_segments(segs, left, right, SR) == [LABEL_PROGRAM, LABEL_YOU]
