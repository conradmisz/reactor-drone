# Architecture Context

`ENGINE.md` is the deep architecture doc (layer map, measured provenance, full
frame order, traps). This file is the summary + the invariants. **Any engine
change updates `ENGINE.md` in the same commit.**

## Stack

| Layer     | Technology                          | Role                                              |
| --------- | ----------------------------------- | ------------------------------------------------- |
| Language  | C++17 (`CMAKE_CXX_STANDARD 17`)     | Engine and game                                   |
| Build     | CMake `3.20...4.0`, FetchContent    | Hermetic deps, no system packages                 |
| Platform  | SDL3 + SDL3_ttf (no SDL2 shim)      | Window, render, input, text                       |
| Scripting | Lua 5.4 + `lua_bindings.cpp`        | `ScriptSystem`; the `ui.*` table is inert here     |
| Data      | nlohmann::json                      | `assets/GameData.json` — every tunable             |
| Tests     | Catch2 v3.5.2 (unit + property)     | 8 ctest targets, `^(Engine\|ResourceManager)` / `^Game` |
| Assets    | Python 3 + Pillow (offline)         | `assets/generator/v2/` — output committed          |
| Runners   | Python 3 (`run.py` and friends)     | Configure / build / test / run. No shell scripts.  |

## System Boundaries

- `CPP/engine/` — the reusable ECS engine: EntityManager, ComponentStorage,
  Blackboard, destruction, ResourceManager, Timer, pathfinding, and the systems
  under `ecs/systems/` (render, input, movement, collision, particles, camera,
  HUD, screenshot, script, and the UI/menu layer). Knows nothing about Reactor
  Drone.
- `CPP/game/` — Reactor Drone itself: `main.cpp` (the phase machine and the
  frame order), gameplay systems, game components, `arena_config` (the typed
  view of `GameData.json`), and the pure headers (`feedback`, `obstacles`,
  `parallax`, `enemy_path`, `item_system`, `shield_system`, `aim_math`).
- `assets/` — `GameData.json` (all tunables, `ui_styles`, `screens`), committed
  generated art/audio, and `generator/v2/` (offline pipeline, never runs at
  build time).
- `CPP/*/tests/{unit,property}/` — Catch2 suites. Engine tests are inherited
  and must keep passing untouched.

## Storage Model

- **`assets/GameData.json`** — the single source of tunables: window, camera,
  seed, player, enemy types, 20 waves, 4 arenas (obstacles/hazards/backdrop
  layers/tints), economy, shop catalogue, feedback, pathfinding, `ui_styles`,
  `screens`. Nothing gameplay-relevant is hardcoded in C++.
- **Blackboard** — cross-system runtime values keyed by string
  (`ship.extra_shots`, `ship.item_amount`, `player.iframes`, `mouse.screen_x/y`,
  `ui.escape_pressed`, `ui.cmd.push/pop`, `UISystem::UI_CLICK_KEY`). Use it for
  a value one system writes and another reads without a component to hang it on.
- **Components** — per-entity state, via explicit-instantiation
  `ComponentStorage`.
- **Disk** — nothing. There is no persistence yet; every run starts fresh.
  Part 5 adds `saves/slot_N.json` (run state only), written atomically.

## Core Entities

Components in play. Adding a **new** component type is expensive — see
Invariant 6.

| Entity/Component | Key fields | Notes |
| --- | --- | --- |
| `ShipState` (player) | `currency`, `keys`, `shield`/`shield_max`/`shield_regen`/`shield_delay`, `speed_mult`, `item_id`, `consumable_id`, `buff_id`/`buff_timer`, `upg_counts[8]` | One fat struct holding *all* per-run shop state (D17) |
| `Health` | `current`, `max_hp` | Player and enemies |
| `WeaponStats` | `fire_rate`, `damage`, range | Renamed from `TowerStats` in v1 |
| `Pickup` | `kind` (`PickupKind`), `value`, `magnet_speed` | The only component v2 gameplay added |
| `ContactDamage` | `damage`, `currency` | Enemies and arena hazards |
| `Flash` | colour, duration | Additive `Tint` on hit/death |
| `Tint` | rgba + `additive` | Colour/alpha mod at draw |
| `Particle` / `ParticleEmitter` | lifetime, lerp endpoints, `EmitterShape` | 2000 global budget |
| `RenderLayer` | `layer` | 0 bg … 2 enemies, 3 player, 4 pickups |
| `UIRect` / `UIElement` / `UIState` / `UIScreen` / `ScreenMembership` | rect, `element_type`, `style_id`, `on_click_fn`, `pulse_hz`, `active` | The data-authored menu/HUD layer |
| `EnemyTag` / `PlayerTag` / `ProjectileTag` / `SeekPlayer` / `Lifetime` / `Collider` | — | Inherited tags and the seek target |

