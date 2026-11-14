#!/usr/bin/env python3
"""Single-publisher Codec 2 TCP ingress -> PCM WebSocket listeners."""

import asyncio
import argparse
import contextlib
import hmac
import json
import logging
from pathlib import Path

from aiohttp import WSMsgType, web

from common import (Codec, FRAME_BYTES, HEADER, MASK, RATE, SAMPLES,
                    load_library, valid_token, validate_header)
from config import DEFAULT_CONFIG, load_settings, write_config

log = logging.getLogger("c2live")
ROOT = Path(__file__).resolve().parent
HUB_KEY = web.AppKey("hub", object)
INGRESS_KEY = web.AppKey("ingress", object)


class Hub:
    def __init__(self, lib, publish_token, listen_token, origin=None,
                 max_tcp_connections=16, max_viewers=8):
        self.lib = lib
        self.publish_token = publish_token
        self.listen_token = listen_token
        self.origin = origin
        self.publisher = None
        self.viewers = set()
        self.writers = set()
        self.tasks = set()
        self.websockets = set()
        self.max_tcp_connections = max_tcp_connections
        self.max_viewers = max_viewers
        self.closing = False

    async def close(self):
        self.closing = True
        tasks = list(self.tasks)
        for task in tasks:
            task.cancel()
        await asyncio.gather(*tasks, return_exceptions=True)
        for writer in list(self.writers):
            writer.close()
        await asyncio.gather(*(ws.close(code=1001, message=b"Server shutdown")
                               for ws in list(self.websockets)), return_exceptions=True)

    def status(self):
        return {"event": "status", "online": self.publisher is not None,
                "rate": RATE, "format": "s16le", "channels": 1}

    def broadcast(self, item):
        # A slow listener cannot stop ingress or delay other listeners.
        for queue in self.viewers:
            if queue.full():
                while not queue.empty():
                    queue.get_nowait()
                queue.put_nowait({"event": "discontinuity"})
            queue.put_nowait(item)

    async def ingest(self, reader, writer):
        task = asyncio.current_task()
        self.tasks.add(task)
        self.writers.add(writer)
        codec = None
        owned = False
        try:
            if self.closing or len(self.writers) > self.max_tcp_connections:
                return
            line = await asyncio.wait_for(reader.readline(), 10)
            fields = line.decode("ascii").rstrip("\n").split(" ")
            if (len(line) > 192 or not line.endswith(b"\n") or len(fields) != 4 or
                    fields[0:2] != ["C2LIVE", "1"] or fields[3] != "3200"):
                raise ValueError("Invalid handshake")
            if not valid_token(fields[2]) or not hmac.compare_digest(fields[2], self.publish_token):
                writer.write(b"ERR auth\n")
                await asyncio.wait_for(writer.drain(), 2)
                return
            if self.publisher is not None:
                writer.write(b"ERR busy\n")
                await asyncio.wait_for(writer.drain(), 2)
                return
            codec = Codec(self.lib)
            self.publisher = writer
            owned = True
            log.info("Publisher connected (%s)", writer.get_extra_info("peername"))
            self.broadcast({"event": "stream_start"})
            self.broadcast(self.status())
            writer.write(b"OK\n")
            await asyncio.wait_for(writer.drain(), 2)
            sequence_next = sample_next = 0
            first = True
            while True:
                header = await asyncio.wait_for(reader.readexactly(HEADER.size), 15)
                sequence, sample, count, size, gap = validate_header(
                    header, sequence_next, sample_next)
                if first and (sequence != 0 or sample != 0):
                    raise ValueError("A session must start at sequence=0, sample=0")
                first = False
                payload = await asyncio.wait_for(reader.readexactly(size), 5)
                if gap:
                    codec.close()
                    codec = Codec(self.lib)
                pcm = b"".join(codec.decode(payload[i:i + FRAME_BYTES])
                               for i in range(0, size, FRAME_BYTES))
                # Same 12-byte shape, but fields 3/4 are sample count / flags.
                self.broadcast(HEADER.pack(sequence, sample, count * SAMPLES, int(gap)) + pcm)
                sequence_next = (sequence + 1) & MASK
                sample_next = (sample + count * SAMPLES) & MASK
                # Buffered reads may complete synchronously during a TCP burst.
                # Give listener tasks a chance to run between decoded records.
                await asyncio.sleep(0)
        except (asyncio.IncompleteReadError, ConnectionError, asyncio.TimeoutError):
            pass
        except (ValueError, UnicodeError) as exc:
            log.info("Rejected publisher input: %s", exc)
        except Exception:
            log.exception("Publisher failed")
        finally:
            if codec is not None:
                codec.close()
            if owned:
                self.publisher = None
                self.broadcast(self.status())
                log.info("Publisher disconnected")
            self.writers.discard(writer)
            self.tasks.discard(task)
            writer.close()
            with contextlib.suppress(ConnectionError, asyncio.TimeoutError):
                await asyncio.wait_for(writer.wait_closed(), 2)

    async def listen(self, request):
        allowed = self.origin or f"{request.scheme}://{request.host}"
        if request.headers.get("Origin") != allowed:
            raise web.HTTPForbidden(text="Origin not allowed")
        if self.closing or len(self.websockets) >= self.max_viewers:
            raise web.HTTPServiceUnavailable(text="Listener limit reached")
        ws = web.WebSocketResponse(timeout=2, heartbeat=20, max_msg_size=1024)
        # Reserve before the first await, including sockets awaiting auth.
        self.websockets.add(ws)
        sender = None
        queue = None
        try:
            await ws.prepare(request)
            message = await asyncio.wait_for(ws.receive(), 5)
            auth = json.loads(message.data) if message.type == WSMsgType.TEXT else {}
            token = auth.get("token") if isinstance(auth, dict) else None
            if not valid_token(token) or not hmac.compare_digest(token, self.listen_token):
                await ws.close(code=1008, message=b"Authentication failed")
                return ws
            # Recheck after authentication: multiple sockets may be authenticating.
            if len(self.viewers) >= self.max_viewers:
                await ws.close(code=1013, message=b"Listener limit reached")
                return ws
            queue = asyncio.Queue(maxsize=8)
            self.viewers.add(queue)
            log.info("Listener connected (%d active)", len(self.viewers))
            queue.put_nowait(self.status())

            async def pump():
                try:
                    while True:
                        item = await queue.get()
                        operation = ws.send_bytes(item) if isinstance(item, bytes) else ws.send_json(item)
                        await asyncio.wait_for(operation, 2)
                except (ConnectionError, asyncio.TimeoutError):
                    pass
                finally:
                    await ws.close()

            sender = asyncio.create_task(pump())
            async for _message in ws:
                # After authentication this is a receive-only audio connection.
                await ws.close(code=1008, message=b"Unexpected client message")
        except (ValueError, TypeError, asyncio.TimeoutError):
            await ws.close(code=1008, message=b"Invalid authentication")
        finally:
            self.websockets.discard(ws)
            if queue is not None:
                self.viewers.discard(queue)
                log.info("Listener disconnected (%d active)", len(self.viewers))
            if sender is not None:
                sender.cancel()
                with contextlib.suppress(asyncio.CancelledError):
                    await sender
        return ws


