# Reactor Drone v2 — Visual Overhaul: remaining-work plan

Reconstructed from README + git history + working-tree WIP (no saved plan existed).
Phases 0–3 are **already committed**. Each phase below is independently shippable and
must leave every gate green before it is committed.

## Invariants (every phase)
- Fresh clone builds via `python run.py`, **zero warnings** under `-Wall -Wextra -Wpedantic`
  (vendored deps exempt; the accepted Lua `tmpnam` linker warning is the only allowed one).
- `python runTestsAll.py` → Engine + Game ctest at **100%**. New pure logic gets ≥1 unit test
  and (where it round-trips) ≥1 property test (`NUM_OUTER_TESTS=10`, `NUM_INNER_TESTS=5`).
- SDL3 only. Y-flip stays confined to `RenderSystem::draw_entity()`. Every tunable lives in
  `assets/GameData.json`, never in code.
- Deterministic replay preserved: `game --seed S --clicks ... --screenshot N --stopframe N`
  → same seed = same result. Verify a mechanic manually with `python run.py` after each phase.
- TDD for new pure functions: test first. Leave changes in the working tree — no commits
  or tags; the user handles git.

## Already done (committed, do not redo)
- Phase 0: v2 bring-up. Phase 1: procedural neon asset pipeline (`assets/generator/v2/`).
- Phase 2: engine render upgrades (Tint, additive blend, flip). Phase 3: particle system.

---

## Phase 4 — Hit feedback & screen shake  *(finish the in-flight WIP)*
`CPP/game/feedback.hpp` and a `Flash` component are already started (uncommitted).
- Finish wiring: producers add **trauma** on player-hit / enemy-death / big impacts; a
  trauma scalar decays each frame (`feedback::decay_trauma`) and offsets the camera by
  `feedback::shake_amplitude(trauma)` in a random direction (seeded — keep replay deterministic).
- `Flash` component → additive `Tint` via `feedback::flash_tint`, faded over its lifetime,
  applied to the flashed entity's draw (player on hit, enemy on death).
- Tunables (`kMaxShakePx`, decay rate, flash duration/colour) → `GameData.json`.
- Tests: `feedback::` math is already pure — add unit tests (amplitude monotone, clamp,
  decay floors at 0, flash alpha at full/expiry) + a property test (shake_amplitude monotone
  in trauma; flash alpha ∈ [0,255] for arbitrary time_left/duration).
- Finish the modified `component_storage`/`destruction`/`player_components` + `feedback.hpp`.

## Phase 5 — Parallax backdrops
Currently a single `backdrop` in `GameData.json`.
- Extend backdrop config to N layers, each with a `depth`/`scroll_factor`. Render each layer
  offset by `camera_pos * (1 - scroll_factor)` (far layers move less). Reuse the neon backdrop
  generator to emit a couple of depth layers.
- Visual-only, no gameplay change. Test: a small pure `parallax_offset(camera, factor)`
  free function + unit test; verify visually via screenshot.

## Phase 6 — Multi-arena + obstacles + contact hazards
Currently one arena, no obstacles.
- Add three arena defs to `GameData.json` (Core / Foundry / Bio-lab): backdrop, obstacle
  layout, hazard layout, which wave-range activates them.
- **Obstacles**: solid static entities with collision (reuse quadtree + collision layers);
  projectiles and drone can't pass. **Hazards**: static entities carrying `ContactDamage`
  (already exists from v1) — no new damage code, just placement.
- Arena progression: `GameStateSystem`/wave logic swaps the active arena at configured waves.
- Tests: arena-select-by-wave logic (pure), obstacle-blocks-movement collision test.

## Phase 7 — A\* enemy pathfinding around obstacles  *(depends on Phase 6)*
Engine already ships `pathfinding.hpp` (A\*, from 090) — currently unused by the game
(enemies do straight-line `seek`).
- Wire it: enemies path around Phase-6 obstacles when line-of-sight to the player is blocked;
  fall back to cheap straight `seek` when LOS is clear (perf). Grid derived from arena obstacle
  layout. Keep it tunable (repath interval) in `GameData.json`.
- Tests: LOS check (pure), path avoids obstacle cells (property test on random obstacle grids),
  enemy still reaches player when path exists.

## Phase 8 — Audio port  *(independent — can run any time)*
Deferred since v1. Port from `070-option-audio-system`.
- Copy `audio_manager.{hpp,cpp}` into `CPP/engine/`, add the SDL3_mixer FetchContent block
  (VORBIS vendored, FLAC/MOD off, MP3 on) to `CPP/CMakeLists.txt`, link `SDL3_mixer`.
- Copy `test_audio_manager.cpp` (nullptr-mixer headless path keeps ctest silent/green).
- Game-side triggers (small system or direct calls) for fire / enemy-death / player-hit /
  level-up / game-over. Generate neon SFX via the `assets/generator/v2/` pipeline; filenames +
  volumes in `GameData.json`. Update `assets/CREDITS.md`.

## Phase 9 — Hardening & wrap
- Fresh-clone rehearsal: clone to scratch, `python run.py`, assert zero warnings, both ctest
  suites 100%, play title → game → win/lose → clean exit.
- Deterministic-replay screenshots for the demo. Update `README.md`, `assets/CREDITS.md`,
  `assets/asset_manifest.json` / `manifest.json`. Repo hygiene (no `.sh`/`.bat`, no strays).
  Leave everything in the working tree — the user handles git.
