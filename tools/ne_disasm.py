"""Disassemble a 16-bit NE executable with relocations resolved to API names / internal labels.

Usage: python3 tools/ne_disasm.py original/extracted/APP/PEST.EXE work/disasm
Writes seg<N>.asm per code segment, functions.tsv (function -> API calls) and strings.tsv.
Needs GNU objdump (i8086 support). DLL export names for WING/WAVMIX16 are read from
original/extracted/SYSTEM; system DLL names come from tools/data/win16_ordinals.json.
"""
import json
import os
import re
import struct
import subprocess
import sys
from collections import defaultdict

sys.path.insert(0, os.path.dirname(__file__))
from nelib import Ne, SRC_FARADDR, SRC_SEGMENT, SRC_OFFSET  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
FARCALL = re.compile(r"^l?(call|jmp)\s+0x([0-9a-f]+)[:,]0x([0-9a-f]+)$")
LINE = re.compile(r"^\s*([0-9a-f]+):\t((?:[0-9a-f]{2} )+)\s*\t?(.*)$")


def dll_exports(path):
    b = open(path, "rb").read()
    ne = struct.unpack_from("<I", b, 0x3C)[0]
    names = {}
    for table in (ne + struct.unpack_from("<H", b, ne + 0x26)[0], struct.unpack_from("<I", b, ne + 0x2C)[0]):
        o, first = table, True
        while b[o]:
            n = b[o]
            if not first:
                names[struct.unpack_from("<H", b, o + 1 + n)[0]] = b[o + 1:o + 1 + n].decode("latin-1")
            first = False
            o += 3 + n
    return names


class Namer:
    def __init__(self, exe_path):
        self.sys = json.load(open(os.path.join(HERE, "data", "win16_ordinals.json")))
        sysdir = os.path.join(os.path.dirname(os.path.dirname(exe_path)), "SYSTEM")
        self.dll = {}
        for mod, f in (("WING", "WING.DLL"), ("WAVMIX16", "WAVMIX16.DLL")):
            p = os.path.join(sysdir, f)
            if os.path.exists(p):
                self.dll[mod] = dll_exports(p)

    def api(self, mod, ordinal):
        if mod in self.dll and ordinal in self.dll[mod]:
            return f"{mod}.{self.dll[mod][ordinal]}"
        e = self.sys.get(mod, {}).get(str(ordinal))
        return f"{mod}.{e['name']}" if e else f"{mod}.#{ordinal}"


def _inside_far_call(seg, loc):
    """True when the segment fixup belongs to a `call/jmp far seg:off` instruction (opcode 9A/EA)."""
    return loc >= 3 and seg.data[loc - 3] in (0x9A, 0xEA)


def label(seg, off):
    return f"sub_{seg}_{off:04x}"


def objdump(data, start=None, stop=None):
    path = os.path.join(OUT, ".seg.bin")
    with open(path, "wb") as f:
        f.write(data)
    cmd = ["objdump", "-D", "-b", "binary", "-m", "i8086", "-M", "intel"]
    if start is not None:
        cmd += [f"--start-address={start}", f"--stop-address={stop}"]
    text = subprocess.run(cmd + [path], capture_output=True, text=True, check=True).stdout
    insns = []
    for line in text.splitlines():
        m = LINE.match(line)
        if m:
            addr = int(m.group(1), 16)
            insns.append([addr, len(m.group(2).split()), m.group(3).strip()])
        elif insns and re.match(r"^\s*[0-9a-f]+:\t(?:[0-9a-f]{2} )+\s*$", line):
            insns[-1][1] += len(line.split("\t")[1].split())   # continuation of long instruction bytes
    return insns


def strings_of(seg):
    found, d, o = {}, seg.data, 0
    while o < len(d):
        e = o
        while e < len(d) and (32 <= d[e] < 127 or d[e] in (9, 10, 13)):
            e += 1
        if e - o >= 3 and e < len(d) and d[e] == 0:
            found[o] = d[o:e].decode("latin-1")
        o = e + 1
    return found


