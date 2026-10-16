"""Dump a 16-bit Windows NE executable: header, segments, imports (by module/ordinal/name), entries, resources."""
import struct
import sys
from collections import defaultdict

RT = {1: "CURSOR", 2: "BITMAP", 3: "ICON", 4: "MENU", 5: "DIALOG", 6: "STRING", 7: "FONTDIR",
      8: "FONT", 9: "ACCELERATOR", 10: "RCDATA", 12: "GROUP_CURSOR", 14: "GROUP_ICON", 16: "VERSION"}


def pstr(b, o):
    n = b[o]
    return b[o + 1:o + 1 + n].decode("latin-1"), o + 1 + n


def main(path):
    b = open(path, "rb").read()
    ne, = struct.unpack_from("<I", b, 0x3C)
    assert b[ne:ne + 2] == b"NE"
    h = lambda off, fmt="<H": struct.unpack_from(fmt, b, ne + off)[0]
    flags, nseg, nmod = h(0x0C), h(0x1C), h(0x1E)
    seg_t, res_t, resn_t, mod_t, imp_t = (ne + h(x) for x in (0x22, 0x24, 0x26, 0x28, 0x2A))
    nonres_t, align = h(0x2C, "<I"), h(0x32)
    print(f"linker {b[ne+2]}.{b[ne+3]}  flags=0x{flags:04x}  target_os={b[ne+0x36]}  "
          f"expected_win={b[ne+0x3F]}.{b[ne+0x3E]:02d}  segments={nseg}  modules={nmod}")
    print(f"CS:IP={h(0x16)}:{h(0x14):04x}  heap={h(0x10)} stack={h(0x12)}")
    name, o = pstr(b, resn_t)
    print("module name:", name, "| description:", pstr(b, nonres_t)[0])

    mods = []
    for i in range(nmod):
        off, = struct.unpack_from("<H", b, mod_t + 2 * i)
        mods.append(pstr(b, imp_t + off)[0])
    print("imported modules:", ", ".join(mods))

    imports = defaultdict(set)
    print("\nsegments:")
    for i in range(nseg):
        sector, length, sflags, minalloc = struct.unpack_from("<HHHH", b, seg_t + 8 * i)
        kind = "DATA" if sflags & 1 else "CODE"
        print(f"  {i+1:3d} {kind} len={length or 65536:6d} flags=0x{sflags:04x}")
        if sflags & 0x100 and sector:
            r = (sector << align) + (length or 65536)
            cnt, = struct.unpack_from("<H", b, r)
            for k in range(cnt):
                src, rflags, _off, a, c = struct.unpack_from("<BBHHH", b, r + 2 + 8 * k)
                typ = rflags & 3
                if typ == 1:
                    imports[mods[a - 1]].add(c)
                elif typ == 2:
                    imports[mods[a - 1]].add(pstr(b, imp_t + c)[0])

    print("\nimports (module: ordinals/names):")
    for m in mods:
        items = sorted(imports[m], key=lambda x: (isinstance(x, str), x))
        print(f"  {m} ({len(items)}): {', '.join(map(str, items))}")

    print("\nresources:")
    shift, = struct.unpack_from("<H", b, res_t)
    o = res_t + 2
    while True:
        tid, = struct.unpack_from("<H", b, o)
        if tid == 0:
            break
        count, = struct.unpack_from("<H", b, o + 2)
        tname = RT.get(tid & 0x7FFF, str(tid & 0x7FFF)) if tid & 0x8000 else pstr(b, res_t + tid)[0]
        names = []
        for k in range(count):
            roff, rlen, rfl, rid = struct.unpack_from("<HHHH", b, o + 8 + 12 * k)
            names.append(str(rid & 0x7FFF) if rid & 0x8000 else pstr(b, res_t + rid)[0])
        print(f"  {tname} x{count}: {', '.join(names[:40])}{' ...' if count > 40 else ''}")
        o += 8 + 12 * count


if __name__ == "__main__":
    main(sys.argv[1])
