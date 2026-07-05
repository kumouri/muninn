"""Wi-Fi TCP transport — the 'wireless PC' path. Receives frames from the device over TCP."""

from __future__ import annotations

import asyncio

from ..hotkey import DeviceLink
from ..pipeline import Pipeline


async def serve_tcp(
    host: str, port: int, pipeline: Pipeline, link: DeviceLink | None = None
) -> None:
    async def handle(reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
        peer = writer.get_extra_info("peername")
        print(f"[muninn] device connected: {peer}")
        loop = asyncio.get_running_loop()
        if link is not None:
            # Let a hotkey thread push CONTROL frames back to this device, thread-safely.
            link.set_sender(lambda frame: loop.call_soon_threadsafe(writer.write, frame))
        try:
            while True:
                data = await reader.read(4096)
                if not data:
                    break
                # Transcription can block (subprocess); keep the event loop free.
                paths = await loop.run_in_executor(None, pipeline.feed, data)
                for p in paths:
                    print(f"[muninn] transcript -> {p}")
        finally:
            if link is not None:
                link.clear()
            print(f"[muninn] device disconnected: {peer}")
            writer.close()

    server = await asyncio.start_server(handle, host, port)
    addrs = ", ".join(str(s.getsockname()) for s in server.sockets)
    print(f"[muninn] listening on {addrs} (tcp)")
    async with server:
        await server.serve_forever()
