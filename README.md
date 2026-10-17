# CyberPest for modern Windows

> A fan project, made for fun. No profit, no ads, no donations, no affiliation with the
> original authors — just a 1996 desktop pet that deserves to keep living on modern Windows.

A native Windows 10/11 remake of the engine of **CyberPest** (1996, Scratch 'n' Sniff / EMG),
the 16-bit Windows 3.1 desktop pet. Rather than emulating Windows 3.1, the remake rebuilds the
game's engine in C++ and runs the **original game data unchanged** — the same animations,
sounds and text, read from the `PEST.ADM` produced by your own copy of the game. One `.exe`,
no emulator, no installer, no extra software.

The original game's data files, sounds and texts are **not included**: you need the original
CyberPest game files (the CD or installer). An optional experimental visual mod includes
a small amount of hand-corrected fan art; it cannot replace the original data. On first
run the remake can download a preserved copy from the Internet Archive for you, or import
files you already have.

**Download:** grab the zip from the [Releases page](https://github.com/FlavioOdas/cyberpest-fan-remake/releases/latest),
unzip it anywhere and run `cyberpest.exe`.

## Playing

1. Unzip the release anywhere and run `cyberpest.exe`. There is nothing to install.
2. On first run it asks where to get the original game data (which is not part of the
   remake). Choose either:
   - **Internet Archive** - it downloads a preserved copy of the 1996 shareware release
     from <https://archive.org/details/cyberpest> (about 11 MB), checks it against a known
     fingerprint and copies the game data to `%APPDATA%\CyberPest`. Nothing is downloaded
     unless you pick this.
   - **A file you already have** - the original `CyberPest.zip`, `cyberpest.rar`, `DATA.Z`
     from the installer, or a `PEST.ADM` from an installed copy (`SLOW\PEST.ADM` is the
     full-frame-rate one). They are imported directly; there is no need to unpack anything
     first.

   The data lands in `%APPDATA%\CyberPest\PEST.ADM`; you can delete the source file after.
   A `PEST.ADM` next to `cyberpest.exe` is used instead if present.
3. Fill in the certificate, then look after your pest.

Your pest is saved in `%APPDATA%\CyberPest\PEST.CFG`; delete that file to start over with
a newborn pest.

**Run when Windows starts:** right-click the title bar (or press Alt+Space) and tick
*Run CyberPest when Windows starts*, the same option the original installer offered.

**Graphics:** *Smooth graphics (HD)* is enabled by default. It enlarges the original
game picture with deterministic edge-aware pixel scaling, including the room, sprites,
text, cursor and emergency pop-up. It does not generate or replace artwork: the game
still reads your unchanged `PEST.ADM`. To see the original sharp pixels, untick it in
the title-bar menu (Alt+Space), or launch with `--classic` (`--hd` forces smoothing).

**Experimental art mod (work in progress):** Alt+Space → *Experimental hand-fitted art mod*
or `--mod` adds hand-corrected artwork on top of Smooth HD. This is an unofficial,
for-fun mod—not an automatic filter or a finished remaster. So far it replaces only the
room's idle megaphone. All other art remains Smooth HD, and the original `PEST.ADM` is
still required. The emergency pop-up currently uses Smooth HD in this mode.

**Experimental 60 FPS animation:** Alt+Space → *60 FPS animation (experimental, WIP)* or
launch with `--60fps` (combine with `--mod` if desired). While enabled, the game redraws
faster and glides moving objects between the positions of their original animation frames
instead of holding still and jumping. This is presentation only: original artwork, frame
timing, sounds, game logic, saves and hit-testing are unchanged — no new animation frames
are invented. Classic mode reaches up to about 60 redraws per second; in Smooth HD the 4×
upscaler currently limits redraws to about 27 per second, so the effect is smaller there.
Actual frame rate depends on your PC; the emergency pop-up is unaffected.

**Experimental generated in-between animation:** Alt+Space → *Generated in-between animation
(experimental, WIP)* or launch with `--gen-anim` (combines with the options above). The
character's pose is moved along the motion between each pair of original animation frames,
so the limbs travel through the window of each frame instead of snapping pose to pose. The
pictures are generated at run time from the original artwork only: changed pixels are
matched between the two poses and displaced; colours are never blended, and whatever cannot
be matched cleanly still changes at the halfway point — so no new art is shipped and the
palette look is preserved. Frame timing, cues, sounds, saves and hit-testing are again
untouched. Known limits (WIP): motion inside one frame is smoothed, but the moving positions
between frames still need *60 FPS animation* for full smoothness; single-pixel pose wobbles
cannot be split (there is no half pixel in the original resolution); the first time each
pose pair appears there is a tiny one-off computation (invisible on most PCs), and the wrap
from the last pose back to the first is still a hard cut.

When the pest needs you while you're using another program, a *CyberPest Emergency* window
pops up: **Attend** brings the game back, **Ignore** returns you to what you were doing.

### PEST.INI (optional)

Like the original, the remake reads `PEST.INI` from its own folder:

```ini
[Options]
Audio=0          ; start with sound off (the speaker button toggles it)

[Files]
ADB=D:\CYBERPST\SLOW\PEST.ADM   ; where the game data is

[Cheats]          ; the original developers' switches for a newborn pest
GoodStomach=50
JunkStomach=0
Bowels=0
MentalState=0     ; mood, -100..100
GotFleas=0
GotRabies=0
OnHeat=0
Neuroticism=-1    ; overrides the birth personality (the game's own range is 1000..9000); -1 = keep
Extroversion=-1
```

## What the remake covers

- **The room**, built from the game's own data: door, light, electrical outlet, the wall clock
  with the local time and the top-of-hour cuckoo, bin, shelf and ornaments, toys, food, pills,
  potion, powder, buttons, cursors and sounds — all interactive.
- **A pest that behaves like the original**: walking and turning, moods, hunger, hygiene,
  fleas, rabies, heat, the birth certificate with your typed name, food/toy preferences by
  birth hour, reward and punishment, megaphone word learning, and the window it lives in.
- **The phone**: all five callers (Pesterminator, Pestkeeper, Pestrologer, TheraPest, Vet)
  with their first-call and repeat-call routines.
- **Desktop integration**: the *CyberPest Emergency* pop-up over other applications, per-monitor
  DPI scaling with an optional classic 4:3 picture, launch options (minimised/maximised), and the
  "run when Windows starts" switch.
- **Your save** in a plain-text `%APPDATA%\CyberPest\PEST.CFG` (the original save format stays
  untouched; this one is the remake's own).

## Building

C++20 with CMake and a MinGW-w64 or MSVC toolchain. The repository's scripts cross-compile
from Linux/WSL with llvm-mingw (`tools/setup_toolchain.sh`, then `source ~/tools/env.sh`):

```bash
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/llvm-mingw.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Command-line options for testing (headless snapshots, simulated clicks and typing, data
import, Internet Archive fetch) are listed at the top of `src/main.cpp`; `tools/snap.sh`
builds and runs the game headlessly with scripted input. `tools/package.sh [version]` builds
the release zip.

## How it was made

The original `PEST.EXE` is a 16-bit NE executable (statically linked MFC 2.x, WinG graphics,
WaveMix16 sound) with no source available. It was reverse engineered with Ghidra and custom
NE/ADM analysis tools; the remake mirrors the original object model and ports each behaviour
from the decompiled function, recording the original address for every method. The story —
from unpacking the InstallShield media to decoding `PEST.ADM`, making the decompiler usable,
porting behaviour function by function and testing it headlessly — is told in
[`docs/MAKING-OF.md`](https://github.com/FlavioOdas/cyberpest-fan-remake/blob/main/docs/MAKING-OF.md).

Where the knowledge lives:

- [`docs/MAKING-OF.md`](https://github.com/FlavioOdas/cyberpest-fan-remake/blob/main/docs/MAKING-OF.md) — how the remake was achieved
- [`docs/PLAN.md`](https://github.com/FlavioOdas/cyberpest-fan-remake/blob/main/docs/PLAN.md) — architecture and milestones
- [`docs/formats/`](https://github.com/FlavioOdas/cyberpest-fan-remake/tree/main/docs/formats) — `PEST.ADM`, `DATA.Z`, `PEST.EXE` formats
- [`docs/re/`](https://github.com/FlavioOdas/cyberpest-fan-remake/tree/main/docs/re) — code findings: runtime, state commands, classes, pest
- [`AGENTS.md`](https://github.com/FlavioOdas/cyberpest-fan-remake/blob/main/AGENTS.md) — toolchain and how to regenerate the analysis

## Repository layout

- `src/` — the remake (C++20, Win32; builds to one self-contained exe)
- `tools/` — reverse-engineering, extraction and test tools (Python standard library only)
- `docs/` — plan, making-of, format and code notes
- `mods/experimental/` — optional, hand-fitted mod artwork embedded in the exe
- `original/` and `work/` — local, gitignored: your game copy's extracted files and generated
  analysis output. Neither is ever committed.

This is an unofficial fan project, not affiliated with the original authors. Original game
files are not part of this repository; players import their own. The experimental mod art
is a small, separately identified fan-made derivative—not official CyberPest artwork.
