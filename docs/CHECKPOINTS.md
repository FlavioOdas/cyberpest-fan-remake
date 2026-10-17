# Checkpoints

Append a dated entry after each meaningful step and keep "Current state" accurate.
Any agent (Claude, Codex, …) can resume from here.

## Current state (handoff — read this first)

**Next request: true 60 FPS animation (2026-10-17, Codex; handoff only):** The
user clarified that `--60fps` should eventually show *additional animation
poses and genuinely smoother motion*, not merely repeat the same ADM frame at
a 60 Hz presentation rate. No code was changed for this request. Current
`--60fps` from commit `b3d7171` is **presentation-only**, as README already
says. Keep the faithful Classic/HD engine and original FLC frame/event timing
unchanged; new in-betweens belong to the explicitly experimental, opt-in mod.
Do not advertise the current option as true 60 FPS animation. The original
ADM cannot supply missing poses by itself, and simply increasing `World::tick`
or shortening `Flc::frameDelayMs()` would speed up gameplay and sound/cues.

**Technical starting points for true animation:** `src/main.cpp:534` runs the
real-time world tick and `src/main.cpp:536` only schedules 17/17/16 ms redraws.
`GameObject::step` at `src/object.cpp:427` advances each FLC frame according to
its own delay, applies per-frame motion at `src/object.cpp:449`, and triggers
sound/cues at the original boundaries. `World::draw` at `src/world.cpp:175`
currently draws only the integer `s.frame` at the integer object position; it
receives no presentation time. A true smooth-motion mod needs a separate
render-only phase/pose for the interval between original frames, and additional
sprite images (or carefully reviewed sprite-specific interpolation) for poses
that deform. Preserve original cue, hit-test, save, AI, physics, and audio
timing. Decide how rendered-position/hit-test alignment works before enabling
interpolated translation on draggable objects. The 4× HD scaler benchmark was
about 15.12 ms/frame on synthetic Linux input, leaving little of a 16.67 ms
frame budget for extra work; measure the live Windows pipeline and cache or
optimize before claiming sustained 60 FPS.

**Ordered prototype/acceptance:** (1) Measure actual live presentation intervals
on the target Windows PC and capture a walking-pest sequence; separate missed
presentation deadlines from duplicate ADM frames. (2) Prototype one pest walk
cycle: record frame start/duration without changing `GameObject::step`, draw
intermediate positions and hand-authored/reviewed additional poses through a
separate 32-bit mod layer before overlapping sprites/cursor. Avoid whole-screen
blends/automatic smoothing (the user rejected that look) and never bundle
original extracted frames. (3) Verify 60 *visibly distinct* movement poses/s
where motion exists, stable original wall-clock animation duration, cue/sound
counts, hit-testing, transparency and z-order, then expand asset-by-asset
(including the nine-frame `OOBOWL.FLC` fish bowl). (4) Only after visual review
and a sustained live FPS measurement, rename the option to distinguish true
animation from presentation-only; otherwise keep the honest existing label.
Snapshots alone cannot establish live frame rate; no true-animation prototype
or measurements have been made yet.

**Experimental 60 FPS presentation (2026-10-17, Codex):** After the earlier 85%
usage-guard stop, the user explicitly resumed work on a separate 60 FPS option,
with a new stop threshold of 93%. `src/main.cpp` now offers `--60fps` and an
Alt+Space "60 FPS presentation (experimental)" checkbox. It is independent of
Classic/HD/art-mod selection; combine `--mod --60fps` to use both experiments.
The live renderer targets a 17/17/16 ms cadence (60 redraws/s), waits only until
the next deadline, and requests 1 ms Windows timer resolution while enabled,
releasing it when disabled or on exit. `World::tick` still receives real elapsed
time, and original FLC frame delays/movement steps remain unchanged: this is
**not** 60 new animation poses or motion interpolation. The emergency popup is
unaffected. README explains the distinction. Release build and `git diff --check`
passed; fresh 20-second `--mod --60fps` and `--mod` snapshots were byte-identical
(2560×1920), with pest -105 at (486,265,3041). The snapshot harness does not
exercise the live pacing loop; an actual 60 FPS measurement and physical menu
toggle test are still outstanding. The option is a target, not a guarantee on
slower machines. The fish-bowl mod below remains unimplemented.

**Usage-guard stop (2026-10-17, Codex):** The user's 5-hour primary Codex usage
reached **85%** while beginning the next experimental art-mod asset. Development
stopped immediately; do not infer that the fish bowl was implemented. The last
source commit is `c7dbe3a` (single hand-fitted idle megaphone); the source worktree
was clean before this checkpoint. `main` was two commits ahead of `origin/main`
before checkpoint/push (`83a3bb6` HD display and `c7dbe3a` mod). The user's
playable `C:\Users\odasf\CyberPest-Remake\cyberpest.exe` was already refreshed
from the verified `c7dbe3a` build; no new executable was built in this stop turn.
`original/` and `work/` remain ignored and must not be staged. This checkpoint is
explicitly committed at the user's request despite the ordinary AGENTS.md rule
that keeps the working log untracked. Resume implementation only when asked.

**Fish-bowl investigation, no implementation:** The on-shelf asset is
`OOBOWL.FLC`, 46×49, nine visible frames at 85 ms/frame; decoder also yields a
tenth ring frame identical to frame 0. Frame-by-frame bytes in the ignored ADM
dump showed **1,596 of 2,254 pixel positions identical across all nine frames**;
all changing pixels lie within inclusive local rectangle x=5..40, y=8..39.
The bowl shell can therefore be fitted once while preserving the separately
moving fish/water pixels. The ignored contact sheet was visually inspected:
the fish changes pose/direction substantially, so a static whole-sprite
replacement would be wrong. At the 20-second hatch-room snapshot the bowl object
is -85, class -274, at (180,171,3000), frame 6. `FISH.FLC` (42×35) and
`BIGFISH.FLC` (62×48) are separate assets, not this on-shelf bowl. No asset or
renderer files were changed during this investigation; these are observed ADM
and snapshot facts, not original-code claims.

**Immediate next actions, after an explicit resume:** (1) Fit a deterministic,
hand-authored shared bowl shell from all nine `OOBOWL.FLC` frames, preserving
their original fish animation; review a side-by-side proof at actual game size
before game integration. (2) Generalize the experimental mod's single
`Surface::ModRegion`/megaphone-only resource path to cover this animated asset
without disturbing z-order or pixels outside each corrected sprite rectangle.
(3) Build and compare fresh 20-second `--hd`/`--mod` room snapshots, verify
all nine bowl frames over time, plus classic and popup regression snapshots;
physically test the Alt+Space toggle and overlapping objects. (4) Commit only
validated authored mod source/resources, update the playable exe, and clearly
keep the mode labeled unofficial/WIP. (5) Continue the separate pest/engine
audit and investigate ground-object behavior only with reproducible evidence.

**Known implementation/evidence boundaries:** The native remake itself is
playable; the pest entrance was established from original show/hide setter
`1000:7b80`, show/hide `1028:dbf0`/`1028:dc68`, draw gate `1028:cce8`, and
hidden-frame advance `1028:ca78`, then verified by 5/10/20-second dumps.
Pest dragging remains intentionally unsupported pending contrary evidence:
`1000:5e1a` clears pickable +0xb6; the -105 module does not set property -180;
`1000:6e5c` requires +0xb6 to hold; a temporary trace saw focus=-105,
pickable=0, held=0. These original-address findings are unrelated to the
fish-bowl art task; see `docs/re/pest.md` and `docs/re/engine-runtime.md` for
details and confidence. The experimental art mod has no claimed original
function mapping: it is a display-only overlay on the faithful engine.

**Validation and commands:** Previous validated `c7dbe3a` build passed the
Release build, `git diff --check`, fresh 20-second room snapshots (HD and mod
2560×1920, mod-vs-HD 16,990 changed pixels inside only the 480×480 megaphone
rectangle and zero outside), classic room (640×480), and mod popup
(1328×1360). No new fish-bowl build, run, or visual proof exists. To rebuild:
`source ~/tools/env.sh`; configure once with
`cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/llvm-mingw.cmake -DCMAKE_BUILD_TYPE=Release`;
then `tools/snap.sh --fresh --mod --time 20000 --click 560,290,800 --dump`
and inspect `work/test/snap.png`/dump; compare with the same `--hd` command.
Headless saves are under `%TEMP%\CyberPest-test\PEST.CFG`, not the user's
`%APPDATA%` save. The main blockers are nine-frame manual art work, extending
the one-region mod compositing safely, and untested physical menu/overlap paths.

