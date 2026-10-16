"""Dump string tables, menus and dialogs from a 16-bit NE executable.

Usage: python3 tools/ne_resources.py original/extracted/APP/PEST.EXE > work/resources.txt
"""
import struct
import sys


def resources(b):
    ne = struct.unpack_from("<I", b, 0x3C)[0]
    res_t = ne + struct.unpack_from("<H", b, ne + 0x24)[0]
    shift = struct.unpack_from("<H", b, res_t)[0]
    o = res_t + 2
    while True:
        tid, count = struct.unpack_from("<HH", b, o)
        if tid == 0:
            return
        for k in range(count):
            off, length, _flags, rid = struct.unpack_from("<HHHH", b, o + 8 + 12 * k)
            yield tid & 0x7FFF, rid & 0x7FFF, b[off << shift:(off << shift) + (length << shift)]
        o += 8 + 12 * count


def cstr(d, o):
    e = d.index(b"\0", o)
    return d[o:e].decode("latin-1"), e + 1


def strings(rid, d):
    o = 0
    for i in range(16):
        n = d[o]
        if n:
            yield (rid - 1) * 16 + i, d[o + 1:o + 1 + n].decode("latin-1")
        o += 1 + n


def menu(d, o=4, depth=0):
    while o < len(d):
        flags, = struct.unpack_from("<H", d, o)
        o += 2
        if flags & 0x10:                       # MF_POPUP
            text, o = cstr(d, o)
            print("  " * depth + f"POPUP {text!r}")
            o = menu(d, o, depth + 1)
        else:
            cmd, = struct.unpack_from("<H", d, o)
            text, o = cstr(d, o + 2)
            print("  " * depth + f"  {cmd:5d} {text!r}")
        if flags & 0x80:                       # MF_END
            return o
    return o


def dialog(d):
    style, = struct.unpack_from("<I", d, 0)
    n = d[4]
    x, y, w, h = struct.unpack_from("<hhhh", d, 5)
    o = 13
    if d[o] == 0xFF:
        o += 3
    else:
        _, o = cstr(d, o)
    _cls, o = cstr(d, o)
    title, o = cstr(d, o)
    print(f"  {title!r} {x},{y} {w}x{h} style=0x{style:08x}")
    if style & 0x40:                           # DS_SETFONT
        size, = struct.unpack_from("<H", d, o)
        face, o = cstr(d, o + 2)
        print(f"  font {face} {size}")
    for _ in range(n):
        cx, cy, cw, ch, cid, cstyle = struct.unpack_from("<hhhhHI", d, o)
        o += 14
        cls = d[o]
        if cls & 0x80:
            cls_name = {0x80: "BUTTON", 0x81: "EDIT", 0x82: "STATIC", 0x83: "LISTBOX", 0x84: "SCROLLBAR",
                        0x85: "COMBOBOX"}.get(cls, hex(cls))
            o += 1
        else:
            cls_name, o = cstr(d, o)
        text, o = cstr(d, o)
        extra = d[o]
        o += 1 + extra
        print(f"    {cls_name:9s} id={cid:5d} {cx},{cy} {cw}x{ch} {text!r}")


def main(path):
    b = open(path, "rb").read()
    for tid, rid, d in resources(b):
        if tid == 6:
            for sid, text in strings(rid, d):
                print(f"STRING {sid:5d} {text!r}")
        elif tid == 4:
            print(f"MENU {rid}")
            menu(d)
        elif tid == 5:
            print(f"DIALOG {rid}")
            dialog(d)


if __name__ == "__main__":
    main(sys.argv[1])
