#!/usr/bin/env python3
"""Encode and pace a raw PCM16LE/8000/mono file like the future N70 sender."""

import argparse
import math
import os
from pathlib import Path
import socket
import struct
import tempfile
import time

from common import Codec, HEADER, MASK, RATE, SAMPLES, load_library, valid_token
from config import DEFAULT_CONFIG, load_settings


def send(path, host, port, token, library=None):
    if not valid_token(token):
        raise ValueError("Set C2_PUBLISH_TOKEN to the proxy's publishing token")
    codec = Codec(load_library(library))
    try:
        with open(path, "rb") as source, socket.create_connection((host, port), timeout=5) as sock:
            sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            sock.sendall(f"C2LIVE 1 {token} 3200\n".encode("ascii"))
            # Read exactly through LF; never discard bytes following a reply.
            reply = bytearray()
            while not reply.endswith(b"\n") and len(reply) < 192:
                value = sock.recv(1)
                if not value:
                    break
                reply.extend(value)
            if reply != b"OK\n":
                raise RuntimeError(f"Proxy rejected publisher: {reply!r}")
            start = time.monotonic()
            sequence = sample = total_samples = 0
            while True:
                raw = source.read(SAMPLES * 2 * 5)
                if not raw:
                    break
                if len(raw) % 2:
                    raise ValueError("Raw PCM has a partial 16-bit sample")
                raw = raw.ljust(((len(raw) + SAMPLES * 2 - 1) // (SAMPLES * 2)) * SAMPLES * 2, b"\0")
                frames = [codec.encode(raw[i:i + SAMPLES * 2])
                          for i in range(0, len(raw), SAMPLES * 2)]
                payload = b"".join(frames)
                total_samples += len(frames) * SAMPLES
                # Send when the corresponding microphone samples would exist.
                delay = start + total_samples / RATE - time.monotonic()
                if delay > 0:
                    time.sleep(delay)
                sock.sendall(HEADER.pack(sequence, sample, len(frames), len(payload)) + payload)
                sequence = (sequence + 1) & MASK
                sample = total_samples & MASK
    finally:
        codec.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pcm", nargs="?", help="Headerless signed PCM16LE, 8000 Hz, mono")
    parser.add_argument("--config", type=Path, help="Read server's TOML configuration and publishing token")
    parser.add_argument("--tone", type=float, metavar="SECONDS", help="Send a generated test tone for up to 60 seconds")
    parser.add_argument("--host", help="Server address (default: 127.0.0.1)")
    parser.add_argument("--port", type=int, help="TCP ingress port (default: 7778)")
    args = parser.parse_args()
    if (args.pcm is None) == (args.tone is None):
        parser.error("Provide a PCM file or --tone SECONDS")
    if args.tone is not None and not 0 < args.tone <= 60:
        parser.error("--tone must be greater than zero and at most 60 seconds")
    try:
        settings = load_settings(args.config) if args.config or DEFAULT_CONFIG.exists() else None
        token = settings.publish_token if settings else os.environ.get("C2_PUBLISH_TOKEN", "")
        port = args.port if args.port is not None else settings.tcp_port if settings else 7778
        # A server bind address such as 0.0.0.0 is not a remote destination.
        host = args.host or "127.0.0.1"
        library = settings.codec2_lib if settings else None
        if args.pcm:
            send(args.pcm, host, port, token, library)
        else:
            with tempfile.TemporaryDirectory() as directory:
                path = Path(directory) / "tone.pcm"
                with path.open("wb") as output:
                    for sample in range(max(1, int(RATE * args.tone))):
                        # Alternate tone and silence; this is a transport test,
                        # not a representative speech-quality evaluation.
                        amplitude = 7000 if sample % RATE < RATE * 0.7 else 0
                        output.write(struct.pack("<h", int(amplitude * math.sin(2 * math.pi * 220 * sample / RATE))))
                send(path, host, port, token, library)
    except (OSError, ValueError, RuntimeError) as exc:
        parser.exit(2, f"Error: {exc}\n")


if __name__ == "__main__":
    main()
