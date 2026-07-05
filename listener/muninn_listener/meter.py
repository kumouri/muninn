"""Per-channel level meter surfaced from the device (TYPE_METER frames) to the terminal.

Lets you set mixer levels before hitting record: a live two-channel VU meter (program + voice).
"""

from __future__ import annotations

import struct
import sys
from dataclasses import dataclass

_METER = struct.Struct("<HHB")  # peak_L, peak_R, clip bits


@dataclass(frozen=True)
class MeterReading:
    peak_l: int  # program
    peak_r: int  # your voice
    clip_l: bool
    clip_r: bool


def parse_meter(payload: bytes) -> MeterReading:
    peak_l, peak_r, clip = _METER.unpack_from(payload)
    return MeterReading(peak_l, peak_r, bool(clip & 0x01), bool(clip & 0x02))


class MeterDisplay:
    """Renders a two-channel bar meter and prints it in place (carriage return)."""

    def __init__(self, full_scale: int = 6000, width: int = 20) -> None:
        self.full_scale = full_scale
        self.width = width

    def _bar(self, peak: int, clip: bool) -> str:
        filled = (
            min(self.width, round(peak / self.full_scale * self.width)) if self.full_scale else 0
        )
        bar = "#" * filled + "-" * (self.width - filled)
        tag = "CLIP" if clip else f"{peak:5d}"
        return f"[{bar}] {tag}"

    def render(self, r: MeterReading) -> str:
        return f"Program {self._bar(r.peak_l, r.clip_l)}   You {self._bar(r.peak_r, r.clip_r)}"

    def update(self, r: MeterReading) -> None:
        sys.stdout.write("\r" + self.render(r))
        sys.stdout.flush()