def create_app(hub, tcp_address=None):
    async def index(_request):
        return web.FileResponse(ROOT / "listener.html", headers={"Cache-Control": "no-store"})

    async def health(_request):
        return web.json_response({"ok": not hub.closing, "service": "c2live", "protocol": 1},
                                 headers={"Cache-Control": "no-store"})

    async def ingress_context(app):
        server = await asyncio.start_server(hub.ingest, *tcp_address, limit=192)
        app[INGRESS_KEY] = server
        log.info("TCP ingress on %s", server.sockets[0].getsockname())
        try:
            yield
        finally:
            server.close()
            await server.wait_closed()

    async def shutdown(app):
        if INGRESS_KEY in app:
            app[INGRESS_KEY].close()
        await hub.close()
        # Recent asyncio versions wait for accepted clients too, so close the
        # publisher tasks before waiting for the listening server to finish.
        if INGRESS_KEY in app:
            await app[INGRESS_KEY].wait_closed()

    app = web.Application()
    app[HUB_KEY] = hub
    app.router.add_get("/", index)
    app.router.add_get("/listen", hub.listen)
    app.router.add_get("/healthz", health)
    if tcp_address is not None:
        app.cleanup_ctx.append(ingress_context)
    app.on_shutdown.append(shutdown)
    return app


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path, help=f"TOML file (default: {DEFAULT_CONFIG})")
    parser.add_argument("--init-config", type=Path, metavar="PATH", help="Create private configuration with new tokens, then exit")
    parser.add_argument("--check", action="store_true", help="Validate configuration and native codec without opening ports")
    args = parser.parse_args(argv)
    try:
        if args.init_config is not None:
            if args.config or args.check:
                parser.error("--init-config cannot be combined with --config or --check")
            write_config(args.init_config)
            print(f"Created {args.init_config}. Enter its listen_token in the browser; copy publish_token to the phone.")
            return
        settings = load_settings(args.config)
        lib = load_library(settings.codec2_lib)
        codec = Codec(lib)
        try:
            if len(codec.decode(codec.encode(bytes(SAMPLES * 2)))) != SAMPLES * 2:
                raise RuntimeError("Native codec self-check failed")
        finally:
            codec.close()
    except (OSError, ValueError, RuntimeError) as exc:
        parser.exit(2, f"Error: {exc}\n")
    if args.check:
        print("OK: configuration and Codec 2 3200 encode/decode; protocol C2LIVE 1")
        return
    hub = Hub(lib, settings.publish_token, settings.listen_token, settings.viewer_origin or None,
              settings.max_tcp_connections, settings.max_viewers)
    app = create_app(hub, (settings.tcp_host, settings.tcp_port))
    log.info("Browser listener: http://%s:%d/", settings.http_host, settings.http_port)
    web.run_app(app, host=settings.http_host, port=settings.http_port,
                shutdown_timeout=2, access_log=None)


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)
    main()
