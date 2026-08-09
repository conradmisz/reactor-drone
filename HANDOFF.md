# Handoff — Reactor Drone v2

You are picking up mid-project. **Phase A (fix-ups), Phase B (difficulty + hard
mode) and iteration 3's Phase 0 (scaffolding) are all DONE and verified.**
Nothing is committed.

**The next work is iteration 3, and it has an approved plan file:**
`/home/conrad/.claude/plans/create-a-plan-to-polymorphic-gosling.md`.
Read it before anything else — it covers all 13 items from the user's latest
Game Notes, split into a scaffolding phase (done) plus five parallel lanes (not
started). The user intends to run those lanes as a **multi-agent workflow**, and
the plan is written for that: file ownership per lane, reserved decision-id
ranges, and a merge order. Do not start lane work without re-reading it.

## Read first

1. `CLAUDE.md` — project rules (they override defaults)
2. `ENGINE.md` — the architecture doc. It is already accurate for everything
   below. Project rule: any engine change updates it **in the same commit**.
3. `agentProjectDocs/progress-tracker.md`
4. `/home/conrad/.claude/plans/snappy-wiggling-snail.md` — the original approved
   plan. Parts 1–3 are done; Parts 4–5 (menu screens, save/load) are still open
   but have been **overtaken** by the user's Game Notes below.

---

## Re-establish the baseline before changing anything

```bash
cd CPP && cmake -B build -S . && cmake --build build -j$(nproc)
(cd build && ctest)     # 8/8 must pass
```

- Zero warnings from our code under `-Wall -Wextra -Wpedantic`.
  Lua's vendored `tmpnam` linker warning is expected and allowed.
- Determinism canary — two runs of the same seed *and difficulty* must print a
  **byte-identical** summary line (Phase B rebalanced the waves, so the line no
  longer matches the pre-Phase-B one; the invariant is run-to-run, not
  release-to-release):
  ```bash
  SDL_VIDEODRIVER=dummy ./CPP/build/game/game --seed 42 --keys 10:SPACE --stopframe 12000
  # Frames: 12000  Final score: 0  Credits: 0  Wave: 1  Phase: 2
  ```
  `--keys N:SPACE` starts a **Normal** run. For Hard, click the menu:
  `--clicks 10:622,349` (and `10:358,349` is NORMAL). stdout prints
  `Run start: difficulty <name>`.

---

## What is already built (do not redo)

### The Option-040 UI/menu layer is ported and live

Copied from `../040-option-ui-menu-system-conradmisz/CPP/` into `CPP/engine/`:
`ui_style.{hpp,cpp}`, `ui_focus_math.hpp`, `ui_fade_math.hpp`, and under
`ecs/systems/`: `ui_render_math.hpp`, `ui_system.*`, `ui_render_system.*`,
`screen_stack_system.*`, `screen_fade_system.*`. The whole 040 test suite came
across and passes.

Merged into existing files: `components.hpp` (the five UI structs),
`component_storage.{hpp,cpp}`, `destruction.cpp`, `input_system.cpp`,
`gamedata_loader.cpp` (optional `ui_styles` + `screens` blocks),
`lua_bindings.cpp`, `game/debug_adapters.{hpp,cpp}`, and the CMake lists.

Two v2-only additions on top of the port:

1. **`UIElement::pulse_hz`** + `pulse_alpha_scale()` / `apply_alpha_scale()` in
   `ui_render_math.hpp`. Render-only, driven by a `UIRenderSystem::elapsed_`
   member that is deliberately **not** a Blackboard key, so replay determinism
   is untouched. `pulse_hz == 0` returns exactly `1.0f`.
2. **`UISystem::UI_CLICK_KEY`** (`"ui.clicked_fn"`). On a confirmed click
   UISystem publishes the widget's `on_click_fn` to the Blackboard as well as
   dispatching to Lua. **This game uses no Lua menu layer** — `main.cpp`
   constructs a `LuaManager` only to satisfy UISystem's constructor and reads
   clicks off the Blackboard. The consumer must
   `blackboard.remove(UISystem::UI_CLICK_KEY)` after handling.

### Screens live in `assets/GameData.json`

Top-level `ui_styles` and `screens` keys. Rects are authored in the **800×600
design canvas**, which `ui_canvas_transform` letterboxes onto the 980×660
logical surface (scale 1.1, x-offset 50). Draw and hit-test both apply it, so
they cannot diverge.

Existing screens: `wave_intermission`, `pause`, and `gameplay`.