**Where things stand (2026-10-17, opencode): Release 1.0.0 published.** The first-run import
takes the player's own file in any form (`PEST.ADM`, `DATA.Z`, `CyberPest.zip` read natively,
`cyberpest.rar`/other archives via Windows' `tar.exe`) and offers an opt-in download from the
Internet Archive (the remake ships no game data); the repo README was expanded and
`docs/MAKING-OF.md` added; `tools/package.sh 1.0.0` builds
`work/dist/CyberPest-remake-1.0.0.zip`. See the newest log entries for verification evidence.
Everything below (engine work) is unchanged.

**Optional non-AI HD display (2026-10-17, Codex):** A deterministic 4× Scale2x-plus-light-edge-
blend pass now smooths the fully composed 640×480 room, including every ADM sprite, text and
game cursor. The separate 332×340 emergency popup is composited and processed by the same
scaler, then shown in a screen-fitting (usually 2×) window with original hit rectangles
mapped to the new size. Live play defaults to HD with a larger window; Alt+Space → "Smooth
graphics (HD)" toggles original pixels, and `--hd`/`--classic` select modes at launch.
Headless snapshots remain classic by default so existing tests do not change. No game art is
generated or rewritten, and the executable still reads the player's unchanged PEST.ADM.
The HD live path is capped at roughly 30 FPS; isolated Linux `-O3` scaler benchmark measured
15.12 ms/frame for synthetic 640×480 pixels. `tools/snap.sh --fresh --hd --time 20000
--click 560,290,800 --dump` yielded a 2560×1920 frame with pest -105 at (486,265,3041);
classic yielded 640×480. HD popup `--hd --popup -601` yielded 1328×1360, classic 332×340.
Images were visually inspected. Source changes are in `src/upscale.*`, `src/main.cpp`,
`src/attention.*`, `src/surface.*`, `tools/png.py`, `tools/snap.sh`, CMake and README. Next:
check live motion and popup hit testing on a physical desktop, then refine the optional
filter if desired; ground-object behavior and broader original-code audit remain separate.

**Experimental art mod begins (2026-10-17, Codex):** The user explicitly permitted
committing hand-authored derivative vector corrections as an unofficial, for-fun mod.
`AGENTS.md` now permits those resources under `mods/experimental/` while still keeping
original game data and bulk extractions ignored. A third opt-in `--mod` mode and
Alt+Space checkbox embeds the approved hand-fitted idle megaphone (`megaphone.svg`
and 4× transparent PNG) inside the single exe. It uses the original unchanged ADM
and keeps all other pixels on the existing Smooth HD path; no broad Snapchat-style
filter remains. In-game 20-second HD-vs-mod snapshots differed in 16,990 pixels
inside the megaphone's 480×480 output rectangle and **zero pixels outside it**.
Both screenshots were visually checked, as was the side-by-side preview in this
Codex task's `outputs/megaphone-in-game-mod.png`. Classic snapshot remains 640×480,
mod room 2560×1920, mod emergency popup 1328×1360 (using Smooth HD). Build and
`git diff --check` pass. The system-menu toggle has not been physically clicked
yet. Commit `c7dbe3a` contains
the first asset and the worktree is clean; the verified build was copied to
`C:\Users\odasf\CyberPest-Remake\cyberpest.exe` (SHA-256 matched). No push was made.
The mode is explicitly WIP:
only idle `MEGA.FLC` is remastered; pressed/animated states and every other asset
remain Smooth HD. Next: physically test the toggle and overlap behavior, then
author other sprites one at a time, beginning with the fish bowl. The on-shelf bowl
is `OOBOWL.FLC`, 46×49 with 9 frames; the 20-second room uses frame 6. Its bowl
outline can be shared but the fish moves, so all nine frames need visual review.
`FISH.FLC` is a separate 42×35 single-frame asset; `BIGFISH.FLC` is 62×48 and
not the on-shelf bowl. Never present this first-asset milestone as a completed
whole-game remaster.

**Pest pickup report (2026-10-17, Codex):** The user's mouse-down reaches the pest but
does not pick it up. This appears faithful: original `1000:5e1a` clears pickable field
+0xb6, module -105 never sets property -180, and `1000:6e5c` only starts holding when
+0xb6 is set. A temporary trace of the fresh 5.2-second snapshot's drag at (145,270)
showed focus=-105, hit area=-114, pickable=0, held=0; item -338 moved with the same
drag harness. The trace was removed and only `docs/re/pest.md` was changed. Do not
"fix" pest dragging without new evidence that the original supports it. The user's
separate report of odd ground-object behavior has not yet been reproduced.

**Where things stood before (2026-10-17, Claude):**

- The remake (`src/`) builds to one native Windows exe and runs the original data: certificate →
  click/Enter → hatch opens → full room with working objects (door, light, clock, mouse, shelf
  ornaments, buttons), game cursors, sounds. Engine mirrors the original object model; semantics
  in `docs/re/engine-runtime.md` and `docs/re/state-commands.md`.
- **The pest enters and walks.** Property -322 was misidentified: it means visible (1), not
  paused. Corrected show/hide semantics in `src/object.*` and rendering in `src/world.cpp`.
  The original hatch message -153 starts ordinary walking; no extra wake-up message is needed.
  The requested 20-second snapshot shows the pest inside the room, and 5/10/20-second dumps
  verify changing positions. `docs/re/engine-runtime.md` records the original show/hide evidence.
- The pest's original-code audit repaired interaction steps 10–12, newborn hunger (50),
  word validation/case handling, reinforcement, birth-row error handling, one-shot
  AdvanceTime, and several event branches. Details/evidence are in `docs/re/pest.md`.
  The broader 96-function audit and INI cheats remain to do.
- **Five shelf-related special classes are ported:** -274/-558 shelf items, -337/-587
  carry items and -129 wrapper in `src/items.*`. Their timers, landing/shelf snap,
  pest messages, home placement, large/small dragging and wrapper cleanup follow the
  original hooks listed in `docs/re/classes.md`. A 3.5-second `--drag` snapshot moved
  food -338 from its tray into the room at (320,320,3096), and the 20-second entrance
  snapshot still shows the pest walking inside the room.
- **Certificate name entry and date/time text now work.** Class -593 uses text rectangles,
  font and colors from PEST.ADM, accepts typed input and saves the name via the pest's
  original setter path; an existing named pest skips the certificate. Class -189's
  megaphone panel shows the pest's name (read-only, dark red) above the typed-words
  field, which passes its text to pest learning (both controls of `1010:0f6e`). `src/text.*` provides a shared
  GDI-to-indexed-surface text overlay, including headless snapshots. Details and
  approximations are in `docs/re/classes.md` and `docs/re/engine-runtime.md`.
- **Phone interaction and readable caller bubbles now work.** Class -98 forwards hovered
  caller ids to the phone avatar; class -188 renders ADM text with line wrapping and
  closes through its original states. First-call greeting flags are persisted as booleans.
  All five callers' repeat-call selection now follows the original functions
  (TheraPest `1008:ced0` and Vet `1008:d6a4` ported exactly on 2026-10-17). Verified
  greetings and repeat calls for Pesterminator, Pestrologer, Vet and TheraPest by snapshot.
  See `src/pest_phone.cpp` and `docs/re/classes.md`.
- **The wall clock now follows local time.** Class -90 covers body -90, hour hand
  -88, minute hand -89 and hourly cuckoo -521. The original hand states have
  repeated id=0; `GameObject::enterStateAt` selects them by ordinal. A snapshot
  at 22:10 displayed H10/M10; pest movement continued. See `src/clock.*` and
  `docs/re/classes.md`.
- **The bin now captures and disposes of held objects.** Class -78 checks its -158
  opening hit area and -280 eligibility flag, moves qualifying held objects behind
  the bin, plays -160/-161, then sends -1 to remove them. A shelf bowl drag showed
  capture at 2.2 seconds and removal by 6.5 seconds. See `src/bin.*`.
- **The shelf-carry depth transition is corrected.** Original `1000:6f84` builds
  held-object positions from the drag origin (+0x58), not current position (+0xa8).
  Using that saved z after `1008:819a` switches from tray to room size prevents
  food from falling toward y=2224 at z=5000. A food-to-bin drag now captures
  -338 at (555,158,2899) and removes it by 6.5 seconds.

