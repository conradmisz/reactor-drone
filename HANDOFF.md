# Reactor Drone v2 — Project Handoff

Snapshot for picking up the v2 visual overhaul. Read this, then
`plans/v2-visual-overhaul-plan.md` for the full phase-by-phase spec.

---

# UPGRADE PLAN (4 phases) — current work

Full plan: `~/.claude/plans/i-would-like-to-iterative-tarjan.md`. Goal: fix aim feel,
expand the map ~22×, make terrain solid and sprite-based. One phase per session,
`/clear` between them. **Phases 1–2 are done; Phase 3 is next.**

## Upgrade Phase 1 — Aim & movement feel ✅ COMPLETE

**Problem it fixed.** Aiming/firing math was already correct
(`player_aim_system.cpp` → `Rotation.angle` → `player_fire_system.cpp:43-48`). The
*feel* was broken by rendering: the player used the leftover Kenney atlas
`images/enemy_boss.json` and spawned with `Rotation{0,0}`, leaving
`flip_when_left = true`, so `render_system.cpp:227-233` **mirrored** the sprite past 90°
instead of rotating it. Plus arrows-only input and ~41%-faster unnormalized diagonals.

**Changes.**
- `assets/GameData.json:101` — `player.sidecar` → `images/v2/player_drone.json`
  (clip `"march"` unchanged, exists in the v2 sidecar).
- `assets/GameData.json:113-115` — `enemy_types[].sidecar` → `images/v2/enemy_spark.json`,
  `enemy_runner.json`, `enemy_hulk.json` (was `enemy_fast` / `enemy_runner` / `enemy_armored`
  from the v1 tower-defense set).
- `CPP/game/main.cpp:264` — player `Rotation{0.0f, 0.0f}` → `Rotation{0.0f, 0.0f, false}`.
  Enemies have **no** `Rotation` component at all; untouched.
- `CPP/engine/ecs/systems/input_system.cpp:54-59` — WASD OR-ed into the existing
  `up/down/left/right` flags. (The file is 81 lines — earlier notes citing `:125-129`
  were wrong.)
- `CPP/engine/ecs/systems/player_control_system.cpp` — after axis accumulation, both
  components ×`0.70710678f` when both are non-zero.
- New test `CPP/game/tests/property/test_player_control_properties.cpp` — sweeps all 16
  key combinations × 10 random speeds, asserts magnitude == move_speed for every live
  input and 0 for the two idle cases. 320 assertions.

**Bug found and fixed along the way (was blocking, would have silently degraded).**
Every v2 sidecar wrote `"atlas": "images/v2/foo.png"`, but
`ResourceManager::load_texture` prepends `assets/images/`, so paths resolved to
`assets/images/images/v2/...` and fell back to the magenta checkerboard. Root-caused at
the generator: `assets/generator/v2/common.py:91` now writes `"v2/{name}.png"` (bare,
relative to `assets/images/`, matching the v1 convention). All 9 committed sidecars in
`assets/images/v2/*.json` were rewritten to match, and
`assets/generator/v2/test_manifest.py:34` was updated (its `os.path.relpath(atlas, "images")`
would have broken on the corrected form). **If you add or regenerate a v2 sprite, the
atlas path is bare `v2/<name>.png`.**

**Verified.** Build clean under `-Wall -Wextra -Wpedantic`; `runTests.py` 6/6 suites
pass; headless run shows zero texture warnings; screenshots with `--hover` injected
left and up confirm the drone's nose follows the cursor with no mirror flip.

**Notes for later phases.**
- `Pillow is not installed` on this machine, so `assets/generator/v2/*.py` (including
  `test_manifest.py`) cannot be run. The generated PNG/JSON assets are all committed, so
  this only matters if you need *new* art.
- `--keys` key names are **uppercase** (`5:SPACE`, not `5:space`).
- The screenshot flag is `--screenshot N ...` (BMP output), not `--screenshot-frames`.
- Useful smoke recipe:
  `./CPP/build/game/game --seed 42 --stopframe 220 --screenshot 150 200 --keys 5:SPACE --hover 150:200,330`
  then `convert <log>/000150-screenshot.bmp out.png` to view it.
- Deliberately skipped: aim smoothing / turn rate, acceleration+drag. Revisit only if
  the drone feels weightless after the Phase 2 camera change.
- Deliberately not fixed: `InputSystem` unprojects the cursor through the *shaken*
  camera, so the world cursor jitters a few px during screen shake. That is
  geometrically correct — the cursor really is over that world point.

## Upgrade Phase 2 — Big arena + follow camera ✅ COMPLETE

**Changes.**
- `assets/GameData.json:11-14` — `arena.radius` 300 → **1400**, `spawn_radius` 340 →
  **620** (now *distance from the player*, not from the arena centre), `center_x/y`
  (480,330) → **(1600,1600)**. `assets/GameData.json:98-99` — `player.start_x/y` →
  1600,1600.
- `CPP/game/main.cpp:137-142` — quadtree world bounds now `arena.radius + margin`
  (was `spawn_radius + margin`).
