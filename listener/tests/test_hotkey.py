from muninn_listener import protocol as p
from muninn_listener.assembler import SegmentBuilder
from muninn_listener.hotkey import DeviceLink, capture_control_frames
from muninn_listener.protocol import FrameParser


def test_device_link_no_sender_returns_false():
    link = DeviceLink()
    assert link.connected is False
    assert link.send_control(p.CTRL_CAPTURE_START) is False


def test_device_link_sends_encoded_control_frames():
    sent = []
    link = DeviceLink()
    link.set_sender(sent.append)
    assert link.connected is True

    link.toggle(True)  # start
    link.toggle(False)  # stop

    frames = FrameParser().feed(b"".join(sent))
    assert [f.type for f in frames] == [p.TYPE_CONTROL, p.TYPE_CONTROL]
    assert frames[0].payload[0] == p.CTRL_CAPTURE_START
    assert frames[1].payload[0] == p.CTRL_CAPTURE_STOP


def test_device_link_clear_disconnects():
    link = DeviceLink()
    link.set_sender(lambda b: None)
    link.clear()
    assert link.connected is False
    assert link.send_control(p.CTRL_CAPTURE_STOP) is False


def test_hotkey_frames_drive_the_assembler():
    # The frames a hotkey emits should bound a capture on the receiving side.
    start, stop = capture_control_frames()
    audio = p.encode(p.TYPE_AUDIO, p.FLAG_CAPTURING, 5, 0, b"\x01\x00\x02\x00")

    builder = SegmentBuilder()
    parser = FrameParser()
    seg = None
    for frame in parser.feed(start + audio + stop):
        result = builder.push(frame)
        if result is not None:
            seg = result
    assert seg is not None
    assert seg.pcm == b"\x01\x00\x02\x00"