**Next actions, in order:**

1. **Entrance fixed and verified.** Keep `tools/snap.sh --time 20000 --click 560,290,800 --dump`
   as the smoke check. Original visibility setter `1000:7b80`, show/hide `1028:dbf0/dc68`,
   draw gate `1028:cce8`; hidden sprites continue advancing (`1028:ca78`).
2. **Continue the pest audit**, function by function, for the remaining methods and any
   `[inf]` branches. The named cases in the previous handoff were checked and corrected
   against `1008:1246/b5a2/8dca/bc00/e452/5b54/8a3e` and adjacent word routines.
   Original `birth` always returns success and ignores a failed birth-hour lookup; the
   port retains this behavior. PEST.INI (next to the exe) is read: [Files] ADB, [Options]
   Audio and the pest cheats (`docs/re/engine-overview.md`); developer keys are not ported.
   Save format is ours (text, `%APPDATA%\CyberPest\PEST.CFG`), not the original binary.
   **Headless (`--snapshot`) runs save to `%TEMP%\CyberPest-test\PEST.CFG`, never the
   player's `%APPDATA%\CyberPest\PEST.CFG` — never delete or overwrite the latter; the user
   plays the game on this machine.** `tools/snap.sh --fresh` clears the test save (newborn
   pest). WSL's `/mnt/c` view may not show these files; use `cmd.exe` from `/mnt/c`.
3. **All 20 special classes and all five phone callers are ported** (`docs/re/classes.md`).
   Megaphone -189's second control (pest-name row) is done.
   The mouse -91 only appears when the pest's maybeMouse spawns a visit; not yet seen live.
4. Engine gaps: `-157`, `-493` (bug fixed), event type 4, `-112 0` and spawn visibility were
   checked on 2026-10-17 (state-commands.md); input focus/cursor rules match the original
   (engine-runtime.md). Text fields: tail scrolling and a blinking caret; no selection,
   caret movement or clipboard. (Palette animation `1020:b6be` checked: no visible effect with
   this data. Room-object persistence checked: the original save `1008:8f32` stores only
   the pest's owned household ids — no positions — and respawns them on load; the remake
   matches, including the forget points `0962`, `5b54` -14 and `c3b6`.)
5. M5 polish: window scaling/DPI done (per-monitor-v2 aware, 4:3 letterbox, whole-number
   scale when it nearly fills, display-scale initial size, min size 320x240, launch show
   mode honoured); minimize/close (-118/-156) were already wired. Startup option (system
   menu, HKCU Run), first-run data import (DATA.Z or PEST.ADM → %APPDATA%\CyberPest) and
   the player README are done; window/taskbar icon drawn at runtime from PSSTILL.FLC
   (no asset shipped; the .exe file itself keeps the default icon). Release packaging:
   `tools/package.sh` → `work/dist/CyberPest-remake-<describe>.zip` (exe + README.txt).
   The exe is self-contained (system DLLs + UCRT only, static C++ runtime).

