# PEST.EXE (observed with `tools/ne_dump.py`)

- 16-bit NE, Windows 3.10, linker 5.60 (MS Visual C++ 1.5x), module `CYBERPEST`,
  statically linked MFC 2.x (strings: `CDialog`, `CPalette`, `CMemoryException`, `_AFX_VERSION`).
  Copyright string mentions "Extroversion".
- Entry CS:IP = 7:038A. Segments: 1–8 CODE (~412 KB total), 9–11 DATA (DGROUP = 11?).
- Imports by ordinal:
  - **WING** (from WING.DLL exports): 1001 WinGCreateDC, 1002 WinGRecommendDIBFormat,
    1003 WinGCreateBitmap, 1006 WinGSetDIBColorTable, 1010 WinGBitBlt.
  - **WAVMIX16**: 3 WaveMixInit, 4 WaveMixActivate, 5 WaveMixOpenWave, 6 WaveMixOpenChannel,
    7 WaveMixPlay, 8 WaveMixFlushChannel, 9 WaveMixCloseChannel, 10 WaveMixFreeWave,
    11 WaveMixCloseSession, 15 WaveMixConfigureInit.
  - **MMSYSTEM**: 401–416 (waveOut* range), 607 (timeGetTime), 1210–1219 (mmio* — RIFF I/O,
    used for PEST.ADM). Names from standard MMSYSTEM ordinal numbering — verify in M2.
  - **SHELL** 11, 12 (DragQueryFile, DragFinish — inferred from standard ordinals).
  - **COMMDLG** 27, **KEYBOARD** 5, 6, **WIN87EM** 1, plus KERNEL (47), GDI (42), USER (129).
- Files referenced: `PEST.INI`, `PEST.ADI` (likely the saved pet), `INSTALL.INI`,
  `PEST.CFG`, `PEST.TMP`, `PALETTE.BMP` (inside ADM).
- Error strings mention "Application Database" (the ADM), "No WinG support",
  CD-ROM messages (likely generic engine code; all data is installed to disk).
- Resources: 13 cursors (+groups), 2 icons, 1 bitmap (30989), 1 menu, 4 dialogs
  (100, 103, 106, 30721), 18 string tables, accelerators, version.

## Resources (`tools/ne_resources.py`)

- String table ids: 2 doc template, 3 `CyberPest`, 4 `PEST.INI`, 5 `PEST.ADI`, 6 "Can't load
  module.", 7 "Can't activate screen.", 8–9 ADM error prefix/"Abort now?", 10–11 CD-ROM errors,
  12–13 out of memory, 14 video (VfW) error, 15 audio error, 16 files not found,
  17–19 `PestGetTextColour` / `PestGetBackColour` / `PestGetBackBrush` (registered window
  messages, inferred), 20 "Bad data file version!", 21 `INSTALL.INI`, 22 data load failure,
  23 `PEST.CFG`, 24 `PEST.TMP`. Ids ≥ 50000 are MFC's standard strings.
- Menu 2: standard MFC File/Edit/Help (probably unused/hidden in the shipped UI — verify).
- Dialog 100 About ("Copyright © EMG 1996"); 103 "Where shall we go now?" (developer screen
  jump, edit box id 1000); 106 "Pest Personality" (cheat: Neuroticism/Extroversion scrollbars
  1001/1002, value labels 1003/1004); 30721 MFC "New" document dialog.
