"""Optional PC-side capture trigger.

In v1 the device's own button drives capture (it sets FLAG_CAPTURING and sends CONTROL frames).
This module is a convenience for setups where you want to toggle capture from the PC — e.g. driving
the loopback harness, or a future two-way transport that relays the toggle back to the device.

A global hotkey needs the optional `keyboard` package; without it, this falls back to pressing
Enter on stdin to toggle.
"""

from __future__ import annotations

from collections.abc import Callable

from . import protocol as p


def capture_control_frames(seq: int = 0) -> tuple[bytes, bytes]:
    """Return (start_frame, stop_frame) as encoded CONTROL frames."""
    start = p.encode(p.TYPE_CONTROL, 0, seq, 0, bytes([p.CTRL_CAPTURE_START]))
    stop = p.encode(p.TYPE_CONTROL, 0, seq + 1, 0, bytes([p.CTRL_CAPTURE_STOP]))
    return start, stop


def run_stdin_toggle(on_toggle: Callable[[bool], None]) -> None:  # pragma: no cover - interactive
    """Blocking loop: each Enter toggles capture and calls on_toggle(now_capturing)."""
    capturing = False
    print("[muninn] press Enter to start/stop capture (Ctrl-C to quit)")
    try:
        while True:
            input()
            capturing = not capturing
            on_toggle(capturing)
            print("[muninn] capturing" if capturing else "[muninn] idle")
    except (EOFError, KeyboardInterrupt):
        pass