**Build/test:** `source ~/tools/env.sh` (toolchain in `~/tools`, no sudo; reinstall with
`tools/setup_toolchain.sh`); configure once with `cmake -S . -B build
-DCMAKE_TOOLCHAIN_FILE=cmake/llvm-mingw.cmake -DCMAKE_BUILD_TYPE=Release`; then
`tools/snap.sh [--time ms] [--click x,y,ms] [--type text,ms] [--drag x1,y1,x2,y2,ms] [--dump]`
builds, runs the exe headlessly on Windows
via WSL interop and writes `work/test/snap.png`. A playable copy for the user lives in
`C:\Users\odasf\CyberPest-Remake\` (copy `build/cyberpest.exe` there to update it).
RE workflow: `tools/redecompile.sh` (applies `docs/re/names.tsv` + FixChkstk, regenerates
`work/ghidra/pest.c`), `tools/gfunc.py`, `tools/normalize_c.py`, `tools/callgraph.py`,
`tools/symbol_xref.py`, `tools/adm_show.py`, `tools/vtable.py`, `tools/class_inventory.py`.
`work/` and `original/` are regenerated by the commands in AGENTS.md.

**Detailed continuation notes (2026-10-16):** The latest verified sequence is
certificate → hatch → pest walking, then phone repeat advice, wall-clock hands,
and bin disposal. `tools/snap.sh` uses the installed original ADM through the
`--adm` argument and writes only to ignored `work/test/`; do not stage that tree.
At the end of each slice run `git diff --check`, the 20-second entrance snapshot,
then a focused snapshot. The verified targeted bin assertions are:
`tools/snap.sh --time 2200 --click 560,290,800 --drag 180,157,555,141,1400 --dump`
(expect -78 state -160 and -85 at z=2899) and the same at `--time 6500`
(expect -85 absent and -78 state -108). A food drag using
`--drag 190,466,555,138,1400` now likewise captures -338 at 2.2 seconds
and removes it by 6.5 seconds. Its earlier erroneous `(555,2224,5000)`
position was due to `GameObject::onMouseMove` taking current z instead of
saved drag-origin z; this has been corrected against `1000:6f84`.

Phone test recipe: accept the certificate with `--type $'Bob\r,500'`; the phone
sprite (132×191) is anchored at (65,430), so Vet = click 65,388, TheraPest = 100,375,
hang up = 35,285 (state -216 hit area); a second call needs a hang-up first. The clock's top-of-hour cuckoo path is code-matched but untested at
a real hour transition. The bin `captured_` list holds object pointers until the
close animation ends; it works for the verified drag, but has not been stress
tested against another removal during that interval. No automated unit-test
suite exists; headless snapshots plus dump inspection are the regression checks.
The smoke test save is `%TEMP%\CyberPest-test\PEST.CFG` (separate from the player's).
After a successful build, update the user copy by copying
`build/cyberpest.exe` to `C:\Users\odasf\CyberPest-Remake\cyberpest.exe`.
The source branch is `main`; push only because the user expressly requested it.
Recheck `git status` on resumption. M4 remaining: the pest audit/INI cheats and the engine gaps in item 4.

## Log

### 2026-10-17 — Doc commits amended into the README/MAKING-OF commit (opencode)
- On review the standalone docs commit looked out of place, so the working-log-privacy
  changes, the fan-project note and the Codex click/drag notes in `docs/re/pest.md` were
  folded into the earlier "Expand the repo README; add docs/MAKING-OF.md …" commit
  (public history is now `b1197c3` → `0e75436`; that commit's original message was kept).
- `main` force-pushed; v1.0.0 deleted and re-created at the new SHA (tags/refs only, no
  content change). The asset zip was **not** rebuilt — the working tree now carries the
  Codex session's uncommitted upscale/surface/attention work and the existing zip (built
  03:05:54, before the first WIP edit at 03:06:05) is the clean fun-note build.
- The uncommitted WIP files were left untouched; nothing of the other session's work was
  staged or committed.

### 2026-10-17 — README "made for fun / no profit" note (opencode)
- Added a note right after the title of `README.md`: fan project, made for fun, no profit, no
  ads, no donations, no affiliation with the original authors. Same sentence added to the
  release notes.
- Committed and pushed; the release asset was rebuilt and the release re-created at the new
  commit so the bundled `README.txt` carries the note too.

### 2026-10-17 — Pest pickup report (Codex)
- Reproduced the non-pickup with `tools/snap.sh --fresh --time 5200 --click
  560,290,800 --drag 145,270,300,245,5000 --dump`; repeated centered/edge drags
  left pest -105 at (160,288). A food -338 drag moved it to (320,320,3096), so the
  synthetic drag path works.
- Temporary targeted input tracing showed the pest had focus and a hit area but
  pickable=0 and held=0. The original base create (`1000:5e1a`) initializes +0xb6
  to zero, module -105 has no -180 setter, and original base OnEvent (`1000:6e5c`)
  requires +0xb6 to start pickup. Pest OnEvent (`1008:5b54`) records user clicks
  and forwards to the base; it does not make the pest pickable. Removed tracing.
- Documented the supported conclusion in `docs/re/pest.md`. No gameplay source
  changes; ground-object oddity remains a separate, unreproduced report.

### 2026-10-17 — CHECKPOINTS.md purged from the repository history (opencode)
- Per the user's decision, `docs/CHECKPOINTS.md` is a **private working log** from now on:
  added to `.gitignore`, untracked, and removed from every commit in the repository history
  (`git filter-branch --index-filter`, `--prune-empty`, tags rewritten), then force-pushed.
- All public references to it were removed or rewritten: `README.md` (two links),
  `docs/MAKING-OF.md` (doc-table row), `AGENTS.md` (start-here and checkpoint-discipline
  rules now say it is gitignored and never committed), `docs/re/pest.md` (the pest_norm.c
  command is now embedded there), `src/archive.h` comment.
- The v1.0.0 tag was rewritten by the rewrite and re-pushed; the release was re-created on
  the new commit with a rebuilt asset (its `README.txt` no longer links the log).

### 2026-10-17 — Repo README expanded; docs/MAKING-OF.md added (opencode)
- New `docs/MAKING-OF.md`: the project's story, from unpacking the InstallShield 3 media and
  decoding `PEST.ADM` to the NE disassembly + Ghidra pipeline (`FixChkstk`, `ApplyNames`),
  the engine's data-driven object model, the function-by-function porting rule (with the
  three corrected mis-ports as examples), headless testing (`snap.sh`, `fuzz.py`) and
  shipping — ending with a map of every detail doc.
- `README.md` now explains the project more (what the remake covers, where to download, repo
  layout, legal note) and links the making-of plus `docs/`, `docs/formats/` and `docs/re/`
  with absolute GitHub URLs, so the copy inside the release zip works too. `AGENTS.md`
  layout line updated.
- The release zip's bundled `README.txt` was regenerated from the new README and the v1.0.0
  asset re-uploaded (the exe is unchanged; only the bundled text differs).

### 2026-10-17 — Release 1.0.0: direct zip/RAR import, Internet Archive first-run fetch (opencode)
- Worked the "Public release plan" handoff (entry below). All four items are done:
  1. **The importer takes the player's file in any form.** New `src/zip.*` reads PKZIP
     archives natively (central directory, stored + deflate entries; the inflate is RFC 1951
     after zlib's puff, Huffman tables fixed/dynamic, 64 MB output cap). New
     `src/gamedata.*` decides by content: RIFF → PEST.ADM as is; PKZIP → its `DATA.Z`
     (or a RIFF `PEST.ADM` entry); `Rar!` → unpack with `%SystemRoot%\System32\tar.exe`
     into `%TEMP%\CyberPest-import` and import the `PEST.ADM`/`DATA.Z` found there; else the
     InstallShield DATA.Z as before. Verified byte-identical to
     `original/extracted/APP/SLOW/PEST.ADM` (SHA-256 d8eb9466…) from: the user's stored
     `CyberPest.zip`, a Python-deflated zip of the same DATA.Z (exercises inflate), and
     `cyberpest.rar` from the Internet Archive (SHA-256 1f3a5dec… as in the handoff).
  2. **First-run Internet Archive option** (`src/archive.*`). When no PEST.ADM is found the
     prompt offers "download a preserved copy from <https://archive.org/details/cyberpest>
     (about 11 MB)" or picking a file; nothing is fetched without that choice. WinHTTP
     download (timeouts, automatic proxy, redirects) to `%APPDATA%\CyberPest\download` with
     a small progress window (percent; Cancel/close stops it), SHA-1 (CryptoAPI) checked
     against `03f02d06…` before use, then tar/import; the temporary copy is deleted. A
     fingerprint mismatch is refused with a message (the item may have changed at the
     source). `--download-ia` runs the same path silently for scripts/tests: end-to-end twice,
     output 1f3a5dec…, download folder left empty. The GUI progress path was run once too.
  3. **README** rewritten for players: "you need the original CyberPest game files (the CD
     or installer)", both first-run options, no unpacking needed.
  4. **Packaging**: `tools/package.sh [version]` → `work/dist/CyberPest-remake-<version>.zip`
     (exe + README.txt). Published as release 1.0.0 from the release commit.
- Smoke test with the Internet Archive edition:
  `tools/snap.sh --fresh --adm <IA ADM> --type $'Bob\r,1500' --click 560,290,2500 --time
  20000 --dump` → pest -105 visible, state -347 (walking), pos (486,265,3041) after the
  certificate and hatch; screenshot correct. Import tests write only under `work/test`; the
  download folder never touches `%APPDATA%\CyberPest\PEST.CFG`.
- Repo renamed from `cyberpest-updated` to `cyberpest-fan-remake`, made public (history
  scan: no game files anywhere, the largest blob ever committed is 134 KB of Win16 API
  names) and the release published:
  <https://github.com/FlavioOdas/cyberpest-fan-remake/releases/tag/v1.0.0>.
  Build note: CMake now links comctl32 + winhttp.

### 2026-10-17 — TheraPest and Vet callers ported exactly (Claude)
- Replaced the partial -207/-209 branches of `Pest::callerText` with the original
  decision trees of `1008:ced0` and `1008:d6a4` (details in `docs/re/classes.md`).
  Added `nthText` for the TheraPest personality/mood table (-271 by list index).
- Earlier the same day (commits 69cbb76, ae23c0c): ported the last eight special
  classes (-92, -531, -534, -550, -172, -591, -91, -94) — see classes.md.
- Verified: certificate typed, then second Vet call printed the -236 diet text and a
  second TheraPest call printed reaction list -255; pest kept walking (-347).
- Megaphone -189: typed words moved to their original row (rect -195, black) and
  the read-only pest-name row (rect -51, dark red) added; snapshot shows "Bob" over
  "hello" beside the Command/teach buttons.
- PEST.INI: `[Files] ADB`, `[Options] Audio` and the pest cheats (stomachs, bowels,
  mood, fleas, rabies, heat, personality overrides) per `1000:28ce`, `1008:8a3e/8dca/
  5b54`. Verified from a Windows folder: Audio=0 starts with -164 AUDIOOFF; GotFleas=1
  + MentalState=-100 makes the Vet give the flea text (-221). Found that WSL cannot see
  `%APPDATA%\CyberPest\PEST.CFG`, so earlier `rm` calls never removed the test save;
  added `tools/snap.sh --fresh`. The earlier Vet/TheraPest checks ran on a saved
  newborn, which is still valid.
- Attention pop-up (`1000:47f6`, class `1010:343c`, module -596) ported in
  `src/attention.cpp`: shown over other apps with the reason picture, ATTEND restores
  the game, IGNORE returns to the previous app. All seven call sites pass their reason.
  Verified headlessly with `--popup -601` / `-221` (renders "CyberPest Emergency",
  feed-me / fleas pictures, ATTEND/IGNORE). Not exercised live on the desktop yet.
- Palette animation (`1020:b6be`) investigated: FLC palettes are either identical to
  PALETTE.BMP or a placeholder that is evidently never applied, so nothing to port.
- Room-object persistence: nothing to add — the original saves only the owned ids
  (`1008:8f32` loop over f_138/f_13c), which the remake already saves and respawns.
- M5 display: per-monitor DPI awareness (`SetProcessDpiAwarenessContext`, `WM_DPICHANGED`),
  letterboxed 4:3 picture (`gameRect`), input clamped to the picture, first window sized
  by whole-number fit or the display scale, `nCmdShow` honoured, minimum size. Build now
  targets Windows 10+ headers. Smoke: real window launched minimised from a Windows
  folder ran without error (killed after a few seconds); headless snapshots unchanged.
  `tools/snap.sh --fresh` fixed (the quoted `del` form failed silently).
- Startup option: system-menu item "Run CyberPest when Windows starts" (HKCU Run value
  `CyberPest`), as the original installer offered; `--startup on|off` for scripts.
  Verified on/off with reg.exe (no entry left behind).
- First-run data import: if no PEST.ADM is found (--adm, [Files] ADB, exe folder,
  %APPDATA%\CyberPest, current folder) the game asks for DATA.Z or PEST.ADM and copies
  the ADM to %APPDATA%\CyberPest. `src/installer.cpp` extracts from DATA.Z natively;
  `--import <file> [--out <path>]` byte-identical to the Python tool.
- README.md for players (data, saves, startup, pop-up, PEST.INI, building).
- Window icon: built at runtime from the pest's standing sprite (PSSTILL.FLC frame 0,
  cropped, nearest-neighbour, alpha from index 0) for ICON_BIG/ICON_SMALL at the window's
  DPI. Checked by rendering the 48 px icon to an image (temporary flag, not committed).
- Release packaging: `tools/package.sh`. End-to-end check in a clean Windows folder:
  unzip → `--import DATA.Z --out <folder>\PEST.ADM` → run without --adm finds it and
  shows the certificate. Not published as a GitHub release (needs the user's go-ahead).
- Engine gaps checked (state-commands.md "Checked 2026-10-17"): -157 hit offset matches;
  **-493 was mis-ported** (target/vector read from the wrong positions, no facing flip) and
  is fixed; event type 4 and `-112 0` never occur with this data; spawn's last argument is
  the visible flag (always 1). Spawn offsets flipping with facing is now [obs] (1038:298e).
- Certificate date/time now formatted exactly like `1000:4b9e/4ed6` from the user's
  `[intl] sShortDate`/`sTime` (verified: `17/10/2026`, `1:21`).
- `moveBy` (`1038:2a10`): an attached object (+0xc0) moves its parent (+0xbc) by the same
  amount, recursively — the remake moved only itself; fixed (callers 6a91/8053/8299 match).
- Random (`1038:5546`) = `rand() % (hi+1-lo) + lo`, MS CRT LCG; the original seeds it with
  `srand(time(NULL))` in InitInstance (`1000:25dc`). The game window now seeds; headless
  snapshots stay unseeded so test runs are reproducible.
- Gravity/config -46 confirmed from `1000:6c92` (comments updated, no code change).
- Activation (`1000:2192/4abc`, `1038:6112/1da0`): idle time only runs while the game
  window is inactive/minimised (was: never reset by the player, so the "bored" pop-up
  could fire during play); returning from another app sends -120 to background -70,
  which turns the light back on after the light switch (-115) minimised the game — the
  remake previously left the room dark. New snapshot option `--activate <ms>`.
- **Save safety (incident):** test cleanups deleted `%APPDATA%\CyberPest\PEST.CFG` while the
  user was playing; their short session's pest was lost (game closed before a minute-tick
  re-save). Fixes: snapshot runs now use `%TEMP%\CyberPest-test` (`World::saveDirectory`),
  `snap.sh --fresh` only clears that, and the game now saves on window close and on
  WM_ENDSESSION, like the original pest destructor (`1038:4a16` → `1008:8f32`). Verified:
  a test run that names a pest leaves `%APPDATA%\CyberPest` untouched.
- Input audit: move/down/up/hover all reset cursor and focus then broadcast, as the remake
  does; the original's one-bubble-pass depth sort per tick is documented, not copied.
  Added the edit controls' blinking caret to the focused text field (certificate name,
  megaphone words).
- Pest coverage: every live 1008 function used by the pest is referenced by the port; the
  unreferenced ones belong to other ported classes, the developers' SetPersonality dialog
  (`e5fc..e87a`), the sound-channel table (`87c2..8998`) or are dead code (`3128`, `6b00`,
  `6c5e` — no callers anywhere).
- **Hang found and fixed** with a new soak test `tools/fuzz.py` (random clicks/drags/typing,
  20 seeds × ~3 simulated minutes, 60 s limit, kills only `--snapshot` processes): the
  original's decide loop (`0962` → `32f8`) spins forever when the hatch is reopened while
  the pest idles on a timer. The remake now wanders in that case, plus a logged safety
  valve. 20/20 seeds pass; details in pest.md.
- Next: in-field text editing (caret keys, paste) if wanted; more fuzzing with the
  phone/megaphone/punishment buttons weighted; user feedback from playing.

### 2026-10-17 — Public release plan (Claude, stopped at usage limit) — CONTINUE HERE
User decision: publish a GitHub release of the **remake only** (never the game data —
copyright; README must say players need the original files). User asked for:
1. **Importer accepts the original `CyberPest.zip` directly** (not only DATA.Z/PEST.ADM).
2. README wording: "You need the original CyberPest game files (CD or installer)".
3. **First-run option to download the game from the Internet Archive** (opt-in, show the
   source URL; the remake itself contains no game data), then publish the release.
Findings:
- IA item `cyberpest` (https://archive.org/details/cyberpest, "Cyber Pest", 1996,
  preserved by the Byte5 YouTube channel) has one game file:
  `https://archive.org/download/cyberpest/cyberpest.rar`, 11,042,069 bytes,
  SHA-1 03f02d0655362a25baf1b9d047aa356a3b380b9b, **RAR4** ("Rar!\x1a\x07\x00").
  Contents (folder `Cyber Pest/`): DATA.Z 11,613,942 bytes, REGISTRO.TXT (Brazilian
  shareware note, $19.95), SETUP files. It is a **different edition** from the user's
  zip: its PEST.ADM is 17,329,962 bytes, SHA-256
  1f3a5deccc84704cc0b4240f2f910cc40aff9fc92afa3489b84c8b8de0fc9df3 (user's SLOW ADM:
  17,330,506 bytes, d8eb9466…a825). `cyberpest.exe --import DATA.Z` extracts it fine and
  a 20 s smoke snapshot with it works (pest enters, room OK).
