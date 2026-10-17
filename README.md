# CyberPest for modern Windows

A native Windows 10/11 remake of the engine of **CyberPest** (1996, Scratch 'n' Sniff /
EMG), the 16-bit Windows 3.1 desktop pet. It runs the original game data unchanged, with no
emulator or extra software. The game's art, sounds and texts are not included: you need the
original CyberPest game files (the CD or installer). On first run the remake can download a
preserved copy from the Internet Archive for you, or you can point it at your own.

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

## Building

C++20 with CMake and a MinGW-w64 or MSVC toolchain. The repository's scripts cross-compile
from Linux/WSL with llvm-mingw (`tools/setup_toolchain.sh`, then `source ~/tools/env.sh`):

```bash
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/llvm-mingw.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Command-line options for testing (headless snapshots, simulated clicks, data import) are
listed at the top of `src/main.cpp`.

## How it was made

The original `PEST.EXE` (Win16, MFC 2.x) was reverse engineered with Ghidra and custom NE
tools; the remake mirrors its object model and per-class behaviour, citing the original
function for each method. Start with `AGENTS.md`, `docs/PLAN.md` and `docs/CHECKPOINTS.md`;
format and code notes are in `docs/formats/` and `docs/re/`. No original game files or
assets are in this repository.
