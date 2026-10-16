"""Cross-reference ADM symbol ids (negative ints) between PEST.EXE code and PEST.ADM data.

Usage: python3 tools/symbol_xref.py [symbol ...]   -> writes work/symbols.tsv; prints given symbols
Needs work/disasm (tools/ne_disasm.py). Columns: symbol, #code functions, #data ids, #data values,
functions, sample data contexts (parent tag/id and sibling values).
"""
import collections
import glob
import os
import re
import sys

sys.path.insert(0, os.path.dirname(__file__))
from admlib import Adm  # noqa: E402

LOW = -1000


def code_uses():
    uses = collections.defaultdict(set)
    for f in sorted(glob.glob("work/disasm/seg*.asm")):
        fn = None
        for line in open(f):
            if line.startswith("sub_") and line.rstrip().endswith(":"):
                fn = line.strip()[:-1]
                continue
            m = re.search(r"(?:push|mov\s+[^,]+,|cmp\s+[^,]+,)\s*0x(f[c-f][0-9a-f]{2})\b", line)
            if m:
                v = int(m.group(1), 16) - 0x10000
                if LOW < v < -1:
                    uses[v].add(fn)
    return uses


def data_uses(adm):
    ids, vals = collections.defaultdict(list), collections.defaultdict(list)

    def rec(node, parent):
        if node.tag in ("ADDM", "ADDL"):
            if node.id < 0:
                vals_here = [c.value for c in node.children if c.tag == "ADVI"][:6]
                ids[node.id].append(f"{node.tag}<{parent}>{vals_here}")
            for c in node.children:
                rec(c, f"{node.tag}:{node.id}")
        elif node.tag == "ADVI" and LOW < node.value < 0:
            vals[node.value].append(parent)

    for r in adm.roots:
        rec(r, "root")
    return ids, vals


def main(wanted):
    adm = Adm("original/extracted/APP/SLOW/PEST.ADM")
    uses = code_uses()
    ids, vals = data_uses(adm)
    symbols = sorted(set(uses) | set(ids) | set(vals), reverse=True)
    with open("work/symbols.tsv", "w") as f:
        for s in symbols:
            f.write(f"{s}\t{len(uses[s])}\t{len(ids[s])}\t{len(vals[s])}\t{','.join(sorted(uses[s]))}\t"
                    f"{' | '.join(ids[s][:3])}\t{' | '.join(collections.Counter(vals[s]).most_common(1)[0][0:1]) if vals[s] else ''}\n")
    for w in wanted:
        s = int(w)
        print(f"{s}: code={sorted(uses[s])}\n  as id: {ids[s][:5]}\n  as value in: {collections.Counter(vals[s]).most_common(5)}")
    print(f"{len(symbols)} symbols -> work/symbols.tsv")


if __name__ == "__main__":
    main(sys.argv[1:])