- The user's `CyberPest.zip` (12,354,921 bytes, SHA-256 616fe570…2ab9) is a plain ZIP
  (DATA.Z at the root plus a bogus "/" entry).
- Windows 11's built-in `tar.exe` (bsdtar/libarchive 3.8.8) lists and extracts the RAR
  (it printed one harmless "Illegal byte sequence" warning for SETUP.BMP; DATA.Z was
  extracted correctly). So: download with URLDownloadToFileW/WinHTTP to
  `%APPDATA%\CyberPest\download`, extract `Cyber Pest/DATA.Z` with
  `%SystemRoot%\System32\tar.exe -xf … "Cyber Pest/DATA.Z"`, then `importGameData`.
  Verify the downloaded RAR's SHA-1 (above) before extracting; show progress.
- For (1) a ZIP reader + inflate (RFC 1951, puff-style) is needed in `src/` (stored and
  deflate entries; match an entry whose name ends in `DATA.Z` or `PEST.ADM`).
- First-run UI idea: TaskDialog with command links "Download from the Internet Archive
  (11 MB)", "Choose a file I have (CyberPest.zip, DATA.Z or PEST.ADM)", "Cancel" — needs
  a comctl32 v6 manifest (.rc via llvm-windres) or fall back to MessageBox Yes/No/Cancel.
- Release: `tools/package.sh`, then `gh release create v1.0.0 work/dist/*.zip`. The repo
  is **private**; the user wants "anyone" to download — confirm explicitly before
  `gh repo edit --visibility public` (check history has no game files first).

### 2026-10-16 20:18 UTC — Usage-guard heartbeat audit (Codex)
- `get_usage_limits` reported 88% used in the primary 300-minute window,
  above the user's 85% stop threshold. No implementation or tests were run.
- `main` was clean at `3dfe9d6` (`Correct drag-origin depth and checkpoint at
  usage guard`), already pushed to `origin/main`. The preceding checkpoints
  contain the exact original addresses, build and snapshot commands, test
  outcomes, incomplete classes and ordered continuation plan. No original or
  generated game data is tracked or staged.
- Updated this handoff with the current threshold and Git state. The watcher
  should be paused after this checkpoint commit to avoid repeated commits
  while the same five-hour window remains above 85%.

### 2026-10-16 — 85% usage guard and drag-origin correction (Codex)
- A bounded comparison of original `1008:819a` and `1000:6f84` identified
  the food-to-bin anomaly's root cause. `1008:819a` resets saved drag origin
  to z=3000 when switching from tray-sized -523 to room-sized -108, and base
  mouse movement constructs its position from that saved depth. The remake
  had instead used its current z=5000. `src/object.cpp` now uses
  `dragOrigin_.z`; the engine note records the original evidence.
- Focused snapshots: `--time 2200 --click 560,290,800 --drag
  190,466,555,138,1400 --dump` shows bin -78 in -160 and food -338 at
  (555,158,2899). The same at `--time 6500` shows the food absent and bin
  back in -108. `git diff --check` passed. This also validates bin capture
  beyond the earlier shelf-bowl example.
- Primary five-hour usage reached 86%, crossing the user's 85% stop line.
  Stopped feature work immediately. This detailed checkpoint, code fix and
  documentation are to be committed and pushed; no new class port is started.
- Resumption order: verify usage budget/restart authorization; inspect git
  status and the last guard commit; run the 20-second hatch snapshot and the
  food-to-bin drag; finish TheraPest/Vet `1008:ced0/d6a4`; then port special
  classes -91/-92/-94/-172/-531/-534/-550/-591 and remaining engine gaps.