- `CPP/game/main.cpp:153-158` — A* grid dims use `center + radius` (was
  `center + spawn_radius`). At `cell_size = 40` that is 76×76 cells.
- `CPP/game/main.cpp:493-524` — **follow camera**: the Phase-4 shake block now takes
  its base look-at from the `PlayerTag` entity's centre (arena centre when no player
  exists — title / game-over), then adds the shake offset. No new machinery.
- `CPP/game/wave_spawner_system.hpp` — new pure `ring_spawn_point(player_x, player_y,
  angle, spawn_radius, center_x, center_y, arena_radius)` returning a `Vec2`
  (`obstacles.hpp`): the point at `spawn_radius` from the player on `angle`, pulled
  back onto the arena circle if it lands outside.
- `CPP/game/wave_spawner_system.cpp:81-99` — ring-spawns around the player via that
  function. **The RNG angle is drawn before the player lookup, unconditionally**, so
  replay determinism is unchanged; keep it that way.
- Comments corrected where they claimed a fixed camera: `main.cpp:117-119`,
  `main.cpp:493-499`, `main.cpp:529-532`, `CPP/game/parallax.hpp:12-14`.
- New test `CPP/game/tests/property/test_spawn_ring_properties.cpp` — for any player
  position inside the arena and any angle, the spawn is inside `arena.radius`, and it
  is exactly `spawn_radius` from the player whenever no clamp fired.
- `CPP/game/tests/property/test_arena_properties.cpp:59-63` — that older spawn-ring
  property left `cfg.arena.radius` at its 320 default while randomising
  `spawn_radius` up to 400, so the new clamp fired and it failed. It now sets
  `radius = spawn_radius + 100`. **Config invariant: `arena.radius > spawn_radius`.**

**Verified.** Build clean under `-Wall -Wextra -Wpedantic` (only the accepted Lua
`tmpnam` linker warning); `ctest` **8/8**; headless run to frame 600 shows no texture
warnings and the drone screen-centred with enemies arriving from every direction.
Camera follow was proven by temporarily setting `player.start_x` to 2100 (reverted):
the drone still renders dead-centre and the parallax layers shift — under the old
fixed camera it would have rendered 500px off-centre.

**Gotcha for the next phase.** `--keys` only understands `ESC`, `SPACE` and `F1`
(`main.cpp:347-351`) — there is **no directional key injection**, so you cannot drive
the drone across the map headlessly. Use an off-centre `player.start_x/y` for
camera/scroll checks instead.

**Known-sparse, as planned.** The three arenas' obstacles/hazards are still authored
around (480,330) — a full 1100+ units from the new arena centre and completely
off-screen at spawn. The map reads as empty terrain. Phase 4 rewrites those layouts.

*Skipped:* camera smoothing / look-ahead, camera bounds clamping at the arena edge.
Add smoothing only if the hard follow reads as stiff.

## Upgrade Phase 3 — Visible boundary wall (NEXT)

See the plan file. In short: keep the circular clamp at `main.cpp:397-413`; spawn ~80
decorative wall segments on the `arena.radius` circle inside `spawn_arena_props`
(`main.cpp:213-236`) with `Position`/`Size`/`Images{v2/pillar_<theme>.png}`/
`RenderLayer{2}` and **no `Collider`** — the clamp stays the real wall.
`clear_arena_props()` already tears them down on arena swap.

---

## What this is

Top-down neon arena survival shooter on the CS-5850 / hearth-and-hollow (Class-110)
ECS engine. v2 is a visual + systems overhaul of the original *Reactor Drone*:
procedural neon art, particle system, additive glow, screen shake / hit feedback,
parallax backdrops, three wave-progressed arenas with solid obstacles + contact
hazards, and A* enemy pathfinding.

- World space is **bottom-left origin** (+X right, +Y up). The Y-flip lives **only**
  in `RenderSystem::draw_entity()`.
- Every tunable lives in `assets/GameData.json`, never hard-coded.
- Deterministic replay is a hard invariant: `game --seed S --keys ... --stopframe N`
  → same seed = same result. No `Date.now()`/`rand()` outside the seeded RNGs.

## Build / run / test

```
python run.py     # interactive menu: 1 compile, 2 run, 6/7 all tests, 9 game tests
```

Direct (bypasses the interactive menu, handy for agents):

```
# configure+build once
cd CPP/build && cmake . && cmake --build . -j
# run the game (opens an SDL window)
./CPP/build/game/game
# headless deterministic smoke run
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ./CPP/build/game/game --seed 1234 --stopframe 900
# tests
cd CPP/build && ctest            # all 8 suites
./game/tests/game_unit_tests     # game unit tests (Catch2)
./game/tests/game_property_tests # game property tests
```

**Note on the test globs:** `CPP/game/tests/CMakeLists.txt` globs test files at
*configure* time. After adding a new `test_*.cpp`, re-run `cmake .` in the build dir
before `cmake --build`, or the new file is silently ignored.

## Status

