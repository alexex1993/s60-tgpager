"""Codec 2 3200 binding and wire constants shared by the reference tools."""

import ctypes as C
import ctypes.util
import os
import re
import struct

RATE = 8000
SAMPLES = 160
FRAME_BYTES = 8
MAX_FRAMES = 10
HEADER = struct.Struct("!IIHH")
MASK = 0xFFFFFFFF
TOKEN_RE = re.compile(r"[A-Za-z0-9_-]{32,128}\Z")


def valid_token(value):
    return isinstance(value, str) and TOKEN_RE.fullmatch(value) is not None


def load_library(path=None):
    path = path or os.environ.get("CODEC2_LIB") or ctypes.util.find_library("codec2")
    if not path:
        raise RuntimeError("libcodec2 not found: install it or set CODEC2_LIB")
    lib = C.CDLL(path)
    lib.codec2_create.argtypes = [C.c_int]
    lib.codec2_create.restype = C.c_void_p
    lib.codec2_destroy.argtypes = [C.c_void_p]
    lib.codec2_destroy.restype = None
    for name in ("codec2_samples_per_frame", "codec2_bits_per_frame"):
        fn = getattr(lib, name)
        fn.argtypes = [C.c_void_p]
        fn.restype = C.c_int
    lib.codec2_encode.argtypes = [C.c_void_p, C.POINTER(C.c_ubyte), C.POINTER(C.c_short)]
    lib.codec2_encode.restype = None
    lib.codec2_decode.argtypes = [C.c_void_p, C.POINTER(C.c_short), C.POINTER(C.c_ubyte)]
    lib.codec2_decode.restype = None
    return lib


class Codec:
    """One native codec state per publisher; never share it across streams."""

    def __init__(self, lib):
        self.lib = lib
        self.state = lib.codec2_create(0)  # CODEC2_MODE_3200, not the bitrate.
        if not self.state:
            raise RuntimeError("codec2_create failed")
        if (lib.codec2_samples_per_frame(self.state) != SAMPLES or
                lib.codec2_bits_per_frame(self.state) != FRAME_BYTES * 8):
            self.close()
            raise RuntimeError("This prototype requires Codec 2 mode 3200")

    def encode(self, pcm_le):
        if len(pcm_le) != SAMPLES * 2:
            raise ValueError("Expected 160 signed PCM16 samples")
        speech = (C.c_short * SAMPLES)(*struct.unpack("<160h", pcm_le))
        bits = (C.c_ubyte * FRAME_BYTES)()
        self.lib.codec2_encode(self.state, bits, speech)
        return bytes(bits)

    def decode(self, bits):
        if len(bits) != FRAME_BYTES:
            raise ValueError("Expected one 8-byte Codec 2 frame")
        packed = (C.c_ubyte * FRAME_BYTES).from_buffer_copy(bits)
        speech = (C.c_short * SAMPLES)()
        self.lib.codec2_decode(self.state, speech, packed)
        return struct.pack("<160h", *speech)

    def close(self):
        if self.state:
            self.lib.codec2_destroy(self.state)
            self.state = None


def validate_header(header, expected_sequence, expected_sample):
    sequence, sample, count, size = HEADER.unpack(header)
    if not 1 <= count <= MAX_FRAMES or size != count * FRAME_BYTES:
        raise ValueError("Invalid frame count or payload length")
    # Unsigned modular arithmetic permits wraparound, rejects backwards jumps.
    seq_gap = (sequence - expected_sequence) & MASK
    sample_gap = (sample - expected_sample) & MASK
    if seq_gap >= 0x80000000 or sample_gap >= 0x80000000:
        raise ValueError("Sequence or timestamp moved backwards")
    if sample_gap % SAMPLES:
        raise ValueError("Timestamp gap is not a whole codec frame")
    return sequence, sample, count, size, bool(seq_gap or sample_gap)