def main(exe, out):
    global OUT
    OUT = out
    os.makedirs(out, exist_ok=True)
    ne, namer = Ne(exe), Namer(exe)
    dgroup = ne.segments[ne.autodata - 1] if ne.autodata else None
    strs = strings_of(dgroup) if dgroup else {}
    with open(os.path.join(out, "strings.tsv"), "w") as f:
        for o, s in sorted(strs.items()):
            f.write(f"{o:04x}\t{s!r}\n")

    def describe(target):
        kind, a, c = target
        if kind == "internal":
            seg = ne.segments[a - 1] if 0 < a <= len(ne.segments) else None
            if seg is not None and seg.is_code:
                return label(a, c)
            return f"seg{a}:{c:04x}"
        if kind == "ordinal":
            return namer.api(a, c)
        if kind == "name":
            return f"{a}.{c}"
        return f"OSFIXUP({a},{c})"

    def far_target(seg, addr, n, text):
        """Internal far call/jmp: the fixup patches only the segment word; offset is in the insn."""
        m = FARCALL.match(text)
        if not m:
            return None
        fx = seg.fixups.get(addr + 3)
        if fx and fx[1][0] == "internal" and fx[0] == SRC_SEGMENT:
            return fx[1][1], int(m.group(3), 16)
        return None

    # Function starts: entry point, exported entries, far references into code, near call targets.
    starts = defaultdict(set)
    starts[ne.entry_cs].add(ne.entry_ip)
    for seg, off in ne.entries.values():
        starts[seg].add(off)
    for seg in ne.segments:
        for loc, (src, t) in seg.fixups.items():
            if t[0] != "internal" or not 0 < t[1] <= len(ne.segments) or not ne.segments[t[1] - 1].is_code:
                continue
            if src == SRC_FARADDR:
                starts[t[1]].add(t[2])
            elif src == SRC_SEGMENT and loc >= 2:
                # Far pointer stored as offset word + segment fixup (vtables, callback tables).
                off = struct.unpack_from("<H", seg.data, loc - 2)[0]
                target = ne.segments[t[1] - 1]
                if off < len(target.data) and not (seg.is_code and _inside_far_call(seg, loc)):
                    starts[t[1]].add(off)
    for seg in ne.segments:
        if not seg.is_code:
            continue
        for addr, n, text in objdump(bytes(seg.data)):
            m = re.match(r"call\s+0x([0-9a-f]+)$", text)
            if m:
                starts[seg.number].add(int(m.group(1), 16))
            t = far_target(seg, addr, n, text)
            if t:
                starts[t[0]].add(t[1])

    calls = defaultdict(lambda: defaultdict(int))    # function -> api/label -> count
    for seg in ne.segments:
        if not seg.is_code:
            continue
        seg_starts = sorted(s for s in starts[seg.number] if s < len(seg.data))
        bounds = seg_starts + [len(seg.data)]
        lines = []
        if not seg_starts or seg_starts[0] != 0:
            bounds = [0] + bounds
        for i in range(len(bounds) - 1):
            lo, hi = bounds[i], bounds[i + 1]
            if lo >= hi:
                continue
            fn = label(seg.number, lo)
            lines.append(f"\n{fn}:")
            for addr, n, text in objdump(bytes(seg.data), lo, hi):
                notes = []
                ft = far_target(seg, addr, n, text)
                if ft:
                    name = label(*ft)
                    calls[fn][name] += 1
                    text = re.sub(r"0x[0-9a-f]+[:,]0x[0-9a-f]+$", name, text)
                for loc in range(addr, addr + n):
                    if ft and loc == addr + 3:
                        continue
                    fx = seg.fixups.get(loc)
                    if fx:
                        src, t = fx
                        name = describe(t)
                        kind = {SRC_FARADDR: "far", SRC_SEGMENT: "seg", SRC_OFFSET: "off"}.get(src, f"src{src}")
                        notes.append(f"{kind} {name}")
                        if text.startswith("call") or text.startswith("lcall"):
                            calls[fn][name] += 1
                m = re.match(r"call\s+0x([0-9a-f]+)$", text)
                if m:
                    calls[fn][label(seg.number, int(m.group(1), 16))] += 1
                # Only plain immediates (push imm / mov reg,imm), never memory operands.
                m = re.match(r"(?:push\s+|mov\s+(?:ax|bx|cx|dx|si|di),)0x([0-9a-f]{3,4})$", text)
                if m and int(m.group(1), 16) in strs:
                    notes.append(repr(strs[int(m.group(1), 16)][:60]))
                lines.append(f"  {addr:04x}  {text}" + (f"    ; {'; '.join(notes)}" if notes else ""))
        with open(os.path.join(out, f"seg{seg.number}.asm"), "w") as f:
            f.write("\n".join(lines) + "\n")
    with open(os.path.join(out, "functions.tsv"), "w") as f:
        for fn in sorted(calls):
            f.write(fn + "\t" + ", ".join(f"{k}x{v}" if v > 1 else k for k, v in sorted(calls[fn].items())) + "\n")
    os.remove(os.path.join(out, ".seg.bin"))
    total = sum(len(v) for v in starts.values())
    print(f"segments={len(ne.segments)} functions~{total} strings={len(strs)} dgroup=seg{ne.autodata}")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
