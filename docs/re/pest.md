# The pest (class -105, module -105, vtable `1038:3f9a`)

Source: the 96 functions reachable from the pest's vtable overrides (segment 2, `1008:*`).
Regenerate the readable listing with the command in CHECKPOINTS (→ `work/pest_norm.c`).
Port target: `src/pest.*`.

## Hooks overridden

| Slot | Function | Role |
|---|---|---|
| +0x14 create | `1008:0000` | base create, read tuning lists → fields, home point, then `1008:8a3e` init |
| +0x34 state start | `1008:56e8` | if state == -324 (decide) → `1008:0962` choose next behaviour |
| +0x38 state end | `1008:571c` | finish walk-to-state (`f_188`): enter `f_186` or -324 |
| +0x3c sprite start | `1008:57d6` | walking: `1000:7460` scale walk motion to the target |
| +0x44 last frame | `1008:583c` | walking arrival checks, follow moving target, timers |
| +0x4c per frame | `1008:69b2` | `1008:e452` per-second life tick, then base per-frame |
| +0x68 event | `1008:5b54` | messages from buttons/food/treatments/megaphone, clicks |

## Tuning (create, from module -105 lists)

`f_c2`=-381 50 (direct-walk % threshold, `6d58`); `f_c4/c6`=-382 30,60; `f_c8/ca`=-383 2,10;
`f_cc/ce`=-384 20,120; `f_d0/d2`=-385 1,60; `f_d4/d6`=-386 20,120 (activity durations, s);
`f_d8/da`=-388 1,5; `f_dc/de`=-389 1,3 (loop counts); `f_e0/e4/e2`=-387 -36,320,676 (x: off
left, centre, off right); `f_e8/e6`=-390 -50,200; `f_ea/ec`=-391 440,690; `f_ee/f0`=-392
100,540 (floor x range); `f_f2/f4`=-393 10,630 (screen x bounds); `f_f6`=-394 -200; `f_f8`=-395
840; `f_fa`..`f_118`=-396..-411 (3,3,3,5,10,50,25,80,50,20,90,10,500,-400,1040,-1000); home
point `f_11a`=-412 (130,224); `f_124`=-413 50 (word strength), `f_126`=-414 60 (teaching window
s), `f_128/12a/12c`=-415 5,10,20, `f_12e/130`=-416 25,75 (stomach bands), `f_132`=-417 10,
`f_134`=-418 50 (junk limit), `f_136`=-419 90, `f_13e`=-420 40 (mood change per snack/shock).

## State fields

- Mode `f_140`: 0 decide, 1 interact with target object (`f_146` step 1..10), 2 activity
  `f_142` (1..16), 3 wander, 4 reaction (`a808`), 5 idle (`a6b8`). `f_144` forced activity.
- Activity timer: `f_15a` running, `f_15c` start (ms), `f_160` duration (ms), `f_164` expired
  (`1008:30b6` start, `1008:3190` check). `f_172` interrupted, `f_178` clicked by the user.
- Walk-to-state: `f_186` state to enter on arrival, `f_188` walking, `f_18a` arrived early,
  `f_18c/18e` pending second leg (`1008:5136`).
- Target: `f_19a` object, `f_19e` reaction state, `f_1a0` interaction distance (first int of the
  reaction state module), `f_1a2` walk state, `f_1a4` target position snapshot.
- Megaphone: word maps `f_1ae` (word → object id) and `f_1c8` (word → behaviour state), strength
  0..100; `f_1e6/1ea` commanded object, `f_1e8/1ec` commanded behaviour; teaching windows
  `f_200..f_204` (object) and `f_206..f_20a` (behaviour) last `f_126` s.
- Identity: `f_218` name, `f_220` birth time, `f_224` birth hour (1–12), `f_226` age (s),
  `f_22a` birth personality, `f_232` personality (Neuroticism, Extroversion; 0..10000),
  `f_23a` point of the last chosen behaviour.
- Behaviour tables (`1008:9fd0`, rows = state + personality point ×1000): `f_242` -346 (idle,
  `a6b8`), `f_248` -353 (walk style, `a530`), `f_24e` -362 (reaction, `a808`), `f_254` -370
  (`a9aa`), `f_25a` -375 (`ab4c`). Choice (`1008:a3ae`): sample n random rows, keep the one
  nearest the mood-adjusted personality (`1008:a2b6`: x − mood·100, y + mood·100).
- Preference lists (`1010:2574/2804/26a8/274c`): `f_260` activities (-377 per birth hour),
  `f_274` reactions (-379); pick = walk the ordered list, accept each entry with p% (p = list
  value -378 = 20); reward moves the last choice (`f_288`) one place forward, punishment back.
- Body/mind: `f_28a` good stomach, `f_28c` junk stomach, `f_28e` bowels, `f_290` mood (−100..100),
  `f_292` fleas, `f_294` rabies, `f_296` on heat, `f_298` (set by snacks/shocks),
  `f_29a..f_2a2` saved, meaning TBD. Predicates: starving `f_28a<1`; needs toilet `f_28e>99`;
  junk-sick `f_28c>f_134`; depressed −100..−50; suicidal `<−99`; happy 50..99; over-excited `>99`.
- Owned objects: `f_138` array of `f_13c` module ids (from -337), re-spawned on load (`c1fc`).

## Main flows

- **Decide** (`0962`, on entering -324): per mode → `2aa4` start activity (`b10e` picks from
  the preference list, overridden by needs), `1246` interact, `2da6` wander, `2fcc` react,
  `3012` idle.
- **Walk** (`5136`): set walk target, choose walk state (`a530` personality table, or turn
  states -459/-460 when facing the wrong way), on arrival enter `f_186`.
- **Life tick** (`e452`, every second; ×60 with AdvanceTime): age, fleas spawn flea objects
  (-518); every minute: save (`8f32`), digest (`c4e4`), mood/personality drift (`c5b4`), maybe
  catch fleas (`e0c0`, 1/240), on heat (`e104`, birth hour and minute > 54), attention
  (`e17e` → pop-up `1000:47f6`), mouse visits (`e2a4`), phone calls (`e3a8` → `c9ee`: -209 vet
  when ill, -208 keeper when messy, -207 therapist when mood extreme); every 2 h of age send
  -165 to the food flap (-74).
- **Entrance**: top-level property -322=1 means visible. Hatch message -153 sets mode 2 and
  enters -324; the ordinary walk logic brings the pest from x=-100 into the room. See
  `engine-runtime.md` for the visibility correction and original drawing evidence.
- **Events** (`5b54`): click → `f_178`; -502 shock, -494 snack (mood ±`f_13e`), -448 flea spray,
  -202/-201 megaphone teach/command (`b5a2`), -27..-40 feed/clean/stomach changes
  (`c3b6`, `c426`, `c472`, `bc00` reward/punish), -1 re-enter after removal (`6736`), …
- **Save** (`8f32` → `PEST.TMP` renamed to `PEST.CFG`, magic 0x3b978a5e) / **load** (`962c`).
