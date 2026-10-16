"""Dump C++ vtables (arrays of far code pointers) from PEST.EXE.

Usage: python3 tools/vtable.py 1038:40de [1038:3f9a ...]   (Ghidra selector:offset of the vtable)
Each slot is offset:segment; the segment word is resolved through the NE relocation table.
Names come from docs/re/names.tsv when known.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(__file__))
from nelib import Ne, SRC_SEGMENT, SRC_FARADDR  # noqa: E402


def names():
    out = {}
    p = os.path.join(os.path.dirname(__file__), "..", "docs", "re", "names.tsv")
    for line in open(p):
        if line.strip() and not line.startswith("#"):
            f = line.rstrip("\n").split("\t")
            out[f[0].lower()] = f[1]
    return out


def main(args):
    ne = Ne("original/extracted/APP/PEST.EXE")
    known = names()
    for arg in args:
        sel, off = (int(x, 16) for x in arg.split(":"))
        seg = ne.segments[(sel - 0x1000) // 8]
        print(f"vtable {arg}")
        slot, first = 0, None
        while True:
            at = off + 4 * slot
            fx = seg.fixups.get(at + 2) or seg.fixups.get(at)
            if not fx or fx[1][0] != "internal":
                break
            src, (_, tseg, toff) = fx
            if src == SRC_SEGMENT:
                toff = struct.unpack_from("<H", seg.data, at)[0]
            target = f"{0x1000 + 8 * (tseg - 1):04x}:{toff:04x}"
            if slot and target == first:          # next vtable starts with the same CObject slot
                break
            first = first or target
            print(f"  +0x{4 * slot:02x}  {target}  {known.get(target, '')}")
            slot += 1


if __name__ == "__main__":
    main(sys.argv[1:])
