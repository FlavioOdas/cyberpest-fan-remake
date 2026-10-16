"""Find MFC 2.x (16-bit) message map entry tables in PEST.EXE.

Entry: {u16 nMessage, u16 nCode, u16 nID, u16 nLastID, u16 nSig, far pfn} (14 bytes); a table
ends with an all-zero entry. pfn's segment word is resolved via relocations.
Usage: python3 tools/msgmaps.py
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(__file__))
from nelib import Ne, SRC_SEGMENT, SRC_FARADDR  # noqa: E402

WM = {0x0001: "WM_CREATE", 0x0002: "WM_DESTROY", 0x0003: "WM_MOVE", 0x0005: "WM_SIZE", 0x0006: "WM_ACTIVATE",
      0x0007: "WM_SETFOCUS", 0x0008: "WM_KILLFOCUS", 0x000F: "WM_PAINT", 0x0010: "WM_CLOSE",
      0x0011: "WM_QUERYENDSESSION", 0x0014: "WM_ERASEBKGND", 0x0016: "WM_ENDSESSION", 0x0018: "WM_SHOWWINDOW",
      0x001C: "WM_ACTIVATEAPP", 0x0020: "WM_SETCURSOR", 0x0021: "WM_MOUSEACTIVATE", 0x0024: "WM_GETMINMAXINFO",
      0x0046: "WM_WINDOWPOSCHANGING", 0x0047: "WM_WINDOWPOSCHANGED", 0x0100: "WM_KEYDOWN", 0x0101: "WM_KEYUP",
      0x0102: "WM_CHAR", 0x0110: "WM_INITDIALOG", 0x0111: "WM_COMMAND", 0x0112: "WM_SYSCOMMAND",
      0x0113: "WM_TIMER", 0x0114: "WM_HSCROLL", 0x0115: "WM_VSCROLL", 0x0200: "WM_MOUSEMOVE",
      0x0201: "WM_LBUTTONDOWN", 0x0202: "WM_LBUTTONUP", 0x0203: "WM_LBUTTONDBLCLK", 0x0204: "WM_RBUTTONDOWN",
      0x0205: "WM_RBUTTONUP", 0x0206: "WM_RBUTTONDBLCLK", 0x0233: "WM_DROPFILES", 0x0311: "WM_PALETTECHANGED",
      0x030F: "WM_QUERYNEWPALETTE", 0x03B9: "MM_WOM_DONE?", 0x0084: "WM_NCHITTEST", 0x0086: "WM_NCACTIVATE",
      0x0019: "WM_CTLCOLOR", 0x0030: "WM_SETFONT"}


def main():
    ne = Ne("original/extracted/APP/PEST.EXE")
    for seg in ne.segments:
        d = seg.data
        for start in range(0, len(d) - 28, 2):
            entries = []
            o = start
            while o + 14 <= len(d):
                msg, code, nid, last, sig = struct.unpack_from("<HHHHH", d, o)
                fx = seg.fixups.get(o + 10) or seg.fixups.get(o + 12)
                if msg == 0 and sig == 0 and not fx:
                    break
                if not fx or fx[1][0] != "internal" or fx[0] not in (SRC_SEGMENT, SRC_FARADDR):
                    entries = []
                    break
                off = fx[1][2] if fx[0] == SRC_FARADDR else struct.unpack_from('<H', d, o + 10)[0]
                pfn = f"{0x1000 + 8 * (fx[1][1] - 1):04x}:{off:04x}"
                entries.append((msg, code, nid, last, sig, pfn))
                o += 14
            if len(entries) >= 2 and all(m < 0x400 or m >= 0xC000 for m, *_ in entries):
                # skip tables that are suffixes of an earlier-reported table
                prev_ok = not (start >= 14 and (seg.fixups.get(start - 4) or seg.fixups.get(start - 2)))
                if prev_ok:
                    print(f"message map seg{seg.number} @ {0x1000 + 8 * (seg.number - 1):04x}:{start:04x}")
                    for msg, code, nid, last, sig, pfn in entries:
                        name = WM.get(msg, hex(msg))
                        extra = f" id={nid}-{last}" if msg == 0x111 else ""
                        print(f"   {name:22s} code={code} sig={sig}{extra} -> {pfn}")


if __name__ == "__main__":
    main()
