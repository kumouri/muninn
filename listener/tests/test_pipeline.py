from muninn_listener import protocol as p
from muninn_listener.pipeline import Pipeline
from muninn_listener.whisper_runner import MockWhisper


def test_pipeline_writes_transcript_on_capture(tmp_path):
    runner = MockWhisper("hello world")
    pipe = Pipeline(runner, out_dir=tmp_path)

    stream = b"".join(
        [
            p.encode(p.TYPE_CONTROL, 0, 0, 0, bytes([p.CTRL_CAPTURE_START])),
            p.encode(p.TYPE_AUDIO, p.FLAG_CAPTURING, 1, 0, b"\x01\x00" * 320),
            p.encode(p.TYPE_CONTROL, 0, 2, 0, bytes([p.CTRL_CAPTURE_STOP])),
        ]
    )
    written = pipe.feed(stream)

    assert runner.calls == 1
    assert len(written) == 1
    text = written[0].read_text(encoding="utf-8")
    assert "hello world" in text
    assert written[0].suffix == ".md"


def test_pipeline_no_output_without_capture(tmp_path):
    pipe = Pipeline(MockWhisper(), out_dir=tmp_path)
    stream = p.encode(p.TYPE_AUDIO, 0, 1, 0, b"\x00\x00" * 10)  # not capturing
    assert pipe.feed(stream) == []
    assert list(tmp_path.iterdir()) == []
