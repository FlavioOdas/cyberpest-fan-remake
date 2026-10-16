"""Print ADM modules as an indented tree with symbol names from docs/re/symbols.tsv.

Usage: python3 tools/adm_show.py -70 -72 [--depth N] [--adm path]
"""
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from admlib import Adm  # noqa: E402


def symbol_names():
    names = {}
    p = os.path.join(os.path.dirname(__file__), "..", "docs", "re", "symbols.tsv")
    if os.path.exists(p):
        for line in open(p):
            if line.strip() and not line.startswith("#"):
                f = line.rstrip("\n").split("\t")
                names[int(f[0])] = f[1]
    return names


NAMES = symbol_names()


def sym(v):
    return f"{v}<{NAMES[v]}>" if v in NAMES else str(v)


def show(n, depth, maxdepth):
    pre = "  " * depth
    vals = []
    for c in n.children:
        if c.tag == "ADVI":
            vals.append(sym(c.value) if c.value < 0 else str(c.value))
        elif c.tag == "ADVS":
            vals.append(repr(c.value[:50]))
        elif c.tag == "NDVE":
            vals.append(c.value["name"])
    print(f"{pre}{n.tag} id={sym(n.id)} ref={sym(n.ref)}" + (f"  [{', '.join(vals)}]" if vals else ""))
    if depth < maxdepth:
        for c in n.children:
            if c.tag in ("ADDM", "ADDL"):
                show(c, depth + 1, maxdepth)


def main(argv):
    depth, adm_path, ids = 99, "original/extracted/APP/SLOW/PEST.ADM", []
    it = iter(argv)
    for a in it:
        if a == "--depth":
            depth = int(next(it))
        elif a == "--adm":
            adm_path = next(it)
        else:
            ids.append(int(a))
    adm = Adm(adm_path)
    mods = {n.id: n for n in adm.roots if n.tag == "ADDM"}
    for i in ids:
        show(mods[i], 0, depth)


if __name__ == "__main__":
    main(sys.argv[1:])
