"""USB-CDC transport — the v1 default. Reads frames from the device's serial port (pyserial)."""

from __future__ import annotations

from ..pipeline import Pipeline


def serve_serial(port: str, baud: int, pipeline: Pipeline) -> None:
    try:
        import serial  # optional dependency: pip install "muninn-listener[serial]"
    except ImportError as e:  # pragma: no cover - environment dependent
        raise SystemExit(
            "pyserial is required for the USB-CDC transport: pip install 'muninn-listener[serial]'"
        ) from e

    ser = serial.Serial(port, baud, timeout=0.1)
    print(f"[muninn] listening on {port} @ {baud} (usb-cdc)")
    try:
        while True:
            data = ser.read(4096)
            if data:
                for p in pipeline.feed(data):
                    print(f"[muninn] transcript -> {p}")
    finally:
        ser.close()
