# Object state machines (data-driven behaviour)

Established from `sub_1_8d10` (= Ghidra `1000:8d10`, named `State_Run`) and the ADM data.
Status markers: **[obs]** observed in code/data, **[inf]** inferred, **[?]** unknown.

## Structure [obs]

- A room object is a module whose `ref` is its class (e.g. `-107` CLASS_OBJECT). Its child
  modules with `ref = -109` are **states**; `-108` is the initial state of every object.
- A state is a sequence of lists; each list's `ref` is a **command symbol** and its values are
  the arguments. `State_Run(object, stateModule)` executes the lists in order.
- Most states are selected by a negative symbol id. Clock hand modules -88/-89
  instead have repeated id=0 state maps after the first -520 map; their original
  selector adds a numeric offset to the first state's ordinal (`1008:854c/85ea`).
  The remake's `enterStateAt` preserves these distinct unnamed states. [obs]
- Object fields used: `+0x2e/+0x32/+0x36` three sprite slots, `+0x3a` current sprite,
  `+0x46` next state, `+0x0a` timer/duration object.

## Commands

Value access: `1038:062e` get int (value idx, list idx); `1038:1bfa` get long (used for
times, scaled by `DAT_1050_2db2` via `1030:407a` [inf: time → ticks]); `1038:1c2e` get file/module
ref; `1000:a21e` get optional value with default; `1018:8dd6` list length.

| Symbol | Args | Action |
|---|---|---|
| -2 | sym, value | set property now: object `SetProperty` (`1000:63ee`) if no sprite yet, else sprite property (`1000:83da`) [obs] |
| -3 | time, sym, value | timed set property (`1000:83da`) [obs] |
| -4 | x | object vtbl+0x28 if no sprite, else `1000:8846` [obs]; meaning [?] |
| -5 | time, x | timed `1000:8846` [obs] |
| -6 | a, b, c, d, e | object vtbl+0x24 / `1000:8796` with a rect-like value (`1038:0662`) [obs]; meaning [?] |
| -7 | time, a..e | timed `1000:8796` [obs] |
| -8 | x | object vtbl+0x2c / `1000:88d4` [obs] |
| -9 | time, x | timed `1000:88d4` [obs] |
| -10 | x | object vtbl+0x30 / `1000:8962` [obs] |
| -11 | time, x | timed `1000:8962` [obs] |
| -12 | hitarea, state, a, b | register event type 1 (`1000:a07e(...,1)`) [obs]; data: door "on click hit area -123 → state -124" [inf: click] |
| -13 | 4 ints | register event type 2 [obs] |
| -170 | 4 ints | register event type 3 [obs] |
| -121 | a, state, flag | register event type 0 (`1000:a07e(...,0)`) [obs] |
| -111 | FLC file | create main sprite (new 0xda, `1038:5570`) into slot `+0x32`, make current [obs] |
| -306 | FLC file | create sprite into slot `+0x2e`, make current [obs] |
| -431 | FLC file | create sprite into slot `+0x36`, make current [obs] |
| -112 | n | `1038:56e4(sprite, n)` [obs]; data usually 1 [inf: play mode/loop] |
| -183 | n | `1038:56ae(sprite, 1, n)` [obs] |
| -114 | id, x1, y1, x2, y2, [cursor1], [cursor2] | define hit area (`1000:9fbe`) [obs] |
| -116 | WAV | attach sound to current sprite (`1000:89f0`, `1018:c5d6`) [obs] |
| -119 | a, b | object vtbl+0x1c / `1000:8472` [obs] |
| -176 | time, a, b | timed `1000:8472` [obs] |
| -132 | state | set next state (`+0x46`) [obs] — "when finished go to" [inf] |
| -151 | state... | pick a random next state (`1038:5546(0, n-1)`) [obs] |
| -157 | a, b | set timer object at `+0x0a` (`1038:1ef0(1, a)`) [obs] |
| -326 | a, b, [c] | `1000:832c(sprite, …)` [obs] |
| -455 | a, b, [c] | `1038:5734(sprite, …)` [obs] |
| -548 | a, b, [c] | `1038:5706(sprite, …)` [obs] |
| -493 | time, time, a, b, c, [d] | `1000:850a(sprite, …)` [obs] |

