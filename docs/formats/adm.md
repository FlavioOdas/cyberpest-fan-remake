# PEST.ADM ("Application Database") — work in progress (M1)

Observed so far (`APP/SLOW/PEST.ADM`, 17,330,506 B; FAST has the same chunk counts):

- RIFF file, form type `ADBM`. Top-level chunks are **not** word-padded (walk with
  `offset += 8 + size`, no alignment); this walk lands exactly on EOF.
- 95 `ADDM` chunks (directory/metadata; contain nested `ADDL`/`ADDM`/`ADVI`/`NDVE` tags),
  85 `ADFI` chunks (file payloads), 1 trailing `RIFF` form `ADBB` (1,552 B; subchunks
  `ADBF`, `ADBI` — probably the index).
- `ADFI` payloads:
  - 3 × standard Windows BMP, 8 bpp (one is named `PALETTE.BMP` in ADDM data).
  - 82 × custom sprite/animation sets: `u32 ?, u16 0xAF12 (tag), u16 frame_count?, u16 w,
    u16 h, u16 8, u16 3 …` — some are 640×480. The `u16` after the tag is larger in SLOW
    than FAST for the same set (e.g. 9 vs 5) → frame count.
  - 100 standard `RIFF/WAVE` sounds embedded inside the sets: PCM, mono, 11025 Hz, 8-bit.

Next: decode ADDM record structure (names, ids, hierarchy), the 0xAF12 sprite format
(compression, per-frame offsets/hotspots), and how sounds attach to frames.
