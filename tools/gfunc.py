"""Print decompiled functions from work/ghidra/pest.c.

Usage: python3 tools/gfunc.py 1000:28ce [more addresses...]   (Ghidra selector:offset)
       python3 tools/gfunc.py sub_1_28ce                       (ne_disasm label; seg N -> selector 0x1000+8*(N-1))
"""
import re
import sys


def selector(arg):
    m = re.fullmatch(r"sub_(\d+)_([0-9a-f]+)", arg)
    if m:
        return f"{0x1000 + 8 * (int(m.group(1)) - 1):04x}:{int(m.group(2), 16):04x}"
    return arg.lower()


def main(args, path="work/ghidra/pest.c"):
    text = open(path).read()
    for arg in args:
        addr = selector(arg)
        m = re.search(r"^// ==== \S+ @ " + re.escape(addr) + r"\b.*?(?=^// ==== |\Z)", text, re.S | re.M)
        print(m.group(0) if m else f"// {arg}: not found")


if __name__ == "__main__":
    main(sys.argv[1:])