## Example (door, module -72) [obs data]

State `-108` (closed): image `ADOOR.FLC`, hit area `-123` at (87,118)-(108,144) with cursors
`-61/-62`, `-12 [-123, -124, 0, 1]` → clicking goes to state `-124`, which plays `ADOOROP.FLC`
+ `ADOOROP.WAV`, has a `-12` to `-131`; `-131` plays `ADOORSHT.FLC/.WAV` then `-132 [-108]`
back to closed.

## Runtime (how states execute) [obs unless marked]

Classes: object (generic `-107`, vtable `1038:40de`); **runner** (one per entered state,
0x48 bytes, ctor `1038:2c28`, vtable `1038:3832`); **sprite** (0xda bytes, engine mSprite
subclass, vtable `1038:5054`).

- `EnterState(obj, state)` (`1000:6100`): retire current runner (stop its sprite, vtbl+0x38),
  `obj+0x52 = state`, new runner, `State_Run`, then refresh the hover cursor.
- `Object_Activate` (`1000:5f02`, vtbl+0x18): apply top-level `-2` property lists until the
  first state sub-module, then `EnterState(first state)`.
- **Animation sequence per state:** up to three sprites: intro (`-306`, runner `+0x2e`),
  main (`-111`, `+0x32`), outro (`-431`, `+0x36`); played in that order. Commands after a
  sprite-creating command attach to that sprite (cues, sound, loop flags).
- **Sprite per-frame step** (`1000:7df8`): start/flush its sound; fire cues whose frame equals
  the current frame; move the object along its motion vector (interpolated over the frames);
  call runner vtbl+0x20 → object vtbl+0x4c (per-frame hook). On the last frame:
  runner vtbl+0x1c; if looping is off (`+0x94 == 0`) or the loop count (`+0x98`) is reached →
  fire cues at frame `count` and at `-1` ("end"), apply final displacement, runner vtbl+0x14
  (finished); else restart (runner vtbl+0x18). `-112 n` (sprite `+0x96`): when 0 the last
  frame is not redrawn [inf].
- **Cue time scaling:** cue times in the data are in high-spec frames, divided (`1030:407a` =
  CRT long divide) by config `-49` (`theApp+0x6d6`): 1 for SLOW/high spec, 2 for FAST/low spec.
- **Runner finished** (`1000:a4f2`): object vtbl+0x48; pick next sprite (intro→main→outro);
  if none: runner done (`+0x42`), object vtbl+0x38, and if still current and `+0x46` (next
  state) set → `EnterState(next)`.
- **Events** (`State_AddEvent`): `{type, value, state, c, d}`. `-121 [value, state, c]` →
  type 0 (message), d=1. `-12 [hitarea, state, c, d]` → type 1 (click). `-13` → type 2,
  `-170` → type 3 (meaning [?]: other mouse actions/drops; type 3 value `-133` special-cased).
  Matching (`1000:b3b4`): first event with equal type and value → `1000:b44c`: if `d == 0`,
  only when the object has focus; if `c == 0` and the runner is still playing → set
  next state, turn looping off, let the animation finish; else `EnterState` immediately.
- **Messages:** `-119 [target, msg]` → target vtbl+0x64 → runner vtbl+0x3c → object
  vtbl+0x68 `OnEvent(msg, type 0)`. Target `-117` = the app/director (`1000:4768`).
  Object lookup (`1000:6982`): `-204` self, `-485` parent object, else room object by id.
- `-4 [obj]` remove: send `-41` to it, then remove from the room. `-6 [obj, x, y, z, ?]` spawn
  (`1000:6aa0` → `CreateObjectForModule` at a position). `-8 [obj]` hide (`+0xb2 = 0`),
  `-10 [obj]` show (`+0xb2 = 1`, property `-322`). Hidden objects continue animating and firing
  cues; see the visibility evidence in `engine-runtime.md`.
- **Hit areas** are relative to the sprite's top-left; image anchor = bottom-centre at (x, y)
  (confirmed by the `ShowHitAreas` debug drawer `1000:a6e2`; door handle lines up).
