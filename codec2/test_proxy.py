"""Protocol and integration checks against real libcodec2 and local sockets."""

import asyncio
from dataclasses import replace
import math
import os
from pathlib import Path
import json
import struct
import sys
import tempfile
import time
import unittest

from aiohttp import ClientSession, WSServerHandshakeError, WSMsgType, web

from common import Codec, HEADER, MASK, SAMPLES, load_library, validate_header
from config import Settings, load_settings, write_config
from proxy import INGRESS_KEY, Hub, create_app
from send_pcm import send

PUBLISH = "p" * 32
LISTEN = "l" * 32


class ConfigTests(unittest.TestCase):
    def test_private_config_roundtrip_and_no_overwrite(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "settings.toml"
            write_config(path)
            settings = load_settings(path, environ={})
            original = path.read_bytes()
            self.assertNotEqual(settings.publish_token, settings.listen_token)
            self.assertNotIn(settings.publish_token, repr(settings))
            if os.name == "posix":
                self.assertEqual(path.stat().st_mode & 0o777, 0o600)
            with self.assertRaises(FileExistsError):
                write_config(path)
            self.assertEqual(path.read_bytes(), original)
            override = load_settings(path, environ={"C2_TCP_PORT": "9000", "C2_MAX_VIEWERS": "2"})
            self.assertEqual((override.tcp_port, override.max_viewers), (9000, 2))

    def test_invalid_settings_and_unknown_keys(self):
        settings = Settings(publish_token=PUBLISH, listen_token=LISTEN)
        for values in ({"tcp_port": True}, {"http_port": 65536}, {"max_viewers": 0},
                       {"viewer_origin": "https://example.com/"},
                       {"publish_token": LISTEN}, {"listen_token": "short"}):
            with self.subTest(values=tuple(values)), self.assertRaises(ValueError):
                replace(settings, **values).validate()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "settings.toml"
            with self.assertRaises(ValueError):
                load_settings(path, environ={})
            path.write_text("typo = 123\n")
            with self.assertRaisesRegex(ValueError, "Unknown configuration keys"):
                load_settings(path, environ={})


class ProtocolTests(unittest.TestCase):
    def test_invalid_lengths_and_backwards_timestamps(self):
        for packet in (HEADER.pack(0, 0, 0, 0), HEADER.pack(0, 0, 11, 88),
                       HEADER.pack(0, 0, 5, 39), HEADER.pack(0, 1, 5, 40)):
            with self.assertRaises(ValueError):
                validate_header(packet, 0, 0)
        with self.assertRaises(ValueError):
            validate_header(HEADER.pack(1, 0, 5, 40), 1, 800)

    def test_modular_wrap_and_forward_gap(self):
        self.assertFalse(validate_header(HEADER.pack(0, 64, 1, 8), 0, 64)[-1])
        self.assertTrue(validate_header(HEADER.pack(3, 1600, 1, 8), 2, 800)[-1])
        self.assertTrue(validate_header(HEADER.pack(0, 160, 1, 8), MASK, MASK - 159)[-1])


class IntegrationTests(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        self.lib = load_library()
        self.hub = Hub(self.lib, PUBLISH, LISTEN)
        app = create_app(self.hub, ("127.0.0.1", 0))
        self.runner = web.AppRunner(app, shutdown_timeout=0.1)
        await self.runner.setup()
        self.tcp = app[INGRESS_KEY]
        self.port = self.tcp.sockets[0].getsockname()[1]
        site = web.TCPSite(self.runner, "127.0.0.1", 0)
        await site.start()
        self.origin = f"http://127.0.0.1:{site._server.sockets[0].getsockname()[1]}"
        self.client = ClientSession()
        self.sockets = []

    async def asyncTearDown(self):
        for sock in self.sockets:
            await sock.close()
        await self.client.close()
        await self.runner.cleanup()

    async def viewer(self, token=LISTEN):
        ws = await self.client.ws_connect(self.origin + "/listen", origin=self.origin)
        self.sockets.append(ws)
        await ws.send_json({"token": token})
        return ws

    async def publisher(self, token=PUBLISH):
        reader, writer = await asyncio.open_connection("127.0.0.1", self.port)
        writer.write(f"C2LIVE 1 {token} 3200\n".encode())
        await writer.drain()
        return reader, writer, await asyncio.wait_for(reader.readline(), 2)

    async def pcm(self, ws):
        while True:
            message = await ws.receive(timeout=2)
            if message.type == WSMsgType.BINARY:
                return message.data
            self.assertEqual(message.type, WSMsgType.TEXT)

    def encoded(self):
        codec = Codec(self.lib)
        try:
            frames = []
            for frame in range(5):
                pcm = struct.pack("<160h", *(int(9000 * math.sin(2 * math.pi * 220 *
                                      (frame * SAMPLES + i) / 8000)) for i in range(SAMPLES)))
                frames.append(codec.encode(pcm))
            return b"".join(frames)
        finally:
            codec.close()

    async def test_fragmented_ingress_two_listeners_and_gap(self):
        a, b = await self.viewer(), await self.viewer()
        self.assertFalse((await a.receive_json())["online"])
        await b.receive_json()
        reader, writer, reply = await self.publisher()
        self.assertEqual(reply, b"OK\n")
        payload = self.encoded()
        packet = HEADER.pack(0, 0, 5, len(payload)) + payload
        # Deliberately split TCP across both header and payload boundaries.
        for part in (packet[:3], packet[3:14], packet[14:]):
            writer.write(part)
            await writer.drain()
            await asyncio.sleep(0.01)
        pcm_a, pcm_b = await self.pcm(a), await self.pcm(b)
        self.assertEqual(pcm_a, pcm_b)
        self.assertEqual(HEADER.unpack(pcm_a[:12]), (0, 0, 800, 0))
        self.assertEqual(len(pcm_a), 1612)
        self.assertGreater(max(abs(x) for x in struct.unpack("<800h", pcm_a[12:])), 0)
        # Decoder synthesis uses process-global pseudo-random phase state in
        # upstream 1.2.0. A second decoder isn't a bit-exact independent oracle.
        writer.write(HEADER.pack(2, 1600, 5, 40) + payload)
        await writer.drain()
        self.assertEqual(HEADER.unpack((await self.pcm(a))[:12]), (2, 1600, 800, 1))
        writer.close()
        await writer.wait_closed()

    async def test_auth_busy_and_bad_frame_cleanup(self):
        ws = await self.viewer("x" * 32)
        self.assertEqual((await ws.receive(timeout=2)).type, WSMsgType.CLOSE)
        self.assertEqual(ws.close_code, 1008)
        reader, writer, reply = await self.publisher("x" * 32)
        self.assertEqual(reply, b"ERR auth\n")
        writer.close()
        await writer.wait_closed()
        reader, writer, reply = await self.publisher()
        self.assertEqual(reply, b"OK\n")
        reader2, writer2, reply = await self.publisher()
        self.assertEqual(reply, b"ERR busy\n")
        writer2.close()
        await writer2.wait_closed()
        writer.write(HEADER.pack(0, 0, 5, 39))
        await writer.drain()
        self.assertEqual(await asyncio.wait_for(reader.read(1), 2), b"")
        writer.close()
        await writer.wait_closed()
        # Releasing a rejected publisher makes the slot available immediately.
        reader3, writer3, reply = await self.publisher()
        self.assertEqual(reply, b"OK\n")
        writer3.close()
        await writer3.wait_closed()

    async def test_slow_viewer_queue_is_bounded(self):
        queue = asyncio.Queue(maxsize=8)
        self.hub.viewers.add(queue)
        try:
            for i in range(100):
                self.hub.broadcast(bytes([i]))
            self.assertLessEqual(queue.qsize(), 8)
            values = []
            while not queue.empty():
                values.append(queue.get_nowait())
            self.assertEqual(values[0], {"event": "discontinuity"})
            self.assertEqual(values[-1], bytes([99]))
        finally:
            self.hub.viewers.discard(queue)

    async def test_desktop_sender_pacing_and_final_frame_padding(self):
        ws = await self.viewer()
        await ws.receive_json()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "speech.pcm"
            path.write_bytes(struct.pack("<1000h", *(int(5000 * math.sin(i / 8)) for i in range(1000))))
            start = time.monotonic()
            sending = asyncio.create_task(asyncio.to_thread(send, path, "127.0.0.1", self.port, PUBLISH))
            try:
                first, last = await self.pcm(ws), await self.pcm(ws)
                await sending
            finally:
                await sending
        self.assertEqual(HEADER.unpack(first[:12]), (0, 0, 800, 0))
        self.assertEqual(HEADER.unpack(last[:12]), (1, 800, 320, 0))
        self.assertGreaterEqual(time.monotonic() - start, 0.13)

    async def test_configured_cli_check_and_tone_publisher(self):
        ws = await self.viewer()
        await ws.receive_json()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "settings.toml"
            values = {"publish_token": PUBLISH, "listen_token": LISTEN,
                      "tcp_port": self.port, "codec2_lib": self.lib._name}
            path.write_text("\n".join(f"{key} = {json.dumps(value)}" for key, value in values.items()))
            root = Path(__file__).resolve().parent
            check = await asyncio.create_subprocess_exec(
                sys.executable, str(root / "proxy.py"), "--config", str(path), "--check",
                stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE)
            output, errors = await asyncio.wait_for(check.communicate(), 5)
            self.assertEqual(check.returncode, 0, errors.decode())
            self.assertIn(b"OK: configuration and Codec 2", output)
            sender = await asyncio.create_subprocess_exec(
                sys.executable, str(root / "send_pcm.py"), "--config", str(path), "--tone", "0.1",
                stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE)
            try:
                audio = await self.pcm(ws)
                _output, errors = await asyncio.wait_for(sender.communicate(), 5)
                self.assertEqual(sender.returncode, 0, errors.decode())
                self.assertEqual(HEADER.unpack(audio[:12]), (0, 0, 800, 0))
            finally:
                if sender.returncode is None:
                    sender.kill()
                    await sender.communicate()

    async def test_http_page_health_and_origin(self):
        async with self.client.get(self.origin + "/healthz") as response:
            self.assertEqual(await response.json(), {"ok": True, "service": "c2live", "protocol": 1})
        async with self.client.get(self.origin + "/") as response:
            self.assertEqual(response.status, 200)
            self.assertIn("N70 live audio", await response.text())
        with self.assertRaises(WSServerHandshakeError) as error:
            await self.client.ws_connect(self.origin + "/listen", origin="https://elsewhere.example")
        self.assertEqual(error.exception.status, 403)

    async def test_unauthenticated_listener_reserves_capacity(self):
        self.hub.max_viewers = 1
        pending = await self.client.ws_connect(self.origin + "/listen", origin=self.origin)
        self.sockets.append(pending)
        with self.assertRaises(WSServerHandshakeError) as error:
            await self.viewer()
        self.assertEqual(error.exception.status, 503)
        await pending.send_json({"token": LISTEN})
        self.assertFalse((await pending.receive_json())["online"])

    async def test_shutdown_closes_publisher_and_browser(self):
        ws = await self.viewer()
        await ws.receive_json()
        reader, writer, reply = await self.publisher()
        self.assertEqual(reply, b"OK\n")
        await ws.receive_json()  # stream_start
        await ws.receive_json()  # status
        closing = asyncio.create_task(self.runner.cleanup())
        try:
            while True:
                message = await ws.receive(timeout=2)
                if message.type != WSMsgType.TEXT:
                    break
            self.assertEqual(message.type, WSMsgType.CLOSE)
            self.assertEqual(ws.close_code, 1001)
            self.assertEqual(await asyncio.wait_for(reader.read(), 2), b"")
        finally:
            await closing
            writer.close()
            await writer.wait_closed()
        self.assertTrue(self.hub.closing)
        self.assertFalse(self.hub.tasks)
        self.assertFalse(self.hub.viewers)

    async def test_truncated_record_releases_slot(self):
        reader, writer, reply = await self.publisher()
        self.assertEqual(reply, b"OK\n")
        writer.write(HEADER.pack(0, 0, 5, 40) + b"\x00" * 3)
        await writer.drain()
        writer.write_eof()
        self.assertEqual(await asyncio.wait_for(reader.read(), 2), b"")
        writer.close()
        await writer.wait_closed()
        reader, writer, reply = await self.publisher()
        self.assertEqual(reply, b"OK\n")
        writer.close()
        await writer.wait_closed()


if __name__ == "__main__":
    unittest.main()