`gameplay` is special: `"gameplay"` is ScreenStackSystem's **base sentinel**, so
a screen with that name is always active and never modal. It is the home for
permanent HUD furniture (the hull/shield gauge panels).

### Phase A fix-ups (all done, from the user's Game Notes)

| Note | What shipped |
|---|---|
| Credits unreachable at wave 4 | `PHASE_INTERMISSION` no longer freezes — it runs control, aim, movement, arena/obstacle clamps, pickups, lifetimes; no combat. |
| ESC should pause | ESC pushes the `pause` screen at base depth, pops the top screen otherwise. |
| Health as a bar | Hull + shield gauges with a chip bar (lost chunk bleeds away over ~0.5s); fill green→amber→red at 55%/25%. |
| Hazards more menacing | Always red regardless of arena, additive, 0.9 Hz throb, ember plume, drawn above obstacles (`RenderLayer{3}`). |
| Arena transition | 1.2s → 5s with a smoothstep ramp. |
| Verify resizing | Verified structurally only (see traps below). |

The pause menu is Resume/Quit only; Save needs the save system and Options needs
an Options screen (Phase C). The screen says so rather than showing dead buttons.

---

### Phase B — difficulty + hard mode (done)

Spec: `agentProjectDocs/specs/difficulty-modes.md`. Design call: **D50**.

| Piece | Where |
|---|---|
| `DifficultyDef` + `apply_difficulty` | `CPP/game/arena_config.hpp` (header-only, pure) |
| `difficulties` parse | `CPP/game/arena_config.cpp` |
| `base_config`, `start_run()`, title menu | `CPP/game/main.cpp` |
| `difficulties` + `main_menu` + rebalanced `waves` | `assets/GameData.json` |
| Tests | `CPP/game/tests/unit/test_difficulty.cpp` (7 cases) |

The user's tuning answers, now locked in and shipped:

- **Normal early waves = "aggressive"**: wave 1 is 12 enemies at 0.45 s with
  spark + runner, hulks from wave 3, wave 4 at 26 @ 0.30 s, and **+25% base
  enemy speed** (spark 95→119, runner 70→88, hulk 45→56). Waves 5-11 were
  rescaled to stay above wave 4; the timed waves 12-20 kept their durations and
  hp/speed mults and only tightened `spawn_interval` (0.30 → 0.20).
- **Hard = "+50% / noticeably harder"**: ×1.5 count, ×0.7 spawn spacing, ×1.3
  HP, ×1.15 speed, ×1.4 credits, ×1.5 hazard damage, `type_lookahead: 2`.

**All of it is provisional and unplayed** — it is the target the user named, not
a measured curve. Every number is a `GameData.json` edit with no rebuild.

Traps this phase added:

- `apply_difficulty` is **not idempotent**. `main.cpp` keeps a pristine
  `const GameConfig base_config` and re-copies it on each run start.
- `type_lookahead` merges each wave's roster with the next N, reading the
  *original* rosters. An empty `types` means "every type" and is never merged
  into — merging could only narrow it.
- At `PHASE_TITLE` the start fallback is `space_edge`, **not** `advance`:
  `advance` includes the mouse click, so clicking HARD would also have started a
  Normal run in the same frame.
- Escape is ignored at `PHASE_TITLE`, or it pops the only screen there is.

Still open from the hard-mode interview: *"more lethal ... boss"* cannot ship
until the boss does (Phase C).

---

### Iteration 3 Phase 0 — scaffolding (done)

Design call: **D51**. Architecture: `ENGINE.md` §6. Test:
`CPP/game/tests/unit/test_scaffolding.cpp` (6 cases).

It adds **no behaviour** — that is the point, and it is why the gate was the
replay canary staying byte-identical. What it added, once, so five parallel
lanes never have to touch a shared file:

| Added | Where |
|---|---|
| `EnemyShot` (tag) + `EnemyBehavior` + `behavior_kinds` | `game/enemy_components.hpp`, registered in `component_storage.{hpp,cpp}`, swept in `destruction.cpp` |
| `layers::ENEMY_SHOT` (0x20); `PLAYER_MASK` and `OBSTACLE_MASK` widened | `game/collision_layers.hpp` |
| `ShipState::dash_cd/dash_timer/active_id/active_cd/gear_levels[8]`; `PickupKind::Health/Shield` | `game/player_components.hpp` |
| `SustainConfig`, `DashConfig`, `MinimapConfig`, `BossConfig`, `ActiveItemDef`, `WaveDef::boss`, `ArenaDef::specialty_unit/_tier`, `EnemyType` behaviour fields + parse | `game/arena_config.{hpp,cpp}` |
| Nine `// === HOOK: <lane> ===` blocks | `game/main.cpp` (slots listed in `ENGINE.md` §6) |
| Six owned data blocks (`sustain`/`dash`/`minimap`/`boss`/`actives` + the note) | `assets/GameData.json` |

