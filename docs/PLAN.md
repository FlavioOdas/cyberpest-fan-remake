# Plan

## Architecture decision

- **Language/platform:** C++17 against the plain Win32 API. One static `.exe`, no runtime
  dependencies. Win32 is the natural target: the original is an MFC/Win16 app, and the
  "pet pops up over other applications" behaviour needs real desktop windows.
- **Graphics:** 8-bit DIB sections + `SetDIBColorTable` + `BitBlt`/`StretchBlt`. This is
  exactly what WinG did (WinG was the prototype of `CreateDIBSection`), so the original
  rendering model maps 1:1; we convert to 32-bit only at present time.
- **Sound:** a small software mixer feeding `waveOut` (replaces WAVMIX16's channel model:
  open wave, open channel, play, flush, close).
- **Data:** read the user's original `PEST.ADM` at runtime; never ship assets.
- **Build:** CMake; cross-compile from WSL with llvm-mingw (`~/tools`, see AGENTS.md), also buildable with MSVC.

## Milestones

| ID | Milestone | Done when |
|----|-----------|-----------|
| M0 | Repo, handoff docs, extraction tools | `tools/bootstrap.sh` reproduces `original/`; docs exist |
| M1 | `PEST.ADM` fully decoded | tool dumps every asset (sprites → PNG, WAVs, palette, tables) and the ADDM directory tree with names |
| M2 | `PEST.EXE` code map | annotated disassembly of all 8 code segments with imports resolved; subsystems identified (startup, ADM loader, sprite blitter, animation player, sound, pet AI/state machine, timers, save/load, UI dialogs/menus) |
| M3 | Engine skeleton | Win32 app opens, loads `PEST.ADM`, shows background + plays one animation with sound |
| M4 | Game logic port | pet behaviour, needs (hunger/health/mood), treatments, megaphone training, phone characters, Pestrologer, birth/certificate, save/load (`PEST.ADI`) |
| M5 | Desktop integration & polish | pop-up-over-apps behaviour, startup option, DPI/scaling, settings, packaging |

## Method for M2/M4 (behaviour fidelity)

Port logic function-by-function from the disassembly into readable C++, keeping a map
`docs/re/functions.md` (segment:offset → C++ function, status). Prefer faithful behaviour
over redesign; mark every deliberate deviation in that map.
