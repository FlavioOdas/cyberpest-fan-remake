# How the CyberPest remake was made

This page tells the story behind `src/`: how a 16-bit Windows 3.1 program from 1996 — one
452 KB `PEST.EXE` and one large `PEST.ADM` data file, with no source code anywhere — was taken
apart and turned into a native Windows 10/11 application that runs the same data.

The dated, evidence-first notes live in [`formats/`](formats/) (file formats) and
[`re/`](re/) (code findings); this page is the overall picture. Every claim in those docs is
marked as observed in bytes, inferred from code, or unknown — that discipline is the whole
method: *read the original, port what it does, and write down how you know.*

## 1. Opening the box

The starting point is the original `CyberPest.zip` (the CD/disk image). Its `SETUP.EXE` is a
16-bit InstallShield 3 installer; the game itself sits inside `DATA.Z`, compressed with PKWARE
DCL "implode" (`tools/is3x.py` extracts all of it with matching sizes, from a port of zlib's
`contrib/blast`). That yielded the two variants the installer chose between: a "high spec"
`SLOW\PEST.ADM` (17,330,506 bytes, full frame rate) and a "low spec" `FAST\PEST.ADM`
(12,171,688 bytes), plus the WinG and WaveMix drivers the original needed. Details:
[`formats/installer.md`](formats/installer.md).

## 2. Reading the data file

`PEST.ADM` turned out to be a RIFF container (`ADBM`) holding a tree of *modules*, *data
lists*, integer/string values and *virtual files*: 688 unique assets (578 FLC animations —
the sprites — 100 WAV sounds and 10 BMPs such as the master palette). A custom reader
(`tools/admlib.py`) and FLC decoder (`tools/flc.py`) dump the whole thing as JSON + PNG frame
strips, which is how the animation names and the object layout tables ("the room is data")
became readable. One crucial detail: palette index 0 is pure green and means *transparent* —
that single fact is what lets the remake blit sprites over the room exactly like the original.
The format notes, including the sprite/script keywords: [`formats/adm.md`](formats/adm.md).

## 3. Taking apart PEST.EXE

The executable is a 16-bit New Executable (NE) built with Microsoft Visual C++ 1.5x and a
statically linked MFC 2.x. Standard reverse-engineering tools can't open it (REA, Hopper and
friends reject NE), so the project wrote its own: a full NE parser with relocation-chain
resolution (`tools/nelib.py`) feeding `objdump -m i8086` disassembly (`tools/ne_disasm.py`),
with Windows API imports resolved from ordinal numbers using tables generated from Wine's
module specs (`tools/gen_win16_names.py`). Import usage then labelled the subsystems —
file I/O through `mmio*`, WinG for graphics, WaveMix16 for sound, `GetPrivateProfile*` for
settings: [`formats/pest-exe.md`](formats/pest-exe.md), [`re/subsystems.md`](re/subsystems.md).

## 4. Making the code readable

The disassembly is enough for hot spots, but the logic needed C. Ghidra's headless decompiler
produced `work/ghidra/pest.c` for essentially every function, with two project-specific
fixes that make the difference between readable and garbage:

- **`FixChkstk.java`** marks MSVC's `__chkstk` stack allocations as such. Without it, Ghidra
  miscomputes every frame that allocates locals, and comparisons decompile as nonsense.
- **`ApplyNames.java` + [`re/names.tsv`](re/names.tsv)** renames the functions and globals
  identified by hand (500+ names), so the decompiled C speaks in terms of the game's actual
  functions rather than `sub_x_y`.

Helper tools then make the decompilation navigable: `tools/gfunc.py` prints one function,
`tools/callgraph.py` walks a call tree, `tools/normalize_c.py` tidies the C,
`tools/symbol_xref.py` finds where each ADM symbol id is used, `tools/vtable.py` and
`tools/class_inventory.py` map object classes and their overridden virtual methods.

## 5. Understanding the engine

The decompiled code revealed a small but real game engine:

- Objects are **modules in the data**, instantiated by a class factory (`1000:5604` in the
  original, segment:offset addresses are used throughout the docs) and driven by a
  **state-machine runtime** — states are sub-modules, and a little command language
  (`-1 remove`, `-6 spawn`, `-109 state`, property setters, …) wires them together. That is
  [`re/state-commands.md`](re/state-commands.md).
- Each object has a **hook table** for creating, showing, hitting, dragging, falling,
  finished sprites and so on — [`re/engine-runtime.md`](re/engine-runtime.md).