Two findings worth keeping:

1. **Enemy projectiles need no damage system.** A shot carrying `ContactDamage`
   on the `ENEMY_SHOT` layer hurts the drone through the path
   `PlayerDamageSystem` already runs for hazards. Lane D's Phase 6 is smaller
   than the plan assumed.
2. **The scaffolding defaults are all inert** (`sustain.interval 0`,
   `minimap.enabled false`, `actives []`, no wave flagged `boss`, no enemy type
   with a `behavior`). `test_scaffolding.cpp` asserts that inertness — the lane
   that turns a feature on **deletes its line there** and asserts the real
   behaviour in its own test file.

### How the lanes are meant to run

Full rules in the plan file; the load-bearing ones:

- **A lane edits only** its own new files, its one `// === HOOK: … ===` block,
  and its one `GameData.json` block. `component_storage.*`, `components.hpp`,
  `collision_layers.hpp` and the CMake lists are **off-limits** — a lane that
  believes it needs one stops and reports instead of editing.
- Lane D is strictly sequential (enemy projectiles → specialty units → boss).
  Lane B's three phases are independent.
- Merge order **A → B → C → E → D**; Lane A is the 50-wave data spine everything
  else is balanced against.
- Reserved decision ids: **A = D52-55, B = D56-60, C = D61-65, D = D66-75,
  E = D76-79, integration = D80+.** Next free id in `CLAUDE.md` currently reads
  **D52**.

---

## Reference: the content Phase C describes (now folded into the plan)

The plan's lanes supersede this list, but the verbatim design detail below is
still the source of truth for *what the boss and its items should feel like*.

- Moon enemy: **three separate `enemy_types` rows** (`moon_1`/`moon_2`/`moon_3`),
  gated by wave number, not one evolving type — slow projectiles → faster /
  tracking → lasers in the final stages. (Locked-in interview answer; do not
  re-ask.) Note they land in `enemy_types` *after* index 2, so every wave's
  `types` list and Hard's `type_lookahead` merge picks them up for free.
- Periodic health pickups ("green scrap") and shield pickups.
- Shop as a **real clickable menu**: *"The shop isnt a true menu, its just a
  text overlay."* Needs item tooltips on hover plus a preview of what the ship
  will look like equipped. (The catalogue interior in `shop_system.cpp` is
  currently untouched — only its entry was gated.)
- **Boss** at the halfway point: a big Starcraft-battlecruiser-style enemy, lots
  of HP, summons small ships. Killing it rewards a choice of three active items,
  all on a 30s cooldown:
  - *heat seeking missiles* — 8 missiles launched radially, seek nearby enemies,
    explode for AoE damage
  - *laser cannon* — 4 beams in the cardinal directions; beams shoot out, hold
    "set" for just under a second, then do a quick 360° sweep and vanish, with a
    particle trail
  - *reinforced optical electro repulsion device* — below 20% HP, auto-creates a
    5s forcefield: heals to full, knocks enemies back, and holds a sphere around
    the player that enemies cannot enter, travelling with the player
- Options screen (the pause menu already promises it).
- Boss lethality on Hard: `DifficultyDef` has no boss knob yet — add one field
  there rather than a boss-specific difficulty path.
- From the original plan, still open: `game_over`/`victory`,
  `save_slots` screens; wiring `ScreenFadeSystem` into the render order (it is
  ported but unused; it belongs between `hud_system.render` and
  `ui_render_system.render`); and run-state save/load per plan Part 4
  (`run_save.{hpp,cpp}`, autosave at the intermission edge, `saves/` in
  `.gitignore`, `test_run_save.cpp`). The decision was **run state only** — no
  entity-graph snapshot, so do not port `SerializationRegistry` / `LoadSystem`.

---

## Nothing is committed

`git status` shows a large tree mixing the user's own pre-existing Phase 5/6
work (arena crossfade, tie-dye enemies, Prism arena, item aura) with the whole
UI layer, Phase A, Phase B and iteration-3 Phase 0. **Ask before bundling them.**
A reasonable offer: their Phase 5/6 work first, then the engine UI layer, then
the intermission, then the Phase A fix-ups, then Phase B, then Phase 0. The user
has been told the diff is getting large; they have not yet chosen a split.

Worth raising early: the tree only grows once five lanes start. Committing the
existing slices **before** the lanes run would make each lane's diff readable.

