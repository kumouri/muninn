from muninn_listener import protocol as p
from muninn_listener.protocol import FrameParser


def test_encode_decode_roundtrip():
    pcm = bytes([0x11, 0x22, 0x33, 0x44])
    raw = p.encode(p.TYPE_AUDIO, p.FLAG_CAPTURING, 0xDEADBEEF, 12345, pcm)
    assert len(raw) == p.HEADER_SIZE + len(pcm)

    frames = FrameParser().feed(raw)
    assert len(frames) == 1
    f = frames[0]
    assert f.type == p.TYPE_AUDIO
    assert f.capturing is True
    assert f.seq == 0xDEADBEEF
    assert f.timestamp_ms == 12345
    assert f.payload == pcm


def test_stream_split_across_reads():
    raw = p.encode(p.TYPE_AUDIO, 0, 1, 0, bytes(range(10)))
    parser = FrameParser()
    # Feed one byte at a time; the frame should surface exactly once, fully.
    out = []
    for b in raw:
        out += parser.feed(bytes([b]))
    assert len(out) == 1
    assert out[0].payload == bytes(range(10))


def test_multiple_frames_one_feed():
    a = p.encode(p.TYPE_CONTROL, 0, 1, 0, bytes([p.CTRL_CAPTURE_START]))
    b = p.encode(p.TYPE_AUDIO, p.FLAG_CAPTURING, 2, 0, b"\x01\x02")
    c = p.encode(p.TYPE_CONTROL, 0, 3, 0, bytes([p.CTRL_CAPTURE_STOP]))
    frames = FrameParser().feed(a + b + c)
    assert [f.type for f in frames] == [p.TYPE_CONTROL, p.TYPE_AUDIO, p.TYPE_CONTROL]


def test_resync_after_garbage():
    good = p.encode(p.TYPE_AUDIO, 0, 9, 0, b"\x55\x66")
    stream = b"\x00\x4d\x99garbage" + good
    frames = FrameParser().feed(stream)
    assert len(frames) == 1
    assert frames[0].seq == 9
    assert frames[0].payload == b"\x55\x66"


def test_bad_version_is_skipped():
    good = p.encode(p.TYPE_AUDIO, 0, 1, 0, b"\xaa")
    corrupt = bytes([p.MAGIC[0], p.MAGIC[1], 99]) + bytes(13)  # valid magic, wrong version
    frames = FrameParser().feed(corrupt + good)
    # The corrupt header is skipped; the good frame still parses.
    assert any(f.payload == b"\xaa" for f in frames)
