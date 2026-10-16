"""16-bit Windows NE executable parser: segments, entry table, relocations (chains resolved)."""
import struct

SRC_LOBYTE, SRC_SEGMENT, SRC_FARADDR, SRC_OFFSET = 0, 2, 3, 5


def pstr(b, o):
    n = b[o]
    return b[o + 1:o + 1 + n].decode("latin-1")


class Segment:
    def __init__(self, number, data, flags, minalloc):
        self.number, self.data, self.flags, self.minalloc = number, data, flags, minalloc
        self.is_code = not flags & 1
        self.fixups = {}          # location -> (source_type, target) ; target is a tuple, see Ne._target


class Ne:
    def __init__(self, path):
        b = self.b = open(path, "rb").read()
        ne = self.ne = struct.unpack_from("<I", b, 0x3C)[0]
        if b[ne:ne + 2] != b"NE":
            raise ValueError("not an NE executable")
        h = lambda off, fmt="<H": struct.unpack_from(fmt, b, ne + off)[0]
        self.autodata = h(0x0E)
        self.entry_cs, self.entry_ip = h(0x16), h(0x14)
        self.align = h(0x32)
        nseg, nmod = h(0x1C), h(0x1E)
        seg_t, mod_t, imp_t = ne + h(0x22), ne + h(0x28), ne + h(0x2A)
        self.imp_t = imp_t
        self.modules = [pstr(b, imp_t + struct.unpack_from("<H", b, mod_t + 2 * i)[0]) for i in range(nmod)]
        self.entries = self._entries(ne + h(0x04), h(0x06))
        self.segments = []
        for i in range(nseg):
            sector, length, flags, minalloc = struct.unpack_from("<HHHH", b, seg_t + 8 * i)
            length = length or 0x10000
            start = sector << self.align
            data = bytearray(b[start:start + length]) if sector else bytearray(minalloc or 0x10000)
            seg = Segment(i + 1, data, flags, minalloc)
            if flags & 0x100 and sector:
                self._relocs(seg, start + length)
            self.segments.append(seg)

    def _entries(self, o, size):
        b, end, ordinal, entries = self.b, o + size, 1, {}
        while o < end:
            count, kind = b[o], b[o + 1]
            o += 2
            if count == 0:
                break
            for _ in range(count):
                if kind == 0:
                    pass
                elif kind == 0xFF:
                    seg, off = b[o + 3], struct.unpack_from("<H", b, o + 4)[0]
                    entries[ordinal] = (seg, off)
                    o += 6
                else:
                    entries[ordinal] = (kind, struct.unpack_from("<H", b, o + 1)[0])
                    o += 3
                ordinal += 1
        return entries

    def _target(self, rflags, a, c):
        kind = rflags & 3
        if kind == 0:
            seg = a & 0xFF
            if seg == 0xFF:
                seg, c = self.entries.get(c, (0, c))
            return ("internal", seg, c)
        if kind == 1:
            return ("ordinal", self.modules[a - 1], c)
        if kind == 2:
            return ("name", self.modules[a - 1], pstr(self.b, self.imp_t + c))
        return ("osfixup", a, c)

    def _relocs(self, seg, r):
        b = self.b
        count, = struct.unpack_from("<H", b, r)
        for k in range(count):
            src, rflags, off, a, c = struct.unpack_from("<BBHHH", b, r + 2 + 8 * k)
            target = self._target(rflags, a, c)
            if rflags & 4:                     # additive: single location
                seg.fixups[off] = (src, target)
                continue
            seen = set()
            while off != 0xFFFF and off < len(seg.data) and off not in seen:
                seen.add(off)
                nxt, = struct.unpack_from("<H", seg.data, off)
                seg.fixups[off] = (src, target)
                off = nxt