Also still pending: the user has **not visually confirmed Phase A, the new title
menu, or the rebalanced waves in a real window**. Worth asking early — colours,
layout and feel are what tests cannot check, and "is wave 1 actually dangerous
now?" is a question only a playtest answers. Their explicit ask was *"Also verify
that resizing works !"*, which has only been verified structurally.

---

## Traps that cost real time in this session

1. **`ComponentStorage` uses explicit template instantiation.** A new component
   type needs a storage member, two `get_storage<>` specialisations, and six
   instantiation lines — or you get a wall of undefined references at link time
   that reads like a build-system fault.
2. **Engine `.cpp` files are listed explicitly in three CMake places**:
   `CPP/game/CMakeLists.txt` and twice in `CPP/engine/tests/CMakeLists.txt`.
   Only `CPP/game/tests/CMakeLists.txt` globs, and even it needs a reconfigure.
3. **`spawn_world()` destroys every entity.** It now explicitly skips entities
   carrying `UIScreen`/`UIElement`, because it was wiping the load-time menus
   from frame zero. Any future teardown must do the same.
4. **`--fps N` does not speed up a headless run** — the Timer still paces to
   60fps, so 60000 frames is ~17 minutes. To reach a late state, temporarily
   relax the trigger (the shop's `current_wave_index() % 4 == 0` → `% 1 == 0`),
   verify, then **revert**. Do not brute-force the frame count.
5. **`SDL_VIDEODRIVER=dummy`**, not `offscreen` — offscreen supplies no mouse,
   so the drone never aims and a zero score is expected rather than a bug.
   `--clicks FRAME:X,Y` drives the menu layer too; `main.cpp` supplies
   `mouse.down` and `mouse.up` in the same frame because UISystem only confirms
   a click from a press and release inside the same widget.
6. **Frame-order landmine in `main.cpp`**: scripted key injection happens after
   `input_system.process_events`, so the Escape-edge handling must come *after*
   the injection or scripted ESC silently does nothing.
7. **Pause vs intermission**: `sim` keys off
   `stack.back() == SCREEN_PAUSE`, deliberately **not**
   `ScreenStackSystem::is_modal()` — the intermission is modal too and must keep
   running so credits can be collected.
8. **Pause freezes the frame counter** (same as the F1 debug pause). Consequence:
   a headless run that pauses can never be resumed, because scripted keys are
   frame-indexed and frames stop advancing. A `--stopframe` timeout with the last
   line one frame after the ESC *is* the proof pause works.
9. **Window resize needs no plumbing and must not grow any.** The logical surface
   is pinned at 980×660 via `SDL_LOGICAL_PRESENTATION_LETTERBOX`, and the loader
   is the only writer of `window_width`/`window_height`. UIRenderSystem and
   UISystem therefore compute the identical transform.
10. **Ported 040 tests predate this project's zero-warning gate.** Two needed
    fixes on import (documented in `ENGINE.md` §5). Expect the same if you port
    more.
11. **`DEFAULT_MAX_PARTICLES` is 2000 and truncates silently.** The hazard embers
    already cost ~120 permanent particles, and ENGINE.md §5 records the budget
    *already* capping at wave 20. The 50-wave arc plus the boss, lasers, poison
    and arena destruction will make this bite — it is a first-class task in the
    plan's Phase 10, not a footnote.
12. **A screen-space entity must have no `Position`.** `RenderSystem` prefers
    `ScreenPosition`, but `CameraSystem` *adds* `ScreenPosition` to anything
    carrying a `Position` — so a minimap blip or a shop ship-preview with both
    gets its screen coords overwritten every frame. The HUD text entities are the
    working example.
13. **`apply_difficulty` is not idempotent** (D50). `main.cpp` keeps a pristine
    `const GameConfig base_config` and re-copies it at each run start. Anything
    that adds a difficulty-scalable number scales it *there*, not in a second
    place.
14. **Headless difficulty select:** `--clicks 10:358,349` = NORMAL,
    `10:622,349` = HARD (design-canvas button centres through
    `ui_canvas_transform`). `--keys N:SPACE` still starts Normal. stdout prints
    `Run start: difficulty <name>`, which is the only proof in a headless run.
15. **Editing `GameData.json` with a Python round-trip reformats the whole
    file.** Splice by line range and re-dump only the block you are replacing
    (`json.dumps(obj, indent=2)` plus a 2-space prefix matches the house style),
    then re-parse the file to prove it is still valid. A bad string slice in this
    session silently wrote `"ustain"` / `"ash"` as top-level keys; the re-parse
    is what caught it.
