"""PC-side capture trigger — toggle capture from the computer instead of the device button.

The device honors inbound CONTROL frames (see firmware `handleInbound`), so the listener can drive
capture remotely over the same connection. `DeviceLink` is the thread-safe handle a hotkey thread
uses to send those frames back to the currently-connected device.

Two triggers are provided:
  * `run_stdin_toggle` — press Enter to toggle (portable, no dependencies).
  * `run_global_hotkey` — a real global hotkey, needs the optional `keyboard` package.
"""

from __future__ import annotations

import threading
from collections.abc import Callable

from . import protocol as p

# Sends encoded bytes to the device; supplied by the active transport.
Sender = Callable[[bytes], None]


class DeviceLink:
    """Thread-safe handle for sending CONTROL frames back to the connected device."""

    def __init__(self) -> None:
        self._lock = threading.Lock()
        self._sender: Sender | None = None
        self._seq = 0

    def set_sender(self, sender: Sender) -> None:
        with self._lock:
            self._sender = sender

    def clear(self) -> None:
        with self._lock:
            self._sender = None

    @property
    def connected(self) -> bool:
        with self._lock:
            return self._sender is not None

    def send_control(self, code: int) -> bool:
        """Encode and send one CONTROL frame. Returns False if no device is connected."""
        with self._lock:
            if self._sender is None:
                return False
            frame = p.encode(p.TYPE_CONTROL, 0, self._seq, 0, bytes([code]))
            self._seq = (self._seq + 1) & 0xFFFFFFFF
            self._sender(frame)
            return True

    def toggle(self, capturing: bool) -> None:
        self.send_control(p.CTRL_CAPTURE_START if capturing else p.CTRL_CAPTURE_STOP)


def capture_control_frames(seq: int = 0) -> tuple[bytes, bytes]:
    """Return (start_frame, stop_frame) as encoded CONTROL frames (handy for tests/tools)."""
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


def run_global_hotkey(  # pragma: no cover - needs optional dep + interactive
    on_toggle: Callable[[bool], None], hotkey: str = "ctrl+alt+m"
) -> None:
    """Global hotkey toggle. Requires `pip install keyboard` (may need elevated privileges)."""
    try:
        import keyboard
    except ImportError as e:
        raise SystemExit(
            "global hotkey needs the 'keyboard' package: pip install keyboard "
            "(or use --hotkey stdin)"
        ) from e

    state = {"capturing": False}

    def _cb() -> None:
        state["capturing"] = not state["capturing"]
        on_toggle(state["capturing"])
        print("[muninn] capturing" if state["capturing"] else "[muninn] idle")

    keyboard.add_hotkey(hotkey, _cb)
    print(f"[muninn] global hotkey armed: {hotkey}")
    keyboard.wait()
