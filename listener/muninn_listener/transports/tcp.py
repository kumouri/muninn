"""Wi-Fi TCP transport — the 'wireless PC' path. Receives frames from the device over TCP."""

from __future__ import annotations

import asyncio

from ..pipeline import Pipeline


async def serve_tcp(host: str, port: int, pipeline: Pipeline) -> None:
    async def handle(reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
        peer = writer.get_extra_info("peername")
        print(f"[muninn] device connected: {peer}")
        loop = asyncio.get_running_loop()
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
            print(f"[muninn] device disconnected: {peer}")
            writer.close()

    server = await asyncio.start_server(handle, host, port)
    addrs = ", ".join(str(s.getsockname()) for s in server.sockets)
    print(f"[muninn] listening on {addrs} (tcp)")
    async with server:
        await server.serve_forever()
