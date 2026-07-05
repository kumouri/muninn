"""USB-CDC transport — the v1 default. Reads frames from the device's serial port (pyserial)."""

from __future__ import annotations

from ..hotkey import DeviceLink
from ..pipeline import Pipeline


def serve_serial(port: str, baud: int, pipeline: Pipeline, link: DeviceLink | None = None) -> None:
    try:
        import serial  # optional dependency: pip install "muninn-listener[serial]"
    except ImportError as e:  # pragma: no cover - environment dependent
        raise SystemExit(
            "pyserial is required for the USB-CDC transport: pip install 'muninn-listener[serial]'"
        ) from e

    ser = serial.Serial(port, baud, timeout=0.1)
    print(f"[muninn] listening on {port} @ {baud} (usb-cdc)")
    if link is not None:
        link.set_sender(ser.write)  # hotkey can write CONTROL frames back over the same port
    try:
        while True:
            data = ser.read(4096)
            if data:
                for p in pipeline.feed(data):
                    print(f"[muninn] transcript -> {p}")
    finally:
        if link is not None:
            link.clear()
        ser.close()
