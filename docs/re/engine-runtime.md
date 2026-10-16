# Engine runtime: object hooks, input, movement (M2/M4)

Complements `state-commands.md`. Offsets are into the original C++ objects. [obs] unless marked.

## Object vtable (generic object -107 = `1038:40de`)

| Slot | Base impl | Meaning |
|---|---|---|
| +0x04 | dtor | |
| +0x14 | `1000:5e1a` | create(world, module id, …) |
| +0x18 | `1000:5f02` | activate: top-level `-2` lists, then first state (first child with ref -109) |
| +0x1c | `1000:69e0` | cue/cmd send message `[target, msg]` (target -117 → app `1000:4768`) |
| +0x20 | `1000:6a52` | cue move target by vector (`1038:2a10` MoveBy, recursive over attached children +0xbc) |
| +0x24 | `1000:6aa0` | spawn object (`-6/-7`) |
| +0x28 | `1000:6b9a` | remove object (`-4/-5`): send -41, World remove |
| +0x2c | `1000:6c02` | hide (`-8/-9`) +0xb2 = 0 |
| +0x30 | `1000:6c4a` | show (`-10/-11`) +0xb2 = 1 |
| +0x34 | stub | **state started** (called at the end of State_Run setup `1000:a27c`) |
| +0x38 | stub | **state ended** (runner done) |
| +0x3c | stub | **sprite started** (sprite) |
| +0x40 | stub | ? |
| +0x44 | stub | ? (pest overrides: walking/target checks) |
| +0x48 | stub | sprite finished (from Runner_OnSpriteFinished) |
| +0x4c | `1000:6c92` | per-frame: falling under gravity (+0x6a / prop -309), then sync sprite position/visibility |
| +0x50 | `1000:718c` | hover tick (point) → runner +0x2c → event type 3 |
| +0x54 | `1000:6f84` | mouse move (point): if held (+0x54, prop -182) follow pointer with floor clamp/drop; → runner +0x2c (type 3) |
| +0x58 | `1000:71fa` | button down (point) → runner +0x30 → event type 1 |
| +0x5c | `1000:7268` | button up (point): if held → release (fall if +0xb8/prop -279), notify; → runner +0x34 → type 2 |
| +0x60 | `1000:7324` | cancel (drop held object, send -184) → runner +0x38 |
| +0x64 | `1000:73b0` | send message (msg, from) → runner +0x3c → object +0x68(msg, type 0) |
| +0x68 | `1000:6e5c` | OnEvent(from, value, type) → runner event match (`1000:b3b4`) |

Object fields: +0x4e runner, +0x52 state id, +0x54 held (-182), +0x56 no-floor-clamp (-181),
+0x6a falling (-309), +0x76 walking, +0x78 walk target, +0x82 walk velocity, +0x8c property
list, +0xa8 position {x,y,z}, +0xb2 visible (-322), +0xb4 facing/flipped (-323), +0xb6 (-180),
+0xb8 falls when released (-279), +0xba (-122), +0xbc parent object, +0xc0 attached flag.

## Input (room/"screen" object, segment 1)

Objects are kept sorted by depth (front first). For every mouse action the room resets the
cursor to `-59` (default), clears focus, and calls the hook on **every** object:
`1000:07c6` per tick → +0x50 (if the pointer is inside the window); `1000:103c` mouse move →
+0x54; `1000:1206` button down → +0x58; `1000:135c` button up → +0x5c.
The runner's handler (`1000:acc0`) converts the pointer to sprite-top-left coordinates and
tests its hit areas: on the first hit, if nobody has focus this object takes it; the focused
object sets the cursor (hit area cursor, or the pressed cursor while the button is down); then
`OnEvent(hit id or 0, type)` is called. Event type: 1 = button down, 2 = button up, 3 = hover,
4 = runner +0x28 (caller unknown). Events with d = 0 only fire for the focused object.

## Movement

- Depth from floor position: `z = y + 3000 - floorMinY` (`1000:77b6`), floorMinY = config -45 (224),
  floorMaxY = config -44 (359).
- Sprite motion fields: +0x9c draw offset (cmd -548), +0xa6 displacement spread over the
  animation (cmd -326; per frame the object is moved so the applied total = motion·(f+1)/n),
  +0xb0 displacement applied when the animation ends (cmd -455; x flipped when facing +0xb4),
  `-493 [t0, t1, dx, dy, dz, …]` adds per-frame move cues between frames t0..t1.
- Walking (`1000:7460`): with a walk target (+0x76/+0x78) and the main sprite playing, scale the
  sprite's motion vector so the object reaches the target in a whole number of cycles and face it.
- Falling (`1000:6c92`): while +0x6a, add gravity (config -47 × -49 per frame) and land at the
  floor depth (sends a landing message).
- Cue types (`1000:7be4`): 0 send (+0x1c), 1 move (+0x20), 2 spawn (+0x24), 3 set property,
  4 remove (+0x28), 5 hide (+0x2c), 6 show (+0x30).

## Visibility (corrected 2026-10-16)

`-322` is **visible**, not paused. `1000:7b80` maps nonzero to sprite vtable +0x34
(`1028:dbf0`, increments the negative hide counter +0x58 towards zero and clears bit 0 of
+0x66), and zero to +0x38 (`1028:dc68`, decrements the hide counter and sets bit 0).
The drawing function `1028:cce8` skips rendering when bit 0 is set. Animation advancement
(`1028:ca78`) tests bit 1 instead, so hiding does not stop animation or event cues.
Room placements default to visible=1 (`1000:055b`); a sixth placement value overrides it.
The pest's top-level `[-322, 1]` therefore keeps it visible. The hatch sends `-153`, which
sets mode 2 and enters -324; the normal walking logic brings it in from x=-100. No extra
wake-up or show message is needed. `1008:6736` handles later re-entry after removal (-1).

## Text controls (remake layer, 2026-10-16)

Original certificate and panel classes create Win16 child edit controls through
`1010:1a1c`, show them at the animation's last frame, and use `1038:6498` to read
their text. The remake's `TextField` reads per-module rectangles, font and RGB lists;
`Surface::drawText` renders with native GDI into an indexed bitmap so snapshots and
the live window use the same pixels. `World::inputChar` forwards `WM_CHAR` to the
active object. The visible output and typed-word path are verified, but caret,
selection, clipboard and original control styles are not yet replicated. [obs:
original control calls and module data; inf: equivalent overlay implementation]
