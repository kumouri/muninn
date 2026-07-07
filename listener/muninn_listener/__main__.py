"""Muninn listener CLI.

Examples:
    python -m muninn_listener --transport tcp --port 5140
    python -m muninn_listener --transport usb --serial-port COM7 --whisper-bin whisper-cli \
        --model models/ggml-base.en.bin
    python -m muninn_listener --transport tcp --mock          # no model, for smoke tests
"""

from __future__ import annotations

import argparse
import asyncio
import sys

from .pipeline import Pipeline
from .whisper_runner import build_runner


def build_parser() -> argparse.ArgumentParser:
    ap = argparse.ArgumentParser(prog="muninn-listener", description=__doc__)
    ap.add_argument("--transport", choices=["tcp", "usb"], default="tcp")
    ap.add_argument("--host", default="0.0.0.0", help="TCP bind host")
    ap.add_argument("--port", type=int, default=5140, help="TCP port")
    ap.add_argument(
        "--serial-port", default="COM7", help="USB-CDC serial port (e.g. COM7, /dev/ttyACM0)"
    )
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--out", default="transcripts", help="output directory for transcripts")

    # whisper.cpp
    ap.add_argument("--whisper-bin", default="whisper-cli", help="whisper.cpp CLI binary")
    ap.add_argument("--model", default="models/ggml-base.en.bin", help="ggml model path or name")
    ap.add_argument("--language", default="en")
    ap.add_argument(
        "--pywhispercpp", action="store_true", help="use the in-process pywhispercpp path"
    )
    ap.add_argument("--mock", action="store_true", help="mock transcription (no model) for testing")

    # remote capture trigger (sends CONTROL frames back to the device)
    ap.add_argument(
        "--hotkey",
        choices=["off", "stdin", "global"],
        default="off",
        help="PC-side capture toggle: 'stdin' (press Enter), 'global' (needs the keyboard package)",
    )
    ap.add_argument("--hotkey-combo", default="ctrl+alt+m", help="key combo for --hotkey global")

    ap.add_argument("--meter", action="store_true", help="show a live per-channel level meter")

    # desktop webcam mic as the voice channel (for the A2DP program-only front-end)
    ap.add_argument(
        "--desktop-mic",
        action="store_true",
        help="capture the desktop mic as the voice channel and diarize (needs the mic extra)",
    )
    ap.add_argument("--mic-device", default=None, help="sounddevice input device (name or index)")
    ap.add_argument(
        "--program-delay-ms",
        type=float,
        default=150.0,
        help="how far the Bluetooth program lags real time, for mic alignment",
    )
    return ap


def _start_hotkey(args, link) -> None:
    """Spawn the chosen hotkey trigger in a daemon thread, sending toggles to `link`."""
    import threading

    from .hotkey import run_global_hotkey, run_stdin_toggle

    target = run_global_hotkey if args.hotkey == "global" else run_stdin_toggle
    hk_args = (link.toggle, args.hotkey_combo) if args.hotkey == "global" else (link.toggle,)
    threading.Thread(target=target, args=hk_args, daemon=True).start()


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    runner = build_runner(args)

    on_meter = None
    if args.meter:
        from .meter import MeterDisplay

        on_meter = MeterDisplay().update

    mic_buffer = None
    desktop_mic = None
    if args.desktop_mic:
        from .mic import DesktopMic, MicBuffer

        mic_buffer = MicBuffer()
        desktop_mic = DesktopMic(mic_buffer, device=args.mic_device)
        desktop_mic.start()
        print("[muninn] desktop mic capturing (voice channel)")

    pipeline = Pipeline(
        runner,
        out_dir=args.out,
        on_meter=on_meter,
        mic_buffer=mic_buffer,
        program_delay_ms=args.program_delay_ms,
    )

    link = None
    if args.hotkey != "off":
        from .hotkey import DeviceLink

        link = DeviceLink()
        _start_hotkey(args, link)

    if args.transport == "tcp":
        from .transports.tcp import serve_tcp

        try:
            asyncio.run(serve_tcp(args.host, args.port, pipeline, link))
        except KeyboardInterrupt:
            print("\n[muninn] stopped.")
    else:
        from .transports.usb_cdc import serve_serial

        try:
            serve_serial(args.serial_port, args.baud, pipeline, link)
        except KeyboardInterrupt:
            print("\n[muninn] stopped.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
