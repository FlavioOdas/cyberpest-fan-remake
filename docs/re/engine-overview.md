# Engine overview (from PEST.EXE strings + Ghidra, M2)

Ghidra selectors → NE segments: `1000`=seg1, `1008`=seg2, `1010`=seg3, `1018`=seg4,
`1020`=seg5, `1028`=seg6, `1030`=seg7, `1038`=seg8, `1040/1048`=seg9/10 (data),
`1050`=seg11 (DGROUP), `1218`=Ghidra's import thunks. Decompiled C: `work/ghidra/pest.c`
(2,217 functions, 0 failures).

## Data model: "modules" (observed strings; mapping to tags inferred)

`PEST.ADM` is the binary form of a text **module** language. Loader error strings:
"Unexpected module definition", "Unexpected text block definition", "Unexpected data list
definition", "Invalid imported module definition", "Can't have an anonymous top level module",
"Module ID not found", "Attempt to auto-unload module which isn't auto-loaded",
"Data list type mismatch", "Invalid data list index", "Invalid text block line number",
"Bad #define substitution", "Module cache error", "Error reading binary module".
Directives: `include`, `define`, `output`, `inpath`, `outpath`.

Likely mapping: `ADDM` = module, `ADDL` = data list (indexable values), `ADVI` = integer
value, `ADVS` = string value / text line, `NDVE` = file value. Modules can be imported and
auto-loaded/unloaded (a cache with `MaxCacheSize`).

## Engine classes (MFC-style runtime class names)

`mObject, mBitmap, mDirector, mFile, mImage, mImageBitmap, mImageBMP, mImageFlic, mImageAVI,
mRenderer, mRenderSolid, mRenderBMP, mRenderFlic, mSprite, mSpriteImage, mAudioWAV, mScript,
mImagePCX, mImageDCX, mSpriteGroup` (+ ` mAudio`). Image types: BMP, FLI, FLC, PCX, DCX, LOD.
Image description keywords: `FILE, RECT, TILE, SOURCEFRAME, ORIGIN, TIME, DELTA, END`.

## Sprite script VM (mScript) opcodes

`NOP, LABEL, FRAME, ADVANCEFRAME, DELAY, SPEED, PAUSE, POSX, POSY, POSZ, MOVEX, MOVEY, MOVEZ,
HIDE, SHOW, WAVEFILE, SOUND, SET, CLEAR, IFNOT, IFFRAME, IFHIT, IFHITID, IFHITMASK, HITID,
HITMASK, HITDEPTH, GOTO, LOOP, ENDCYCLE, END`. Errors: "Label doesn't exist", "Missing
parameter", "Value out of range", "Bad ProgInst Value", "Unknown Object".

## PEST.INI keys

- `[Files] ADB=<path to PEST.ADM>`, `AsyncLoad`
- `[Options] Audio`
- `[Cache] CacheUsage`, `MaxCacheSize`
- `[Cheats]` (developer switches — useful to validate the remake against the original):
  `NoDefaultAction, ForcedMoveType, FollowRightClicks, ShowObjectCoords, KeyBehaviours,
  CreateMouse, AdvanceTime, ShowHitAreas, ShowCells, RestoreHousehold, TraceMessages,
  TraceStates, TraceAnimCount, TraceBehaviours, TraceCache, TraceOther, Neuroticism,
  Extroversion, SetPersonality, DumpPersonality, GoodStomach, JunkStomach, Bowels,
  MentalState, GotFleas, GotRabies, OnHeat`
- `[Intl] sShortDate, sTime` (read from WIN.INI for date display)

Details (2026-10-17, Claude; `App_ReadIniSettings 1000:28ce`, observed): the file is
`<current directory>\PEST.INI` (string resource 4). `Ini_GetInt(app, default, key,
section)` stores into theApp, a static object at DS:0x26dc, so other code reads the
values as globals `DAT_1050_(0x26dc + offset)`. Defaults are 0 except `[Options] Audio`
= 1, `GoodStomach` = 50 and `Neuroticism`/`Extroversion` = -1 (no override).

| Key | theApp | Used by |
|---|---|---|
| Audio | +0xcc | audio button -94 / mixer mute |
| GoodStomach, JunkStomach, Bowels, MentalState, GotFleas, GotRabies, OnHeat | +0x2be..+0x2ca | pest init `1008:8a3e`; GoodStomach/JunkStomach/Bowels also the overfull reset on -226 (`1008:5b54`) |
| Neuroticism, Extroversion | +0x2b6/+0x2b8 | `1008:8dca`: override current personality when ≥ 0 (birth value kept) |
| FollowRightClicks | +0x29a | `1000:1206` right-button handling |
| KeyBehaviours, CreateMouse, AdvanceTime, RestoreHousehold, SetPersonality, DumpPersonality | +0x29e.. | developer keys in `1000:14d0` (Esc restores household; Space advances time; Ctrl/Shift letters force behaviours; M mouse; P/D personality) |
| ShowHitAreas, ShowCells | +0x2a4/+0x2a6 | `Runner_DebugDrawHitAreas 1000:a6e2` |
| Trace*, NoDefaultAction, ForcedMoveType, AsyncLoad, Cache* | | diagnostics / loading |

The remake reads `PEST.INI` next to the exe: `[Files] ADB` (used when no `--adm` is
given and the file exists), `[Options] Audio` and the pest cheats in the first three rows.
Values are parsed as signed (GetPrivateProfileInt would turn negatives into 0). The
developer keys, debug drawing and trace switches are not ported. Windows' INI API does
not read the file when the exe runs from a `\\wsl$` path; test from a Windows folder.

## Pest model hints

Personality axes **Neuroticism** and **Extroversion** (Eysenck model), set by birth hour
(Pestrologer texts in ADM). Body/health state: stomach (good/junk), bowels, mental state,
fleas, rabies, on heat. Debug output: "Household Behaviour=%s", "Default Behaviour=%s",
"%d: %s (x%d)", "%s %s (%d%%)" (behaviour selection with weights/percentages).
