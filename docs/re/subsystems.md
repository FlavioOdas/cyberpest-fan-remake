# PEST.EXE subsystem map (M2, in progress)

Labels are `sub_<segment>_<offset>` from `tools/ne_disasm.py` (`work/disasm/`). Ghidra uses
its own names (`FUN_<selector>_<offset>`); map by segment number + offset.

Compiler: MSVC 1.5x, **unoptimised** (`cmp ax,0 / je / jmp` patterns), large model (far
data pointers via `les bx,[bp+..]`). Most functions begin with `mov ax,<locals>; call
sub_7_067c` → `sub_7_067c` is the C runtime stack check (`__chkstk`).

## Located by API usage (observed)

| Area | Functions | Evidence |
|---|---|---|
| ADM file I/O | `sub_6_0132` (open: mmioOpen/mmioSetBuffer/mmioGetInfo/mmioSeek), `sub_6_0488`, `sub_6_0912`, `sub_6_0ba8` (read: mmioAdvance/_lread), `sub_6_1140` (write), `sub_6_12e8` (seek), `sub_6_15fa` (close: mmioClose) | MMSYSTEM mmio* imports |
| ADM chunk parsing | seg5 `0x287b..0x2c52`, `0x341d` (compares `'AD'`/`'DM'`/`'DL'`/`'VI'`), seg4 `0xa1d2..0xa80b`, seg7 `0x2ac1` | four-CC constants |
| Graphics (WinG) | seg5 `0x878a` RecommendDIBFormat, `0x8947` CreateDC, `0x89bc` CreateBitmap, `0x8de5` SetDIBColorTable, `0xa23b` WinGBitBlt | WING imports |
| Palette | `sub_5_4888` (CreatePalette, GetSystemPaletteEntries, StretchDIBits), `sub_5_b6be` (AnimatePalette, RealizePalette) | GDI imports |
| Sound (WaveMix) | seg7 `0x0036..0x0366` (Init, ConfigureInit, OpenChannel, OpenWave, Play, FreeWave, CloseSession), seg8 `0x2071` Activate, `0x5d81` FlushChannel | WAVMIX16 imports |
| Sound (direct) | `sub_5_64fe` waveOutOpen/Restart + RegisterClass/CreateWindow | MMSYSTEM |
| Settings | `sub_4_4ea2` GetPrivateProfileString; `sub_4_055a` WritePrivateProfileString (+ MFC app init) | KERNEL |
| Timing | `sub_8_6ffc` timeGetTime | MMSYSTEM |
| Error dialogs | `sub_6_bae4` (LoadString + MessageBox) | USER |