`git log` HEAD is committed **through Phase 4**. Phases **5, 6, and 7 are complete
but uncommitted** — they live in the working tree (the user handles git). All gates
are green as of this handoff: zero warnings under `-Wall -Wextra -Wpedantic` (only the
accepted Lua `tmpnam` linker warning), and **8/8 ctest suites at 100%**.

| Phase | What | State |
|-------|------|-------|
| 0–3 | v2 bring-up, neon asset pipeline, render upgrades (Tint/additive/flip), particle system | committed |
| 4 | Hit feedback + screen shake (`feedback.hpp`, `Flash`) | committed |
| 5 | Parallax backdrops (`parallax.hpp`, `RenderSystem::render_layers`/`TiledLayer`) | **working tree** |
| 6 | Multi-arena + obstacles + contact hazards | **working tree** |
| 7 | A* enemy pathfinding around obstacles | **working tree** |
| 8 | Audio port | **intentionally left out — see below** |
| 9 | Hardening & wrap | not started |

### Uncommitted work — files touched

- **Phase 5 (parallax):** `CPP/game/parallax.hpp` (+ tests), `render_system.{hpp,cpp}`
  (`TiledLayer` / `render_layers`), `main.cpp` backdrop-layer loop, `GameData.json`
  `backdrop_layers` with per-layer `scroll_factor`.
- **Phase 6 (arenas/obstacles/hazards):** `arena_config.{hpp,cpp}` (`ArenaDef`,
  `ObstacleDef`, `HazardDef`, `active_arena_index`), `obstacles.hpp`
  (`push_circle_out_of_aabb`), `collision_layers.hpp` (OBSTACLE/HAZARD layers +
  masks), `main.cpp` arena-swap + obstacle push-out, `projectile_hit_system` /
  `player_damage_system` (shots stop on obstacles, hazards bleed the drone), tests
  `test_arena_multi*`.
- **Phase 7 (pathfinding):** `CPP/game/enemy_path.hpp` (new, pure), rewritten
  `enemy_seek_system.{hpp,cpp}`, `enemy_components.hpp` (`PathFollower` gained
  `repath_timer`/`target_x`/`target_y`), `arena_config.{hpp,cpp}` +
  `GameData.json` `pathfinding` block, `main.cpp` grid build on arena swap, tests
  `test_enemy_path*`.

## How Phase 7 pathfinding works (most recent addition)

Enemies steer straight at the player when line-of-sight is clear (cheap), and fall
back to the engine's A* (`CPP/engine/pathfinding.hpp`, `find_path`) when an obstacle
blocks the shot.

- `enemy_path::build_obstacle_grid(...)` rasterises the active arena's obstacle AABBs
  into a `TileMap` (cells an obstacle — grown by `clearance` — covers become
  unwalkable), so the engine's grid-based `find_path` runs unchanged.
- `enemy_path::line_of_sight_clear(...)` is a segment-vs-AABB slab test (obstacles
  inflated by the enemy's radius) — the fast-path gate.
- `EnemySeekSystem`: LOS clear → straight seek; LOS blocked → path to the player's
  cell, steer toward the next cell centre, recompute only every `repath_interval`
  seconds (tracked per-enemy on `PathFollower`, so it stays deterministic).
- The grid is rebuilt on each arena swap in `main.cpp`.
- Tunables: `GameData.json` → `pathfinding: { repath_interval, cell_size, clearance }`.
- A* is **4-connected** (paths look slightly blocky). If that reads poorly, add
  8-connected neighbours in `pathfinding.hpp`.

## Audio — intentionally left out

**Phase 8 (audio port) was deliberately skipped and is not implemented.** It is fully
independent of the visual/gameplay phases and can be added any time without touching
existing systems. When resuming it, the spec is in `plans/v2-visual-overhaul-plan.md`
§Phase 8: port `audio_manager.{hpp,cpp}` from `070-option-audio-system` into
`CPP/engine/`, add the SDL3_mixer FetchContent block to `CPP/CMakeLists.txt`, wire
game-side triggers (fire / enemy-death / player-hit / level-up / game-over), generate
neon SFX via `assets/generator/v2/`, and record filenames/volumes in `GameData.json`.
The README currently advertises "a full audio port" — that line is aspirational and
should be trimmed until Phase 8 actually lands.

## Remaining after this

- **Phase 8** — audio (see above).
- **Phase 9** — hardening: fresh-clone rehearsal (zero warnings, both ctest suites
  100%, title → play → win/lose → clean exit), deterministic-replay demo screenshots,
  update `README.md` / `assets/CREDITS.md` / manifests, repo hygiene.
- Commit Phases 5–7 (currently uncommitted working tree).

## Gotchas

- Adding a new ECS component means registering its storage — there's known friction
  here, so Phase 7 reused the existing `PathFollower` component rather than adding one.
- Score stays 0 in scripted headless runs because firing needs a real mouse-aim
  vector the scripts don't provide — that's expected, not a bug; use it only for
  determinism/crash checks.
- Enemies don't get an obstacle push-out (only the player does), so pathing is what
  keeps them from burrowing into walls — keep A* wired if you touch seeking.
