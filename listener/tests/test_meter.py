from muninn_listener import protocol as p
from muninn_listener.meter import MeterDisplay, MeterReading, parse_meter
from muninn_listener.pipeline import Pipeline
from muninn_listener.whisper_runner import MockWhisper


def _meter_frame(peak_l, peak_r, clip_l=False, clip_r=False):
    import struct

    clip = (0x01 if clip_l else 0) | (0x02 if clip_r else 0)
    payload = struct.pack("<HHB", peak_l, peak_r, clip)
    return p.encode(p.TYPE_METER, 0, 0, 0, payload)


def test_parse_meter_roundtrip():
    frame = _meter_frame(1234, 5678, clip_l=False, clip_r=True)
    parsed = p.FrameParser().feed(frame)[0]
    assert parsed.type == p.TYPE_METER
    r = parse_meter(parsed.payload)
    assert r == MeterReading(1234, 5678, False, True)


def test_meter_display_bar_scales():
    d = MeterDisplay(full_scale=1000, width=10)
    # Half scale -> half-filled bar; clip on the voice channel shows CLIP.
    line = d.render(MeterReading(peak_l=500, peak_r=1000, clip_l=False, clip_r=True))
    assert "Program [#####-----]   500" in line
    assert "You [##########] CLIP" in line


def test_pipeline_routes_meter_to_callback(tmp_path):
    seen = []
    pipe = Pipeline(MockWhisper(), out_dir=tmp_path, on_meter=seen.append)
    written = pipe.feed(_meter_frame(100, 200))
    assert written == []  # meter frames don't produce transcripts
    assert seen == [MeterReading(100, 200, False, False)]
    assert list(tmp_path.iterdir()) == []  # nothing written


def test_pipeline_ignores_meter_without_callback(tmp_path):
    pipe = Pipeline(MockWhisper(), out_dir=tmp_path)  # no on_meter
    assert pipe.feed(_meter_frame(1, 2)) == []