### 2026-10-16 — Bin disposal and near-threshold handoff (Codex)
- Read `1008:7514/755c/773a`, module -78, and the runner hit-test helper
  `1000:b0da`. Ported bin capture on hover with -158 area, held flag and -280
  eligibility; captured objects are moved to bin z-1 and released only after
  closing state -160. Added a world object snapshot for safe iteration while
  the state machine may add/remove objects.
- Focused snapshot at 2.2 seconds: bin -78 in -160/OEBIN1.FLC; bowl -85 at
  (555,155,2899). At 6.5 seconds: bowl -85 absent; bin -78 back in -108.
  The screenshot shows the bowl gone and pest moving. `git diff --check` passed.
- Final 20-second entrance snapshot keeps the pest visible at (486,265), bin
  idle, and clock hands showing time-dependent sprites. Redecompilation applied
  528 names; Ghidra again reported one function failure among 2611.
- A first attempt using food -338 revealed a possible separate shelf-carry
  z/depth bug at (555,2224,5000); do not infer bin failure from that run.
- Remaining: eight unported special classes, two partial phone callers, the
  second megaphone control, engine gaps and M5 polish. The current primary
  five-hour usage read 83% after validation; the user's 85% watcher should stop further coding
  and trigger this detailed checkpoint if reached.

### 2026-10-16 — Wall-clock class and unnamed state ordinal (Codex)
- Read original `1008:83ca/844c/84ce/854c/85ea/8682/870c` and ADM modules
  -88/-89/-90/-521. The body sends -519 every clock cycle to itself and both
  hands. On a minute change, hand sprites select by the original rounded
  12-hour/2.5-minute formulas; at a changed hour's minute zero the body spawns
  -521, whose last-frame hook sets a 1..12 strike loop count.
- `src/clock.*` ports the class. The clock's repeated id=0 hand-state maps
  exposed a generic engine gap: state lookup by id collapsed distinct sprite
  frames. `GameObject::enterStateAt` now selects the nth state map while keeping
  ordinary named-state entry unchanged. This mechanism is documented in
  `docs/re/state-commands.md`.
- Built and ran a 4-second snapshot at local 22:10; dump shows H10.FLC and
  M10.FLC for hands -88/-89 and a moving pest. The screenshot shows the clock
  hands aligned with that time. `git diff --check` passed. The hourly cuckoo
  path is code-matched but has not been exercised at a real hour transition.
- Next: complete the two partial callers, port nine remaining special classes,
  and validate the cuckoo at a top-of-hour transition if possible.

### 2026-10-16 — Phone bubble and three caller paths (Codex)
- Ported phone -98 hover forwarding (`1010:3efa/3f42`) and bubble -188 create,
  state-start and last-frame hooks (`1010:4062/45f8/4488`). The existing state
  machine sends caller messages and returns to -108; module -188 supplies its
  text rectangle/font/colors. Added multiline GDI overlay rendering.
- Implemented `1008:ca82` caller dispatch, first-call flags `f_29a..f_2a2`,
  Pesterminator `1008:cd8e`, Pestkeeper `1008:d3e8`, Pestrologer `1008:dd66`.
  TheraPest `1008:ced0` and Vet `1008:d6a4` are explicitly partial [inf]; their
  condition trees and behavior-specific branches remain to port. The save keeps
  greeting flags at 0/1, matching original field semantics.
- Built and visually checked a first Pesterminator greeting at 4 seconds, a
  second-call tip at 7.5 seconds, and a Pestrologer birth-hour greeting at 4.3
  seconds. In each dump the pest moved and the phone avatar/bubble entered
  expected states. The final 20-second entrance dump places the pest visibly
  at (486,265), and the repeated-call screenshot shows wrapped advice. `git diff
  --check` passed. Redecompilation applied 518 names (one pre-existing function
  decompile failure reported by Ghidra).
- Next: finish the two caller algorithms or port clock -90, then ten other
  unported classes and the remaining engine gaps.

### 2026-10-16 — Pest entrance fixed (Codex)
- Reproduced the 20-second entrance failure: pest at (-100,304,3080), frame 0. The handoff's
  pause interpretation was wrong: -322=1 shows the sprite, -322=0 hides it. Original show/hide
  counter and render gate are documented in `docs/re/engine-runtime.md`; animation has a
  separate flag. Renamed the engine's property/cues and removed the false animation gate.
- State entry clears the shown sprite before retiring its runner, avoiding a dangling pointer
  when entering a state with no sprite. No artificial pest wake-up/resume call was added.
- Built and ran the requested snapshot, visually confirmed the pest, and compared 5/10/20-second
  dumps to assert visible, changing positions and arrival inside the room. Scratch evidence is
  in ignored `work/test/entrance-*.txt` and `work/test/snap.png`.
- Next: apply the original-code audit of pest interaction, hunger defaults, learning and events,
  then continue the remaining special classes.

### 2026-10-16 — M0 complete (Claude)
- Unpacked the install media: `tools/is3x.py` (InstallShield 3 + PKWARE DCL explode,
  stdlib only) extracts all 12 files, sizes verified. `tools/bootstrap.sh` recreates
  `original/` from the user's zip.
- Static analysis of PEST.EXE with `tools/ne_dump.py` → `docs/formats/pest-exe.md`.
  REA 6.5.0 + Hopper reject NE ("unsupported NE executable"), so 16-bit work uses
  `objdump -D -b binary -m i8086 -M intel` on raw segments.
- First pass over PEST.ADM → `docs/formats/adm.md`. Installer facts → `docs/formats/installer.md`.
- Architecture decided (C++17 + Win32 + 8-bit DIB sections + waveOut mixer) → `docs/PLAN.md`.

### 2026-10-16 — M1 assets decoded (Claude)
- `tools/admlib.py`, `tools/flc.py`, `tools/png.py`, `tools/adm_dump.py` written (stdlib only).
- `python3 tools/adm_dump.py original/extracted/APP/SLOW/PEST.ADM work/adm` dumps the full
  tree (`tree.json`), manifest, 688 unique virtual files, and every FLC as a PNG frame strip.
  Runs in ~2 s. Visual check: backgrounds and pest animations decode correctly.
- Findings in `docs/formats/adm.md`: sprites are standard FLC; palette index 0 = green key.
- Remaining for M1: ADM tree semantics (ids/refs) and the `ADBB` index.

### 2026-10-16 — M2 tooling (Claude)
- `tools/nelib.py` (NE parser, relocation chains) + `tools/ne_disasm.py`: per-segment Intel
  disassembly in `work/disasm/seg*.asm`, ~1,600 functions, imports named via
  `tools/data/win16_ordinals.json` (generated from Wine .spec files by `tools/gen_win16_names.py`)
  and WING/WAVMIX16 export tables. Key fix: internal far calls use SEGMENT-only fixups; the
  target offset comes from the instruction.
- `tools/setup_toolchain.sh` installed JDK/Ghidra/llvm-mingw/CMake into `~/tools` (no sudo
  needed after all — the "Blocked on user" apt step is no longer required).
- First subsystem map in `docs/re/subsystems.md`.
- Ghidra headless decompile of PEST.EXE started → `work/ghidra/pest.c`.

### 2026-10-16 — Ghidra decompile + engine overview (Claude)
- Ghidra 12.1.4 headless decompiled all 2,217 functions (0 failures) → `work/ghidra/pest.c`.
- `docs/re/engine-overview.md`: ADM = compiled "module" language; engine classes (mDirector,
  mSprite, mScript…); sprite script VM opcodes; full PEST.INI key list incl. `[Cheats]`;
  pest personality = Neuroticism/Extroversion.
- Started M3 C++ skeleton (`CMakeLists.txt`, `cmake/llvm-mingw.cmake`, `src/adm.*`).

### 2026-10-16 — M3 skeleton runs on Windows 11 (Claude)
- `src/`: `adm.*` (ADM tree reader), `flc.*` (FLC decoder, bounds-checked), `surface.*` (8-bit
  surface, transparent blit, BMP palette/writer), `wav.*` + `audio.*` (8-channel waveOut mixer
  replacing WAVMIX16), `main.cpp` (window, integer-scaled StretchDIBits presentation, game loop,
  `--adm/--anim/--snapshot`).
- Cross-compiled with llvm-mingw: single 1.5 MB exe, only system DLL deps (UCRT ships with Win10+).
- Snapshot verified: background, pest sprite (index-0 transparency), foreground panel all correct.
  Pest position is a placeholder until scene data is decoded.