- The pest itself is not scripted: it is ~96 C functions covering decision making, walking,
  needs (stomach, bowels, mood, fleas, rabies, heat), ordered toys/food preferences per birth
  hour, megaphone word learning, reward and punishment, the phone callers and save/load —
  [`re/pest.md`](re/pest.md).
- Twenty **special classes** override the generic object slots (shelf items, certificate,
  text panels, phone, clock, bin, mouse, attention pop-up, …) —
  [`re/classes.md`](re/classes.md).

The remake mirrors all of this: the same data modules instantiate the same classes, so the
room is built from the player's own `PEST.ADM`, not from hard-coded scenes.

## 6. Porting behaviour function by function

With the engine understood, the work becomes a long, careful translation into C++ — and the
project's ground rule is that a port cites its source: every method in `src/` names the
original function it comes from, and any deliberate deviation is written down. Porting this
way also fixed bugs in the *original* that were faithfully reproduced at first: property -322
doesn't mean "paused" but "visible" (the pest used to enter the room invisible); moving a held
item has to use the *saved drag-origin depth*, not its current position (else food teleports);
the `-493` ranged-move command read its arguments in the wrong slots. Each of those is
written up with the byte-level evidence.

The platform layers were rebuilt with the closest modern equivalents, chosen so the original
model maps 1:1: WinG → 8-bit DIB sections (WinG *is* the prototype of `CreateDIBSection`, the
original's Blast/BitBlt model becomes `StretchDIBits`), and WaveMix16 → a small software mixer
feeding `waveOut`. The architecture decision and milestones:
[`PLAN.md`](PLAN.md).

## 7. Testing a game that only runs on Windows 3.1

There is no emulator in the loop, so tests run the *real* remake: `tools/snap.sh` starts the
Windows executable headlessly from WSL with a simulated clock and scripted input — clicks,
drags, typing — then dumps every object's state and saves a frame. That makes it possible to
assert things like "the bin captures the held object at 2.2 s and the object is gone by
6.5 s" without a mouse. `tools/fuzz.py` goes further: twenty random-input sessions of a few
simulated minutes each. It paid off immediately — it found a hang in the original's decision
loop (reopening the hatch while the pest idles on a timer spins forever), which the remake
now guards against. Snapshot runs keep their own save file so a test can never overwrite the
player's pest.

## 8. Shipping it

The player-facing side is deliberately boring: one `.exe`, no installer. On first run the
remake imports the game data from whatever the player has — `CyberPest.zip` (PKZIP is read
natively), `cyberpest.rar` (unpacked with the Windows built-in `tar.exe`), the installer's
`DATA.Z`, or an installed `PEST.ADM` — or, if the player chooses, downloads a preserved copy
of the 1996 shareware release from the Internet Archive (verified against a known SHA-1
before use). `tools/package.sh` builds the release zip; version 1.0.0 was published on
2026-10-17.

## Where the knowledge lives

| Doc | What it covers |
|---|---|
| [`PLAN.md`](PLAN.md) | architecture decisions, milestones, current status |
| [`formats/installer.md`](formats/installer.md) | the InstallShield 3 media and `DATA.Z` |
| [`formats/adm.md`](formats/adm.md) | `PEST.ADM`: container, modules, virtual files, FLC/WAV/BMP |
| [`formats/pest-exe.md`](formats/pest-exe.md) | the NE executable, imports, resources, dialogs |
| [`re/subsystems.md`](re/subsystems.md) | code map: I/O, graphics, sound, timing, UI |
| [`re/engine-overview.md`](re/engine-overview.md) | the module language, engine classes, sprite VM, INI keys |
| [`re/engine-runtime.md`](re/engine-runtime.md) | object hooks, input, movement, gravity, activation |
| [`re/state-commands.md`](re/state-commands.md) | the state-machine command language and runtime |
| [`re/classes.md`](re/classes.md) | the 20 special classes and their original functions |
| [`re/pest.md`](re/pest.md) | the pest's functions: behaviour, needs, learning, phone, save |
| [`re/names.tsv`](re/names.tsv) | function/global names applied to the Ghidra decompilation |
| [`AGENTS.md`](../AGENTS.md) | how to set up the toolchain and regenerate all analysis output |

## A note on the game itself

The remake contains none of the game: the repository holds only original code, tools and
notes (short quotes of disassembly where they explain a decision). The art, sounds and text
remain the property of their authors; the first-run import and the Internet Archive option
exist so that someone who owns the game — or its preserved shareware release — can use it.