Non-entity config types live in `arena_config.hpp`: `ArenaDef`, `WaveDef`,
`EnemyType`, `GameConfig`. (`wave_config.hpp` is dead — superseded by `WaveDef`.)

## Runtime Model

- **Phases** (`main.cpp`): `PHASE_TITLE 0`, `PHASE_PLAYING 1`, `PHASE_GAMEOVER 2`,
  `PHASE_VICTORY 3`, `PHASE_SHOP 4`, `PHASE_INTERMISSION 5`. The shop and the
  intermission are *phases*, not overlays (D25) — while they run, the arena is
  genuinely frozen.
- **Frame order is load-bearing and documented in `ENGINE.md` §3.** Do not
  reorder systems without reading it; several orderings are deliberate
  (shields before damage, loot before sprite load, flash last writer of `Tint`,
  UI composited over world *and* HUD).
- **Two coordinate spaces.** World space is bottom-left origin, flipped once in
  `RenderSystem::draw_entity()`. The UI layer is a separate 800×600 design
  canvas with its own single flip (`to_sdl_y()`), letterboxed onto the 980×660
  logical surface by `ui_canvas_transform` (scale 1.1, x-offset 50). They never
  mix.

## Environment and Setup

- No env vars, no secrets, no services. Everything is committed or fetched.
- Needs a C++17 toolchain, CMake ≥ 3.20, Python 3. SDL3, Catch2, Lua and
  nlohmann::json are fetched hermetically by CMake.
- `assets/generator/v2/` needs Pillow, which is **not** installed system-wide
  here. Generators are run by hand; their output is committed.
- Headless: `SDL_VIDEODRIVER=dummy` works and supplies a mouse; `offscreen`
  does not (a zero-score offscreen run is expected, not a bug).

## Invariants

1. **Zero warnings** from our code under `-Wall -Wextra -Wpedantic`. The only
   accepted one is Lua's vendored `tmpnam` linker warning.
2. **100% ctest**, both `^(Engine|ResourceManager)` and `^Game`. Engine tests
   inherited from the class are not modified.
3. **World-space Y-flip lives only in `RenderSystem::draw_entity()`**; the UI
   space's single flip is `to_sdl_y()` in `ui_render_math.hpp`. No third flip.
4. **Determinism.** Every RNG draw happens on every code path in a fixed order;
   a conditional may decide which draws are *used*, never how many are *taken*
   (D18/D19). Canary: two runs of the same `--seed` print an identical summary.
5. **Every tunable lives in `GameData.json`.** If a playtest would want to
   change it, it is data.
6. **`ComponentStorage` uses explicit instantiation.** A new component type
   needs a storage member, two `get_storage<>` specialisations, ~6 instantiation
   lines, a `destruction.cpp` sweep, and a `debug_adapters` registration — or it
   fails at link time or leaks onto recycled entity ids. Prefer a field on
   `ShipState` or a Blackboard key.
7. **CMake engine source lists are explicit in three places** (`game/CMakeLists.txt`
   and twice in `engine/tests/CMakeLists.txt`). A new engine `.cpp` must be added
   by hand.
8. **Anything meant to outlive a run must be skipped in `spawn_world()`** — it
   tears down every entity, and already skips `UIScreen`/`UIElement`.
9. **Generators never run at build time.**
10. **Enemy sprites are pure luminance**; their colour comes from
    `ArenaDef::enemy_tint` at spawn. Never bake an arena colour into enemy art.
11. **`SDL_INIT_AUDIO` is not initialised and no mixer is linked.** Any audio
    work must stay isolated enough to be removed in one revert.
