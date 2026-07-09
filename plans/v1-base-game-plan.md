# Plans: CS-5850 Final Project — "Reactor Drone"

Three independent plans in one file (plan-mode constraint). **On approval, the first execution step is to split them into three standalone documents** — `110-finalproject-conradmisz/plans/v1-base-game-plan.md`, `plans/v2-upgrade-plan.md`, and `plans/v3-production-plan.md` — so v1 can be built in isolation and each later plan layered on the finished previous one.

---

# PART 1 — V1 Base Game Plan (build this first, submit, tag `final`)

## Context

Build the Class-110 final game described in `110-finalproject-conradmisz/submission/design.md`: a top-down arena survival shooter (maintenance drone in a circular reactor core, ring-spawning enemy waves, mouse-aim shooting, XP/upgrades, title → play → win/lose flow). The design doc mandates forking **Class-090** (tower defense) — its ECS engine, sprite-sheet animation, quadtree collision, wave spawner, and damage pipeline are all reused; the tilemap/A* parts are dropped.

Stack reality: **C++17 / CMake 3.20 / SDL3 + SDL3_image + SDL3_ttf**, Catch2 v3.5.2 tests (unit + `GENERATE`-based property tests), nlohmann_json, Lua 5.4. No JS/TS anywhere.

Hard gates (from `rubric.md` + `how-to-submit-final.md`):
- Fresh clone builds via `python run.py` with **zero warnings under `-Wall -Wextra -Wpedantic`**, CMake range `3.20...4.0`, Python-only scripts.
- Engine tests (`ctest -R "^Engine"`) and game tests (`ctest -R "^Game"`) at **100%**.
- ≥1 game-specific unit test and ≥1 property test (`NUM_OUTER_TESTS=10`, `NUM_INNER_TESTS=5`).
- SDL3 only, bottom-left origin (Y-flip only in `RenderSystem::draw_entity()`), course naming/structure.
- Title / in-game / end state reachable; `GameData.json` committed; `submission/design.md` + `postmortem.md` present; `final` tag.

User decisions (interviewed): build **inside `110-finalproject-conradmisz/`**; **reuse 090 assets + CC0 drone sprite**; ship the **full design doc scope** (XP, upgrades, victory wave); **port the 070-option audio system**.

Deadlines: demo video Aug 10, `final` tag Aug 18 (today: Jul 7).

## Key paths

- Fork source: `090-class-pathfinding-ai-tower-defense-conradmisz/` (`CPP/engine`, `CPP/game`, `assets/`, `run.py`, `runTestsAll.py`, `runEngineTests.py`, `runGameTests.py`, `compileGame.py`, root `CMakeLists.txt` at `CPP/CMakeLists.txt`)
- Audio source: `070-option-audio-system-sound-design-conradmisz/CPP/engine/audio_manager.{hpp,cpp}`, its SDL3_mixer FetchContent block (`CPP/CMakeLists.txt:63-78`), `engine/tests/unit/test_audio_manager.cpp`
- Target: `110-finalproject-conradmisz/` (currently docs-only: `submission/`, `rubric.md`, etc.)
- Canonical system-update ordering to replicate: `090.../CPP/game/main.cpp:697-760`

## Phase 0 — Scaffold + green baseline

1. Copy 090's `CPP/`, `assets/`, and Python runners into `110-finalproject-conradmisz/` (keep existing `submission/` and workbook docs). `git init` if the folder isn't already a repo with a remote for tagging.
2. Add `-Wpedantic` to the GCC/Clang flags in `CPP/CMakeLists.txt` (090 only has `-Wall -Wextra`; the checklist grades with `-Wpedantic`). Confirm `cmake_minimum_required(VERSION 3.20...4.0)`.
3. Build fresh + run `runTestsAll.py`. **Fix any pedantic warnings in the inherited engine/game code before writing a line of new code** — this is the baseline the whole project must keep. Verify ctest test names match the `^Engine` / `^Game` regex convention.

## Phase 1 — Strip tower-defense game code

Keep the **engine** untouched (its tests must pass 100%, including `pathfinding`/`tile_map` engine tests — unused ≠ deleted). In `CPP/game/`, remove tower-placement/pathfinding gameplay: tower placement UI, `PathFollower` waypoint logic, tilemap loading in `GameData.json`. Keep and rename per design doc:
- `tower_components.hpp`: `TowerStats` → `WeaponStats`; keep `DamageEvent`, `ProjectileTag`, `ProjectileData`.
- `enemy_components.hpp`: keep `EnemyTag`, `Health`, `HealthBarTag`; `PathFollower` → `SeekPlayer` (bare steering target).
- Keep `projectile_movement_system`, `projectile_hit_system`, `damage_apply_system`, `enemy_death_system`, `enemy_cleanup_system`, `health_bar_system`, `game_hud_system`, `game_state` — adapt, don't rewrite.

## Phase 2 — Core loop (new/modified systems)

New components (design doc, exactly 3): `PlayerTag`, `Experience {xp, level, threshold}`, `ContactDamage`.

Systems, in the design's own list:
- `PlayerControlSystem` (mod): 8-way WASD/arrows via existing `Input` component → `Velocity`.
- `PlayerAimSystem` (new): mouse world position (blackboard, camera-aware) → player `Rotation` facing cursor. **Flagged risk #1 in the design — write its angle math as a free function in a header so it's unit/property testable.**
- `PlayerFireSystem` (new): hold mouse/space + `WeaponStats` cooldown → spawn projectile entities along aim vector (reuse projectile pipeline).
- `EnemySeekSystem` (new): normalize (player − enemy) → `Velocity` (replaces path following).
- `PlayerDamageSystem` (new): `CollidedWith` + `ContactDamage` → `DamageEvent` on player, with i-frame timer.
- `WaveSpawnerSystem` (mod): spawn on arena ring edge (random angle, fixed radius) instead of path start; escalating waves + optional `victory_wave` from `GameData.json`.
- `CameraFollowSystem` (mod from 040-style camera): lerp camera to player, clamped to arena.
- `GameStateSystem` (mod): title → play → game-over/victory; click/space to start/restart; escape quits.
- `GameHUDSystem` (mod): HP, wave, score, XP/level.