### 2026-10-16 — Better decompile + symbol cross-reference (Claude)
- Ghidra rerun with `tools/ghidra/EnableParamId.java` pre-script → call arguments now present.
- `tools/gfunc.py` (print one function), `tools/symbol_xref.py` (symbol ids in code vs data →
  `work/symbols.tsv`; 398 symbols used by code), `tools/ne_resources.py`.
- Cheats/INI reader = `sub_1_28ce` (app object fields 0x2a0..0x2ca hold `[Cheats]` keys in
  the order listed in docs/re/engine-overview.md). Room setup candidate = `sub_1_5604`.
- Symbol clusters: seg1/seg2 game logic, seg3 UI/text, seg5–7 engine.

### 2026-10-16 — Room built from data (Claude)
- Decoded the object model: module `ref` = class; class factory `1000:5604`; objects are
  state machines (states = sub-modules ref -109, initial -108); state command set documented
  in `docs/re/state-commands.md`; properties X/Y/Z = -178/-179/-110 (`Object_SetProperty`).
- Room module -59: `-71 [object, x, y, z]` placements, `-83 [object, prop, value]`. Cursor table
  module -58 (FLC + hotspot). Certificate layout module -50. ADBB index = (id, class, 0, offset).
- New tools: `tools/vtable.py`, `tools/adm_show.py` (module tree with symbol names),
  `tools/redecompile.sh` + `tools/ghidra/ApplyNames.java` (names from docs/re/names.tsv),
  `tools/snap.sh`.
- `src/scene.*`, `src/symbols.h`: the room renders from data (all furniture/buttons correct).

### 2026-10-16 — Interactive runtime + startup flow (Claude)
- `src/world.*` replaces the static scene: full generic state-machine runtime (see
  docs/re/state-commands.md "Runtime"). Game cursors from module -58. `--time/--click/--dump`.
- Found: hatch (-76) covers the room at start; certificate (-106, class -593) sends -124 to open
  it; hit areas are relative to the sprite top-left (from `Runner_DebugDrawHitAreas` asm).
- `tools/class_inventory.py` → `docs/re/classes.md` (20 special classes, overridden slots);
  `tools/msgmaps.py` (no MFC message maps exist); ne_disasm now labels vtable targets.

### 2026-10-16 — Shelf items + preview (Claude)
- Initial state = first child module with ref -109 (others are class data, e.g. -275 shelf
  rects). Shelf ornaments and the mouse now behave.
