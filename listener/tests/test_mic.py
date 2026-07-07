import array

from muninn_listener import protocol as p
from muninn_listener.mic import MicBuffer, mix_two_mono
from muninn_listener.pipeline import Pipeline
from muninn_listener.whisper_runner import MockWhisper, TranscriptSegment

SR = 16000


def _pcm(vals):
    return array.array("h", vals).tobytes()


def test_mic_buffer_slices_by_time():
    mic = MicBuffer(SR)
    mic.add(_pcm([10] * SR), start_time=0.0)  # 0..1 s quiet
    mic.add(_pcm([5000] * SR), start_time=1.0)  # 1..2 s loud

    quiet = array.array("h")
    quiet.frombytes(mic.slice(0.0, 1.0))
    loud = array.array("h")
    loud.frombytes(mic.slice(1.0, 2.0))
    assert max(abs(x) for x in quiet) == 10
    assert max(abs(x) for x in loud) == 5000


def test_mic_buffer_trims_to_capacity():
    mic = MicBuffer(SR, max_seconds=1.0)
    mic.add(_pcm([1] * SR), start_time=0.0)
    mic.add(_pcm([2] * SR), start_time=1.0)  # exceeds 1 s; oldest dropped
    # The window now starts near t=1.0; asking for [0,1) returns little/nothing.
    assert mic.slice(1.0, 2.0) != b""
    assert len(mic.slice(0.0, 0.5)) == 0


def test_mix_two_mono_averages_to_shorter():
    a = _pcm([100, 200, 300])
    b = _pcm([10, 20])
    assert mix_two_mono(a, b) == _pcm([55, 110])  # averaged, truncated to 2


def test_pipeline_merges_desktop_mic_and_diarizes(tmp_path):
    # Program capture is 0.1 s and quiet; the desktop mic is loud in that window -> label "You".
    n = 1600  # 0.1 s
    program = _pcm([50] * n)

    # MicBuffer appends contiguously; start_time anchors the first block. Build one block that is
    # quiet up to 1.9 s and loud over [1.9, 2.0).
    quiet_n = int(1.9 * SR)
    loud_n = int(0.1 * SR)
    mic = MicBuffer(SR)
    mic.add(_pcm([10] * quiet_n + [6000] * loud_n), start_time=0.0)

    # Finalize "now" = 2.0, delay 0 -> voice window [1.9, 2.0): the loud region.
    pipe = Pipeline(
        MockWhisper(segments=[TranscriptSegment(0.0, 0.1, "hey there")]),
        out_dir=tmp_path,
        mic_buffer=mic,
        program_delay_ms=0.0,
        clock=lambda: 2.0,
    )

    stream = b"".join(
        [
            p.encode(p.TYPE_CONTROL, 0, 0, 0, bytes([p.CTRL_CAPTURE_START])),
            p.encode(p.TYPE_AUDIO, p.FLAG_CAPTURING, 1, 0, program),
            p.encode(p.TYPE_CONTROL, 0, 2, 0, bytes([p.CTRL_CAPTURE_STOP])),
        ]
    )
    written = pipe.feed(stream)
    assert len(written) == 1
    text = written[0].read_text(encoding="utf-8")
    assert "**You:** hey there" in text


def test_pipeline_mono_without_mic_is_unlabeled(tmp_path):
    program = _pcm([50] * 1600)
    pipe = Pipeline(MockWhisper(text="plain"), out_dir=tmp_path)  # no mic
    stream = b"".join(
        [
            p.encode(p.TYPE_CONTROL, 0, 0, 0, bytes([p.CTRL_CAPTURE_START])),
            p.encode(p.TYPE_AUDIO, p.FLAG_CAPTURING, 1, 0, program),
            p.encode(p.TYPE_CONTROL, 0, 2, 0, bytes([p.CTRL_CAPTURE_STOP])),
        ]
    )
    text = pipe.feed(stream)[0].read_text(encoding="utf-8")
    assert "plain" in text
    assert "**You:**" not in text  # no diarization labels
    assert "**Program:**" not in text
    assert "Diarized" not in text
