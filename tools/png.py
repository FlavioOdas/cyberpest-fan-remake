"""Minimal indexed-colour PNG writer (stdlib only)."""
import struct
import zlib


def _chunk(tag, data):
    return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)


def write_indexed(path, width, height, pixels, palette, transparent=None):
    raw = b"".join(b"\0" + bytes(pixels[y * width:(y + 1) * width]) for y in range(height))
    out = b"\x89PNG\r\n\x1a\n"
    out += _chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 3, 0, 0, 0))
    out += _chunk(b"PLTE", b"".join(bytes(c) for c in palette[:256]))
    if transparent is not None:
        out += _chunk(b"tRNS", b"\xff" * transparent + b"\0")
    out += _chunk(b"IDAT", zlib.compress(raw, 6))
    out += _chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(out)


def write_rgb(path, width, height, pixels):
    if len(pixels) != width * height * 3:
        raise ValueError("RGB pixel count does not match dimensions")
    raw = b"".join(b"\0" + bytes(pixels[y * width * 3:(y + 1) * width * 3]) for y in range(height))
    out = b"\x89PNG\r\n\x1a\n"
    out += _chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    out += _chunk(b"IDAT", zlib.compress(raw, 6))
    out += _chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(out)
