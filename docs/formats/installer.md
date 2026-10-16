# Install media (InstallShield 3)

Source: `CyberPest.zip` = CD/disk contents. `SETUP.EXE` + `_INST16.EX_` are 16-bit
InstallShield 3 (Stirling, 1994). Game files are inside `DATA.Z`.

## DATA.Z layout (observed; `tools/is3x.py` extracts all 12 files with matching sizes)

- Header (255 bytes): magic `13 5D 65 8C`; `u16 file_count @0x0C`; `u32 archive_size @0x12`;
  `u32 toc_offset @0x29`; `u16 dir_count @0x31`.
- At `toc_offset`: `dir_count` directory records: `u16 file_count, u16 record_size,
  u16 name_len, name…` (records padded to `record_size`).
- Then `file_count` file records, in directory order: `u8 ?, u16 ?, u32 uncompressed_size,
  u32 compressed_size, u32 data_offset, u32 dos_datetime, u32 attrs, u16 record_size @+0x17,
  u32 ?, u8 name_len @+0x1D, name @+0x1E`.
- Each file's data is one PKWARE DCL "implode" stream (binary literals, dict 4–6 bits),
  decoded with a port of zlib's `contrib/blast`.

## Contents → install location

| Archive dir | Files | Installed to |
|---|---|---|
| SYSTEM | WINGPAL.WND, WING32.DLL, WINGDE.DLL, WINGDIB.DRV, WING.DLL, DVA.386, WAVMIX16.DLL | `\WINDOWS\SYSTEM` |
| APP | PEST.EXE, README.TXT | `<target>` |
| APP\SLOW | PEST.ADM (17,330,506 B) — "high spec": ~2× animation frames | `<target>` (one of SLOW/FAST) |
| APP\FAST | PEST.ADM (12,171,688 B) — "low spec" | `<target>` |
| WINDOWS | WAVEMIX.INI | `\WINDOWS` |

## What SETUP.INS writes (from its strings; order observed, exact values partly inferred)

- `SYSTEM.INI [386Enh] device=dva.386` — Win3.x VxD, irrelevant to the remake.
- `PEST.INI [Files] ADB=<path to PEST.ADM>` (key `ADB`).
- `PEST.INI [Cache] CacheUsage=…`, `MaxCacheSize=2048`.
- `WAVEMIX.INI [default] WaveBlockLen=2048`.
- Program group + optional StartUp entry. Version string `1.1a`; README says 1.11.
