"""Build tools/data/win16_ordinals.json from Wine .spec files (fetched into work/wine/).

Fetch: for each of krnl386.exe16 user.exe16 gdi.exe16 mmsystem.dll16 shell.dll16 commdlg.dll16
keyboard.drv16 win87em.dll16: https://raw.githubusercontent.com/wine-mirror/wine/master/dlls/<d>/<d>.spec
"""
import json
import re
import sys

MODULES = {"KERNEL": "krnl386.exe16", "USER": "user.exe16", "GDI": "gdi.exe16", "MMSYSTEM": "mmsystem.dll16",
           "SHELL": "shell.dll16", "COMMDLG": "commdlg.dll16", "KEYBOARD": "keyboard.drv16",
           "WIN87EM": "win87em.dll16"}

out = {}
src = sys.argv[1] if len(sys.argv) > 1 else "work/wine"
for mod, spec in MODULES.items():
    table = {}
    for line in open(f"{src}/{spec}.spec"):
        line = line.split("#")[0].strip()
        m = re.match(r"(\d+)\s+(\w+)\s+((?:-\S+\s+)*)(\w+)(?:\(([^)]*)\))?", line)
        if m:
            ordinal, kind, _flags, name, args = m.groups()
            table[ordinal] = {"name": name, "kind": kind, "args": args.split() if args else []}
    out[mod] = table
json.dump(out, open("tools/data/win16_ordinals.json", "w"), indent=0, sort_keys=True)
print({k: len(v) for k, v in out.items()})
