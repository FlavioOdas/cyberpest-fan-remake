"""Make Ghidra's 16-bit decompiled C easier to read: rewrite field accesses through a base
pointer into `base->f_<byteoffset>` so the same field always looks the same, whatever pointer
scale Ghidra picked ((undefined4 *)p + 0x5e and (int)p + 0x178 are both p->f_178).

Usage: python3 tools/normalize_c.py < in.c > out.c
"""
import re
import sys

SIZES = {"undefined4": 4, "undefined2": 2, "undefined1": 1, "int": 2, "uint": 2, "char": 1, "byte": 1,
         "long": 4, "ulong": 4, "undefined": 1, "short": 2, "ushort": 2}

VAR = r"(?:param_\d+|[ip]Var\d+|local_\w+|in_\w+)"


def scaled(m):
    typ, var, off = m.group(1), m.group(2), int(m.group(3), 0)
    return f"({var} + 0x{off * SIZES.get(typ, 1):x})"


def main():
    s = sys.stdin.read()
    # ((int)param_1 + 0x292) and ((int)(undefined4 *)param_1 + 0x292) -> (param_1 + 0x292): the
    # (int) cast makes the addition byte-sized, so this must run before the scaled rewrite.
    s = re.sub(r"\(\(int\)(?:\(\w+ \*\))?(" + VAR + r") \+ (0x[0-9a-f]+|\d+)\)",
               lambda m: f"({m.group(1)} + 0x{int(m.group(2), 0):x})", s)
    # ((undefined4 *)param_1 + 0x5e)  ->  (param_1 + 0x178)
    s = re.sub(r"\(\((\w+) \*\)(" + VAR + r") \+ (0x[0-9a-f]+|\d+)\)", scaled, s)
    s = re.sub(r"(?<!\(int\))\((\w+) \*\)(" + VAR + r") \+ (0x[0-9a-f]+|\d+)", scaled, s)
    # *(T *)(param_1 + 0x178)  ->  param_1->f_178
    s = re.sub(r"\*\((?:\w+) \*\)\((" + VAR + r") \+ 0x([0-9a-f]+)\)", lambda m: f"{m.group(1)}->f_{m.group(2)}", s)
    # *(T *)(param_1 + 0x178) written with a cast on the base
    s = re.sub(r"\*\(\w+ \*\)\(\(\w+ \*\)(" + VAR + r") \+ 0x([0-9a-f]+)\)",
               lambda m: f"{m.group(1)}->f_{m.group(2)}", s)
    # Ghidra renders segment constants as string-pointer offsets; mark them.
    s = re.sub(r"\(char \*\)s_\w+_1050_([0-9a-f]+) \+ (0x[0-9a-f]+|\d+)", lambda m: f"SEG_{int(m.group(1), 16) + int(m.group(2), 0):04x}", s)
    sys.stdout.write(s)


if __name__ == "__main__":
    main()
