"""Decoder for Autodesk Animator FLC/FLI animations (8-bit, as used inside PEST.ADM)."""
import struct

FRAME, PREFIX = 0xF1FA, 0xF100
COLOR_256, DELTA_FLC, COLOR_64, DELTA_FLI, BLACK, BYTE_RUN, FLI_COPY, PSTAMP = 4, 7, 11, 12, 13, 15, 16, 18


class Flc:
    def __init__(self, data):
        self.data = data
        (size, magic, self.frame_count, self.width, self.height, self.depth, self.flags,
         self.speed) = struct.unpack_from("<IHHHHHHI", data, 0)
        if magic not in (0xAF12, 0xAF11):
            raise ValueError("bad FLC magic 0x%04x" % magic)
        if magic == 0xAF11:            # FLI: speed is in 1/70 s jiffies, frames start at 128
            self.speed = self.speed * 1000 // 70
        self.oframe1 = struct.unpack_from("<I", data, 80)[0] if magic == 0xAF12 else 0
        self.palette = [(0, 0, 0)] * 256
        self.palette_set = False
        self.chunk_types = set()

    def frames(self):
        """Yield (pixels bytearray, palette list) for each frame, including the ring frame."""
        w, h, d = self.width, self.height, self.data
        pixels = bytearray(w * h)
        o = self.oframe1 or 128
        for _ in range(self.frame_count + 1):
            if o + 16 > len(d):
                break
            size, ctype, nchunks = struct.unpack_from("<IHH", d, o)
            if ctype == FRAME:
                c = o + 16
                for _ in range(nchunks):
                    csize, sub = struct.unpack_from("<IH", d, c)
                    self.chunk_types.add(sub)
                    self._apply(sub, d, c + 6, pixels)
                    c += csize
            o += max(size, 16)
            yield bytes(pixels), list(self.palette)

    def _apply(self, sub, d, p, px):
        w, h = self.width, self.height
        if sub in (COLOR_256, COLOR_64):
            scale = 1 if sub == COLOR_256 else 4
            packets, = struct.unpack_from("<H", d, p)
            p += 2
            i = 0
            for _ in range(packets):
                i += d[p]
                n = d[p + 1] or 256
                p += 2
                for _ in range(n):
                    r, g, b = d[p], d[p + 1], d[p + 2]
                    self.palette[i & 255] = (min(r * scale, 255), min(g * scale, 255), min(b * scale, 255))
                    i += 1
                    p += 3
            self.palette_set = True
        elif sub == BLACK:
            px[:] = bytes(w * h)
        elif sub == FLI_COPY:
            px[:] = d[p:p + w * h]
        elif sub == BYTE_RUN:
            for y in range(h):
                p += 1                       # obsolete packet count
                x = y * w
                end = x + w
                while x < end:
                    n = struct.unpack_from("<b", d, p)[0]
                    p += 1
                    if n < 0:
                        px[x:x - n] = d[p:p - n]
                        p -= n
                        x -= n
                    else:
                        px[x:x + n] = bytes([d[p]]) * n
                        p += 1
                        x += n
        elif sub == DELTA_FLI:
            y, lines = struct.unpack_from("<HH", d, p)
            p += 4
            for _ in range(lines):
                packets = d[p]
                p += 1
                x = y * w
                for _ in range(packets):
                    x += d[p]
                    n = struct.unpack_from("<b", d, p + 1)[0]
                    p += 2
                    if n > 0:
                        px[x:x + n] = d[p:p + n]
                        p += n
                        x += n
                    else:
                        px[x:x - n] = bytes([d[p]]) * (-n)
                        p += 1
                        x -= n
                y += 1
        elif sub == DELTA_FLC:
            lines, = struct.unpack_from("<H", d, p)
            p += 2
            y = 0
            while lines > 0:
                word, = struct.unpack_from("<H", d, p)
                p += 2
                if word & 0xC000 == 0xC000:          # skip lines
                    y += 0x10000 - word
                    continue
                if word & 0xC000 == 0x8000:          # last pixel of line
                    px[y * w + w - 1] = word & 0xFF
                    continue
                x = y * w
                for _ in range(word):
                    x += d[p]
                    n = struct.unpack_from("<b", d, p + 1)[0]
                    p += 2
                    if n > 0:
                        px[x:x + 2 * n] = d[p:p + 2 * n]
                        p += 2 * n
                        x += 2 * n
                    else:
                        px[x:x - 2 * n] = d[p:p + 2] * (-n)
                        p += 2
                        x -= 2 * n
                y += 1
                lines -= 1
        # PSTAMP and unknown chunks are ignored.
