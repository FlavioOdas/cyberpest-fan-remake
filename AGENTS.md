# CyberPest remake — agent guide

Goal: make CyberPest (1996, Scratch 'n' Sniff / EMG; a 16-bit Windows 3.1 "desktop pet")
run natively on Windows 11 **without emulators or extra software**, by reverse engineering
the original program and rebuilding it as a modern native Win32 app that loads the
user's original data file (`PEST.ADM`) at runtime.

## Start here (every session)

1. Read `docs/CHECKPOINTS.md` — the **current state** section and the last log entry tell
   you exactly where work stopped and what to do next. It is a **local working log**:
   gitignored, never committed (create it if missing).
2. Read `docs/PLAN.md` for milestones and the architecture decision.
3. Format/code findings live in `docs/formats/` and `docs/re/`.

## Ground rules

- **Never commit original game files or anything extracted/derived from them** (EXE,
  DLLs, PEST.ADM, decoded sprites, WAVs, full disassembly listings). They live in
  `original/` and `work/`, both gitignored. Short quotes of disassembly in docs are fine.
- If `original/` is missing, run `tools/bootstrap.sh` (needs the user's
  `CyberPest.zip`, default `/mnt/c/Users/odasf/Downloads/CyberPest.zip`).
- Python tooling must use the **standard library only** (no pip available on this machine).
- Every finding goes in a doc with how it was established (observed in bytes, inferred,
  or unknown). Do not present guesses as facts.
- **Checkpoint discipline:** after each meaningful step, append a dated entry to
  `docs/CHECKPOINTS.md` and update its "Current state" section. The file is gitignored and
  must never be committed — it is a private working log. Commit the code/docs it describes;
  do not push unless the user asks.
- REA/Hopper cannot open 16-bit NE executables. Use `tools/ne_disasm.py` (objdump `-m i8086`,
  relocations resolved to API names) and Ghidra headless (`tools/ghidra/`, output in `work/ghidra/`).

## Toolchain (dev only, no sudo)

`tools/setup_toolchain.sh` installs JDK 21, Ghidra 12.1.4, llvm-mingw (Windows cross-compiler)
and CMake into `~/tools`. Then `source ~/tools/env.sh`. Regenerate analysis outputs with:

```bash
python3 tools/adm_dump.py original/extracted/APP/SLOW/PEST.ADM work/adm
python3 tools/ne_disasm.py original/extracted/APP/PEST.EXE work/disasm
"$GHIDRA_HOME/support/analyzeHeadless" work/ghidra CyberPest -import original/extracted/APP/PEST.EXE \
  -overwrite -scriptPath tools/ghidra -preScript EnableParamId.java -postScript FixChkstk.java -postScript DecompileAll.java work/ghidra/pest.c work/ghidra/functions.tsv
```

After the first import, `tools/redecompile.sh` re-applies `FixChkstk.java` (marks MSC `__chkstk`
frame allocations — without it locals and comparisons decompile as garbage) and the names in
`docs/re/names.tsv`, then regenerates `work/ghidra/pest.c` (~20 s).

Handy: `python3 tools/gfunc.py sub_1_5604` (or `1000:5604`) prints one decompiled function;
`python3 tools/symbol_xref.py -178` shows where an ADM symbol id is used in code and data;
`python3 tools/ne_resources.py original/extracted/APP/PEST.EXE` dumps strings/menus/dialogs.
Ghidra selector = 0x1000 + 8*(segment-1); `sub_<seg>_<off>` labels come from `tools/ne_disasm.py`.

## Layout

- `tools/` — RE and extraction tools (stdlib Python)
- `docs/` — plan, making-of, format and code notes (`CHECKPOINTS.md` is a local, gitignored working log)
- `src/` — the remake (C++20, Win32, CMake); `README.md` is the player-facing guide
- `original/` — gitignored: `installer/` (zip contents) and `extracted/` (installed files)
- `work/` — gitignored: generated disassembly, dumped assets, scratch output
