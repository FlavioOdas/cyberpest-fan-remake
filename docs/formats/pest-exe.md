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
