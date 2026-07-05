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
    return ap


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    runner = build_runner(args)
    pipeline = Pipeline(runner, out_dir=args.out)

    if args.transport == "tcp":
        from .transports.tcp import serve_tcp

        try:
            asyncio.run(serve_tcp(args.host, args.port, pipeline))
        except KeyboardInterrupt:
            print("\n[muninn] stopped.")
    else:
        from .transports.usb_cdc import serve_serial

        try:
            serve_serial(args.serial_port, args.baud, pipeline)
        except KeyboardInterrupt:
            print("\n[muninn] stopped.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