`GameData.json`: rewrite per the design's schema — window, camera, arena radius, player block (speed, hp, i-frames, weapon stats), `enemy_types` (max 3: reuse `enemy_fast`, `enemy_runner`, `enemy_armored` atlases), `waves`, `victory_wave`, `xp_curve`, `upgrades`. Every tunable lives here, none in code.

## Phase 3 — XP / upgrades / victory

- `ExperienceSystem` (new): enemy death → XP grant → level-up when `xp >= threshold`, next threshold from `xp_curve`.
- `UpgradeSystem` (new): on level-up, weighted-random pick from `upgrades` pool in `GameData.json`, auto-apply (mutate `WeaponStats`/player stats) + flash HUD text — matches the design's "no real UI" mitigation.
- Victory when the configured `victory_wave` is cleared.

## Phase 4 — Audio port (070-option)

1. Copy `audio_manager.{hpp,cpp}` into `CPP/engine/`, copy the SDL3_mixer FetchContent block (VORBIS vendored, FLAC/MOD off, MP3 on) into `CPP/CMakeLists.txt`, link `SDL3_mixer`.
2. Copy `test_audio_manager.cpp` into `engine/tests/unit/` and register it (it supports the nullptr-mixer headless path, so ctest stays silent/green).
3. Game-side: small `AudioTriggerSystem` (or direct calls at spawn/damage sites) for fire, enemy death, player hit, level-up, game over. Volume/filenames in `GameData.json`.

## Phase 5 — Assets (open-source first)

- **Reuse from 090** (already Kenney-derived, license-clean, sidecars exist): `enemy_fast/runner/armored.png`, `effect_explosion.png` (hit/death VFX), `projectile_cannonball.png`, `fonts/default.ttf`.
- **Player drone**: source CC0 from Kenney (Space Shooter Redux or Top-Down Tanks Redux, kenney.nl — CC0, no attribution required). Cut a single-frame (or 2-frame thruster) atlas + hand-write the sidecar `.json` (schema: `{atlas, frame_width, frame_height, columns, total_frames, animations}`). Fallback if download is unusable: render one via 090's `assets/generator/` glb pipeline.
- **SFX**: CC0 from Kenney audio packs (Sci-Fi Sounds / Digital Audio): laser shot, impact, explosion, level-up chime, game-over sting → `assets/Audio/`.
- Update `assets/asset_manifest.json`; record all sources + licenses in a short `assets/CREDITS.md`.

## Phase 6 — Tests (gate: 100%)

- Engine tests: **do not modify** except adding the audio test; all inherited engine tests must keep passing.
- Game tests: adapt inherited 090 game tests that still apply (damage pipeline, projectiles, lifetime); delete ones testing removed tower/path logic.
- New game unit tests (Catch2, headless — pure logic in headers/free functions): aim-angle math, fire cooldown, XP threshold/level-up, i-frame gating, ring-spawn position on radius, weighted upgrade selection (seeded RNG).
- New property tests (`GENERATE`, `NUM_OUTER_TESTS=10` / `NUM_INNER_TESTS=5`): aim angle round-trips for arbitrary cursor positions; spawn points always within ε of arena radius; XP never decreases / level monotone; damage with i-frames never applies twice within the window; upgrade picker always returns a pool member with all weights ≥ 0.
- TDD for the new pure-logic functions: test first, then implement.

## Phase 7 — Hardening + submission mechanics

1. **Fresh-clone rehearsal** (the checklist's Part 4, and the graded gate): clone into scratch, `python run.py`, capture full build log and assert **zero warnings**, `ctest --test-dir build -R "^Engine"` and `-R "^Game"` both 100%, play title → game → end state, clean exit.
2. Repo hygiene: no `.sh`/`.bat`, no uncommitted changes, no large stray binaries, no secrets.
3. Write `submission/postmortem.md` (all 4 questions: what worked / what didn't / what was cut / engine debt). Note any divergence from `design.md`.
4. Tag: `git tag final` + push tags — **only when the user confirms it's submission-ready** (outward-facing, irreversible-ish).
5. Demo video (Aug 10) is the user's task; provide a suggested walkthrough outline (core loop, systems/components tour, one hurdle — mouse-aim math or perf are the pre-declared candidates).

## Verification (continuous, not just at the end)

After every phase: `python runTestsAll.py` (configure + build + ctest) and confirm zero warnings in the build output (grep the log for `warning:`). Run the game (`python run.py`) and exercise the new mechanic manually. The Phase 7 fresh-clone run is the final proof.

## Risks (from the design doc, with mitigations)

1. **Mouse-aim math** — isolate as pure functions, property-test round-trips, respect bottom-left origin (Y-flip only in RenderSystem).
2. **Perf with no particle system** — quadtree broad-phase already in engine; hit effects are short-`Lifetime` explosion-atlas entities; enemy counts tunable in `GameData.json`.
3. **Upgrade readability with no UI** — auto-apply + HUD flash text (design's own mitigation).
4. **SDL3_mixer FetchContent** adds build time/deps — 070-option proves it builds warning-free in this course setup; keep its exact cache flags.

---