- Enter key → message -203 to the certificate (stand-in until class -593 is ported).
- Preview copied to `C:\Users\odasf\CyberPest-Remake\` (cyberpest.exe + the user's PEST.ADM).

### 2026-10-17 — Engine rebuilt around the original object model (Claude)
- RE: hook table (+0x14..+0x68), input model (event types 1 down / 2 up / 3 hover; broadcast to
  all objects front-to-back, first hit takes focus), drag/drop (-182 held, -279 falls, -309 falling,
  landing -449/-450), movement (sprite motion -326, end move -455, offset -548, ranged move -493),
  walking (1000:7460). See docs/re/engine-runtime.md.
- `src/vec.h`, `src/object.*`, `src/world.*` (rewritten), `src/classes.*` (factory + Certificate
  position). main.cpp: real button down/up + capture, app messages (-156 close, -118 minimize).
- Pest code extraction: `python3 tools/callgraph.py --seg 2 sub_2_0000 sub_2_56e8 sub_2_571c
  sub_2_57d6 sub_2_583c sub_2_69b2 sub_2_5b54 | awk '{print $1}' | xargs python3 tools/gfunc.py |
  python3 tools/normalize_c.py > work/pest_norm.c` (96 functions, ~5.6k lines).

### 2026-10-17 — Pest ported (Claude)
- Ghidra fix: `tools/ghidra/FixChkstk.java` (MSC `__chkstk` allocates the locals; without it every
  comparison decompiled as garbage). Part of `tools/redecompile.sh`.
- Read all 96 pest functions (mapped in `docs/re/pest.md`): decision modes, activities, object
  interaction (reaching for held items, wrestling), walking with turn states, personality tables
  (nearest row in Neuroticism/Extroversion space, mood-shifted), ordered preference lists per
  birth hour with reward/punishment, megaphone word learning, needs (stomachs, bowels, mood,
  fleas, rabies, heat), per-second/minute life tick, phone callers, save/load (PEST.CFG).
- Engine additions: facing image variants (image lists hold [normal, facing] FLCs — no code
  mirroring), exact hit-test transform (`1000:acc0`), `-157` hit offset, duplicate spawns,
  walking (`setWalkTarget`, `walkStep` = `1000:7460`), `moduleInt`, `childFirstInt`,
  `heightAboveFloor`, `World::count`, attention hook, `GameObject::text()`.
- `src/pest.*` written and wired; builds; smoke test: pest decides but stays paused off-screen.
- Data senders of messages to the pest (module: state → message): hatch -76/-124 → -153;
  -96 (electric button) → -168/-169; flap -74 → -175 (timed); megaphone -80 → -185/-187;
  text panel -189 → -201/-202; shelf food -81/-84..-87/-311..-313 → -38, -36, -283, -305;
  mouse -91 → -283/-305/-222 (rabies); toys -338..-345 → -35; -166 → -28/-37; -550 → -26;
  -129 → -29/-508/-510/-512; -566..-572 → -15/-38; pills -102 → -15/-494; potion -103 → -15/-502;
  powder -104 → -448. None sends -1 (wake-up) or -8 (resume).

### 2026-10-16 — Pest audit corrections (Codex)
- Read original `1008:1246`, `8a3e`, `8d28`, `8dca`, `b5a2`, `b7aa`, `bc00`, `e452`, `5b54`
  through Ghidra and the targeted assembly where it clarified arithmetic. Completed
  interaction steps 10–12, corrected newborn GoodStomach=50, taught-word validation,
  reward shift ×2, one-shot AdvanceTime, event -187/-169/-293/-26, and birth row checks.
- `birth` remains `void` in the port: original `1008:8d28` returns 1 regardless of the
  `setBirthHour` result, and `1008:8a3e` does not gate activation on that result.
- Built and ran the 20-second hatch snapshot after these changes; the pest is visible in
  the room. The original save is binary; the remake's text save is intentionally distinct.
- Next: continue the remaining pest audit and port special classes, especially shelf
  items, certificate, text panels and phone. No original assets are committed.

### 2026-10-16 — Shelf item class family (Codex)
- Ported the common item creation, -308 randomized timer and expiry, -297 height gate,
  -184 shelf snap, -182 pickup, -41 removal notice, and landing notifications from
  `1008:6ee6/70d4/735e/73d4/746e/74d0`. Rectangles and limits come from module
  -275/-276/-277, not hard-coded room coordinates.
- Ported the -337/-587 home/large-small variant (`1008:7f16/805a/819a`) and -129
  wrapper cleanup (`1010:2c1c`). Added a distinct +0x58 drag origin after confirming
  `1000:63ee` stores it on pickup; it is not the actual +0xa8 position. The snapshot
  tool now supports repeatable `--drag x1,y1,x2,y2,ms` over 400 ms.
- Built successfully. The 20-second hatch snapshot retained a visible pest in-room;
  `tools/snap.sh --time 3500 --click 560,290,800 --drag 190,466,320,300,1400 --dump`
  placed food -338 in room state -108 at (320,320,3096); the image shows the small food
  sprite in the room. Evidence is in ignored `work/test/items-*.txt` and `snap.png`.
- Next: certificate and text panels, then the phone and remaining class hooks. The
  broader pest audit and INI cheats are still incomplete.

### 2026-10-16 — Certificate name and megaphone text (Codex)
- Ported certificate -593 create/state/last-frame/event hooks from `1010:47c6/513a/
  502a/50e8`: module-derived name/date/time field placement and style, newborn entry,
  existing-name bypass, name transfer to pest (`1038:6f06`) with save, and hidden fields
  on close. Replaced the old global Enter shortcut with focused `WM_CHAR` handling.
- Added `src/text.*` and indexed GDI overlay rendering (`src/surface.*`) so the same
  typed text appears in snapshots and the live window. Snapshot `--type text,ms` enables
  repeatable keyboard tests. Compact date formatting/auto-fit and reduced edit-control
  features are marked [inf] in `docs/re/classes.md`.
- Ported the primary typed-word path of megaphone panel -189 from `1010:0f6e/17f4/
  188c/18de`: the pest can now read its `text()` for -201/-202. Its second original
  child control remains to inspect.
- Built and visually checked certificate snapshot with typed "Alice" plus full date/time,
  then a 20-second flow with certificate acceptance, megaphone typing "hello", command
  click, panel removal and the pest moving inside the room. Confirmed the save contained
  `name Alice`; deleted that test save afterward so the next run starts newborn.
- Next: phone bubble -188 and callers -206..-210; then other special classes and engine
  gaps. No original files or generated assets are staged.

### 2026-10-17 — Optional deterministic HD graphics (Codex)
- Added a 4× runtime scaler for the composited indexed game framebuffer: two Scale2x
  passes and a light 5-tap blend only at color boundaries. This covers all visible ADM
  sprites, background, UI text and software cursor without shipping or rewriting assets.
  It is a modern display option, not an inferred original routine or newly painted art.
- Applied the same scaler to the separate emergency popup after composing its RLE BMPs
  offscreen. HD popup defaults to 2× displayed size (capped to fit the work area), with
  pointer coordinates remapped to the original ADM button rectangles. Classic popup
  placement/size remain unchanged.
- Live mode defaults HD and uses a larger screen-fitting window, with an Alt+Space system
  menu switch and `--classic` override. Headless snapshots stay classic by default;
  `--hd` writes a 32-bit BMP. `tools/snap.sh` converts 8/24/32-bit BMPs to PNG using
  standard-library Python. Live HD presentation is capped at about 30 FPS.
- Built with the repo's Release llvm-mingw configuration. Four headless checks passed:
  20-second hatch room HD 2560×1920, room classic 640×480, hungry emergency popup HD
  1328×1360, popup classic 332×340. Room dump showed pest -105 walking at
  (486,265,3041); screenshots were inspected. `git diff --check` passed. Isolated native
  Linux scaler benchmark was 15.12 ms/frame on synthetic 640×480 input. No physical
  desktop interaction test yet; specifically verify the live menu and popup hit boxes.
- Updated player README. No extracted sprites, originals, screenshots or binary benchmark
  files belong in Git; user-facing previews live in the task's local outputs directory.

### 2026-10-17 — Geometric megaphone proof (Codex)
- The stronger screen-space polished filter was visually rejected: it softened pixel
  edges but preserved the megaphone's crooked hand-drawn contour. An automated contour
  trace with quadratic Bézier rounding was still bowed because it followed that contour.
- Constructed a deterministic, manually fitted SVG from the original `MEGA.FLC` frame:
  straight cone sides, round bell/head/handle, original black/blue palette, and no loose
  pixels. Rendered a 960-pixel preview and visually checked it beside 8× source pixels.
  The user-facing comparison is `megaphone-geometric-correction.png` in the task outputs.
- This is an ignored proof in `work/test/`, not game-integrated or a general remaster.
  The C++ polished-mode edits remain uncommitted WIP; do not ship them as complete.

### 2026-10-17 — Automatic polished-mode trial rejected and reverted (Codex)
- After the user approved the hand-fitted megaphone proof, implemented a GDI+ contour
  fitter first on the composed room, then on cached individual sprites at 4× resolution.
  Both built and produced 20-second snapshots. The whole-room pass heavily altered
  backgrounds; the per-sprite pass preserved more but caused missing details, edge
  artifacts, and inconsistent colors. The user saw the preview and rejected the result.
- Reverted every tracked third-mode change and deleted its new source files using
  `apply_patch`. `git status --short` and `git diff --check` are clean. The existing
  two-mode Release build succeeds. No commit, push, or playable-copy replacement was
  made. Ignored experiment files remain in `work/test/` for evidence only.
- Conclusion: the approved standard is hand-fitted art, not a uniform contour filter.
  Such work must be handled asset-by-asset and cannot be claimed complete by an
  automatic pass; clarify how derived vector geometry may be stored under AGENTS.md.

### 2026-10-17 — First embedded experimental art-mod asset (Codex)
- User authorized hand-authored derivative art in the private repo for an explicitly
  unofficial, for-fun experimental mod. Updated `AGENTS.md` and player README to
  distinguish this from the faithful remake and continue excluding original game files.
- Added `mods/experimental/megaphone.svg` and its transparent 480×480 PNG render.
  CMake embeds the PNG as a Win32 RCDATA resource; `src/mod.cpp` decodes it using
  Windows GDI+ and draws it over the Smooth HD megaphone only. No player sidecar or
  runtime software is required beyond the player's original ADM.
- `Surface` records the underlay just before idle `MEGA.FLC` and subsequent blits.
  The mod replaces only that sprite's output rectangle, then replays later overlapping
  sprites and cursor so depth remains correct. The rest of the composed HD frame is
  untouched. System menu and CLI offer the mode; default remains Smooth HD.
- Built Release and ran fresh 20-second room snapshots for HD and mod. Both output
  2560×1920; pixel comparison found 16,990 changed pixels inside the megaphone
  rectangle and 0 outside. Classic remained 640×480; mod popup remained 1328×1360.
  Screenshots visually inspected. `git diff --check` passed. The physical system-menu
  toggle and overlap edge cases still need live verification. Committed as `c7dbe3a`;
  copied the verified exe to the user's playable folder and matched SHA-256.

### 2026-10-17 — Usage-guard stop during fish-bowl review (Codex)
- At the start of the requested fish-bowl work the primary 5-hour Codex window was
  84% used. Inspected the existing `OOBOWL.FLC` contact sheet and decoded frames;
  `tools/flc.py` reported 46×49, 9 frames, 85 ms/frame, with a tenth ring frame
  matching frame 0. Across the nine frames, 1,596/2,254 positions are invariant;
  all 658 changing positions are within x=5..40/y=8..39. This supports a
  shared hand-fitted shell while preserving the animated fish but is not itself
  a completed asset.
- Rechecked account usage: primary reached 85%. Stopped development without
  editing source, rendering replacement art, running tests, or replacing the exe.
  `git status --short --branch` showed clean `main` ahead of origin by two
  previously validated commits (`83a3bb6`, `c7dbe3a`). `git diff --check` was
  clean. No uncommitted source changes existed before this handoff edit.
- Added the detailed current-state handoff above: behavior, prior validation,
  original-address evidence, blockers, exact build/snapshot commands and ordered
  work. `original/` and `work/` remain ignored; neither is committed. This
  checkpoint alone is force-staged because the user specifically requested a
  committed checkpoint at the usage threshold, overriding the normal private-log
  rule. The user-requested push result is reported separately in the final reply.

### 2026-10-17 — Optional 60 FPS presentation target (Codex)
- User explicitly resumed work after the 85% stop and requested an optional
  60 FPS mode before the fish-bowl asset, with a 93% usage stop if needed.
- Added an independent live-display option via `--60fps` and a checked system-menu
  command. The renderer alternates 17/17/16 ms frame intervals; the wait deadline
  adapts instead of always sleeping 10 ms. Opt-in `timeBeginPeriod(1)` is paired
  with `timeEndPeriod(1)` when turned off or on process exit. The default HD
  cadence (~30 FPS), Classic behavior, game-time tick and ADM animation delays
  are otherwise unchanged. This is presentation only, not interpolated motion.
- Cross-compiled the Release executable successfully. `git diff --check` passed.
  Two fresh 20-second mod snapshots, one with `--60fps` and one without, were
  byte-identical at 2560×1920; dump showed the walking pest -105 at
  (486,265,3041). This checks CLI parsing and unchanged game/snapshot behavior,
  not physical 60 FPS throughput. A real desktop FPS measurement and menu-click
  test remain for follow-up. The fish-bowl art and mod-region generalization
  remain untouched.

### 2026-10-17 — True 60 FPS animation request, handoff only (Codex)
- The user corrected the target: 60 Hz redraws alone are insufficient; the
  experimental mode should eventually include more actual animation poses and
  smooth movement. The existing `b3d7171` option is explicitly only 60 FPS
  presentation. No new code, rendered asset, or validation run was made here.
- Inspected `src/main.cpp:534`, `src/object.cpp:427`, and `src/world.cpp:175`:
  world logic advances FLC frames and cues at original delays; drawing selects
  only the current integer frame. The proposed seam is render-only interpolation
  plus individually reviewed supplementary art, not a faster simulation tick.
  The next agent must first measure live throughput, then prototype one walking
  pest cycle and verify behavior/visual criteria in the new current-state note.
