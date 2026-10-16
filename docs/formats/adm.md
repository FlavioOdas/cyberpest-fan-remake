# PEST.ADM ("Application Database")

Tools: `tools/admlib.py` (reader), `tools/flc.py` (FLC decoder), `tools/adm_dump.py`
(dumps `tree.json`, `manifest.json`, every virtual file, and FLC frame strips as PNG into
`work/adm/`). All facts below are observed on `APP/SLOW/PEST.ADM` unless marked.

## Container

- RIFF, form `ADBM`. Chunks are **not** word-padded (`next = offset + 8 + size`).
- The file ends with a separate `RIFF` form `ADBB` (1,552 B; subchunks `ADBF`, `ADBI`) —
  probably an id → offset index. **Not decoded yet.**
- Everything before it is a sequence of top-level chunks forming a tree:

| Tag | Kind | Payload |
|---|---|---|
| `ADDM` | container (759) | `i32 id, i32 ref, u32 child_count`, then children |
| `ADDL` | container (4653) | same header as ADDM, then children (usually values) |
| `ADVI` | value (8741) | `i32` |
| `ADVS` | value (505) | `u32 len, char[len]` (sometimes NUL-terminated) — UI text, font names, Pestrologer predictions |
| `NDVE` | value (872) | virtual file entry: `u32 absolute_offset, u32 size, u32 name_len, name` |
| `ADFI` | blob (85) | raw bytes; the virtual files referenced by NDVE live inside these |

Ids are mostly negative and appear to be references between nodes (e.g. an `ADDM` with
`ref=-69` and children whose `ADVI` values are other ids). **Semantics of id/ref are not
yet understood** — needs the loader code in PEST.EXE (M2).

## Virtual files (NDVE)

872 references → 688 unique by offset: 578 FLC, 100 WAV, 10 BMP. Every referenced
range has the right signature for its extension (verified).

- **FLC** = standard Autodesk Animator FLC (magic `0xAF12`, 128-byte header). Chunk types
  used: 4 COLOR_256, 15 BYTE_RUN, 7 DELTA_FLC, 18 PSTAMP. All decoded by `tools/flc.py`;
  frames verified visually (backgrounds, pest animations).
- **Palette:** `PALETTE.BMP` (8-bit BMP) is the master palette. **Index 0 = (0,255,0) pure
  green = transparency key** for sprites. Each FLC carries its own COLOR_256 chunk; 22 match
  the master exactly, 555 differ only in entries the art does not use (low indices shared).
- **WAV:** PCM mono 11,025 Hz 8-bit (sampled). Names pair with animations
  (`PBEAT.FLC` + `PBEAT.WAV`), suggesting sound is triggered with the animation.
- Name prefixes group assets: `CC*` cursors, `PB*` pest behaviours, `OE*` objects/events,
  `M0..M6*`, `H0/H1*`, city backdrops (`PARIS.FLC`, `MOSCOW.FLC`, …) for the window view.
- SLOW vs FAST: same tree shape and names; SLOW animations have ~2× frames ("high spec").
