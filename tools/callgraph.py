"""Functions reachable from given roots (by ne_disasm labels), using work/disasm/functions.tsv.

Usage: python3 tools/callgraph.py [--seg N ...] sub_2_0000 sub_2_69b2 ...
Prints reachable functions with their direct API calls; --seg limits traversal to those segments.
"""
import re
import sys


def load():
    graph, apis = {}, {}
    for line in open("work/disasm/functions.tsv"):
        fn, _, calls = line.rstrip("\n").partition("\t")
        items = [re.sub(r"x\d+$", "", c.strip()) for c in calls.split(",") if c.strip()]
        graph[fn] = [c for c in items if c.startswith("sub_")]
        apis[fn] = [c for c in items if not c.startswith("sub_")]
    return graph, apis


def main(argv):
    segs, roots, it = set(), [], iter(argv)
    for a in it:
        if a == "--seg":
            segs.add(next(it))
        else:
            roots.append(a)
    graph, apis = load()
    seen, todo = [], list(roots)
    while todo:
        f = todo.pop()
        if f in seen:
            continue
        seen.append(f)
        for c in graph.get(f, []):
            if not segs or c.split("_")[1] in segs:
                todo.append(c)
    for f in sorted(seen, key=lambda x: (int(x.split("_")[1]), int(x.split("_")[2], 16))):
        print(f, ", ".join(apis.get(f, [])))
    print(len(seen), "functions", file=sys.stderr)


if __name__ == "__main__":
    main(sys.argv[1:])
