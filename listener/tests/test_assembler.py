import array
import wave
from io import BytesIO

from muninn_listener import protocol as p
from muninn_listener.assembler import SegmentBuilder


def _audio(seq, pcm, capturing=True):
    flags = p.FLAG_CAPTURING if capturing else 0
    raw = p.encode(p.TYPE_AUDIO, flags, seq, seq, pcm)
    return p.FrameParser().feed(raw)[0]


def _stereo_audio(seq, left, right):
    inter = array.array("h")
    for lft, rgt in zip(left, right):
        inter.append(lft)
        inter.append(rgt)
    raw = p.encode(p.TYPE_AUDIO, p.FLAG_CAPTURING | p.FLAG_STEREO, seq, seq, inter.tobytes())
    return p.FrameParser().feed(raw)[0]


def _control(code, seq=0):
    raw = p.encode(p.TYPE_CONTROL, 0, seq, seq, bytes([code]))
    return p.FrameParser().feed(raw)[0]


def test_segment_bounded_by_control_frames():
    b = SegmentBuilder()
    assert b.push(_control(p.CTRL_CAPTURE_START)) is None
    assert b.push(_audio(1, b"\x01\x00\x02\x00")) is None
    assert b.push(_audio(2, b"\x03\x00\x04\x00")) is None
    seg = b.push(_control(p.CTRL_CAPTURE_STOP))
    assert seg is not None
    assert seg.pcm == b"\x01\x00\x02\x00\x03\x00\x04\x00"


def test_segment_bounded_by_capturing_flag():
    b = SegmentBuilder()
    assert b.push(_audio(1, b"\xaa\xbb", capturing=True)) is None
    seg = b.push(_audio(2, b"", capturing=False))  # capturing drops -> finalize
    assert seg is not None
    assert seg.pcm == b"\xaa\xbb"


def test_non_capturing_audio_is_ignored():
    b = SegmentBuilder()
    assert b.push(_audio(1, b"\x00\x00", capturing=False)) is None


def test_stereo_segment_keeps_channels_and_downmixes():
    b = SegmentBuilder()
    b.push(_control(p.CTRL_CAPTURE_START))
    b.push(_stereo_audio(1, left=[100, 100], right=[300, 300]))
    seg = b.push(_control(p.CTRL_CAPTURE_STOP))
    assert seg is not None
    assert seg.is_stereo
    assert seg.left == array.array("h", [100, 100]).tobytes()
    assert seg.right == array.array("h", [300, 300]).tobytes()
    assert seg.pcm == array.array("h", [200, 200]).tobytes()  # (L+R)/2 mono for whisper


def test_segment_wav_is_16k_mono_s16():
    b = SegmentBuilder()
    b.push(_control(p.CTRL_CAPTURE_START))
    b.push(_audio(1, b"\x00\x01" * 160))  # 160 samples
    seg = b.push(_control(p.CTRL_CAPTURE_STOP))
    with wave.open(BytesIO(seg.to_wav_bytes()), "rb") as w:
        assert w.getnchannels() == 1
        assert w.getsampwidth() == 2
        assert w.getframerate() == p.SAMPLE_RATE_HZ
        assert w.getnframes() == 160
