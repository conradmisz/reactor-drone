# ENGINE.md — Reactor Drone v2 engine architecture

The living architecture document for this repo's engine. It exists because two things are
otherwise undocumented and expensive to rediscover: **what is actually ours versus the
class baseline**, and **the order the frame runs in**. Both are load-bearing and neither
is derivable from a quick read of `main.cpp`.

**Maintenance rule:** any change under `CPP/engine/`, any new game system, or any change to
the `main.cpp` frame order updates this file *in the same commit*. Provenance is regenerated
by re-running the sweep in §2, never from memory.

---

## 1. Layers

```
                         [=] class original, untouched
                         [~] class original, modified by v2
                         [+] added by v2

  ┌──────────────────────────────────────────────────────────────────────┐
  │  DATA                                                                │
  │  assets/GameData.json  [=] loader / [+] game-side GameConfig         │
  │  assets/images/v2/*.png + *.json sidecars   [+]                     │
  │  assets/generator/v2/*.py — OFFLINE, never runs at build time [+]   │
  │  GameData.json "ui_styles" + "screens" — menus are DATA [+]         │
  └──────────────────────────────────────────────────────────────────────┘
                                     │
  ┌──────────────────────────────────────────────────────────────────────┐
  │  GAME SYSTEMS (CPP/game/)                                            │
  │  wave_spawner [~]   enemy_seek [~]   enemy_death [~]                │
  │  player_aim [=]     player_fire [~]  player_damage [~]              │
  │  projectile_hit [~] damage_apply [=] game_hud [~]                   │
  │  shop [+]  pickup [+]  item [+]  shield [+]  flash [+]              │
  │  pure headers: feedback [+] parallax [+] obstacles [+]              │
  │                enemy_path [+] aim_math [=]                          │
  └──────────────────────────────────────────────────────────────────────┘
                                     │
  ┌──────────────────────────────────────────────────────────────────────┐
  │  ENGINE SYSTEMS (CPP/engine/ecs/systems/)                            │
  │  render [~]  input [~]  player_control [~]  particle [+]            │
  │  movement [=] collision [=] (+ brute_force / quadtree / uniform_grid │
  │  strategies [=])  lifetime [=]  animation [=]  rotation [=]         │
  │  camera [=]  camera_control [=]  hud [=]  debug_hud [=]             │
  │  screenshot [=]  script [=]  wrap [=]                               │
  │  UI & MENU (ported from Option-040) [+]                             │
  │    screens: gameplay (HUD gauges) / wave_intermission / pause [+]    │
  │    ui [+]  ui_render [+]  screen_stack [+]  screen_fade [+]         │
  │    pure headers: ui_render_math [+] ui_focus_math [+]               │
  │                  ui_fade_math [+]  ui_style [+]                     │
  └──────────────────────────────────────────────────────────────────────┘
                                     │
  ┌──────────────────────────────────────────────────────────────────────┐
  │  ENGINE CORE (CPP/engine/)                                           │
  │  EntityManager [=]   ComponentStorage [~]   Blackboard [=]          │
  │  destruction [~]  Timer [=]  ResourceManager [=]  sidecar_loader [=]│
  │  gamedata_loader [=]  project_paths [~]  pathfinding [=] tile_map [=]│
  └──────────────────────────────────────────────────────────────────────┘
                                     │
  ┌──────────────────────────────────────────────────────────────────────┐
  │  SDL3 + SDL3_ttf  (SDL3-only; no SDL2 compat shim anywhere)          │
  └──────────────────────────────────────────────────────────────────────┘
```

---

## 2. Provenance — measured, not remembered

Baseline: `../110-finalproject-conradmisz/CPP/`. Regenerate with:

```bash
cd CPP
find . -path ./build -prune -o -type f \( -name '*.cpp' -o -name '*.hpp' \) -print |
while read -r f; do
  b="../../110-finalproject-conradmisz/CPP/${f#./}"
  if   [ ! -e "$b" ];    then echo "NEW      $f"
  elif cmp -s "$f" "$b"; then echo "IDENT    $f"
  else                        echo "MODIFIED $f"; fi
done | sort
```

Current measured state: **150 identical · 31 modified · 27 new** (208 source files).

### Engine — new in v2

| File | What it is |
|---|---|
| `ecs/systems/particle_system.{hpp,cpp}` | The whole particle simulation: emitters, per-particle lerp, `DEFAULT_MAX_PARTICLES` budget, and the `emit` flag that separates ageing from spawning (see §5) |
| `tests/unit/test_particle_system.cpp`, `tests/property/test_particle_properties.cpp` | Its tests |
| `tests/unit/test_tint.cpp` | `modulate_color` / Tint semantics |
| `ecs/systems/bloom_math.hpp`, `ecs/systems/bloom_system.{hpp,cpp}` | v3 Tier 1+2 render-target bloom: scene target + **emissive target** + halving linear-filtered downsample chain composited back additively. The chain reads the emissive target only (D208), so hulls/backdrop/HUD never bleed. Self-disables without render-target support (headless = old pipeline). `BloomConfig` lives here; parsed from GameData's optional `"bloom"` block (D207) |
| `tests/unit/test_bloom_math.cpp` | Chain geometry + intensity clamp |
| `ecs/systems/line_mesh_math.hpp` | v3 Tier 5 (D211): pure ribbon geometry — miter joins (width-preserving, hairpin-clamped), strip triangulation, arc-length UVs. v3 Tier 7 (D213) adds a **per-point-width** `build_ribbon` overload for tapered trails; the scalar form delegates to it, so pre-Tier-7 callers are unchanged |
| `tests/unit/test_line_mesh_math.cpp` | Its tests, including the taper overload and scalar/per-point equivalence |
| `ecs/systems/particle_mesh.hpp` | v3 Tier 9 (D215): pure quad geometry for the batched particle renderer — four corner verts per particle with full [0,1] UVs over the glow disc, and `quad_indices` winding two triangles each at a 4-vert stride. Takes ALREADY camera-transformed positions (unlike `line_mesh_math.hpp`, which takes world space): particles are read off entities the CameraSystem has already transformed, so only the Y-flip is left for the render system |
| `ecs/systems/trail_math.hpp` | v3 Tier 7 (D213): pure position-history math — `push_sample` (with the `min_spacing` guard that stops a stationary or hit-stopped entity packing the buffer with duplicates), `taper_widths` (v3 Tier 10 adds an `exponent`: > 1 concentrates width at the head so a shot reads as a tracer, default 1.0 keeps every earlier caller on the straight taper), `points_within_budget`. Points are stored oldest-first so they feed `build_ribbon` directly and its `u` doubles as head-ness |
| `tests/unit/test_trail_math.cpp` | Its tests (10 cases) |
| `ecs/systems/postfx_system.{hpp,cpp}` | v3 Tier 4 (D210): one SPIR-V fragment shader (offline-compiled `assets/shaders/postfx.frag.spv`) on a full-screen draw via `SDL_CreateGPURenderState` — aberration, vignette, grade, shockwave. GPU renderer only, which is OPT-IN (`--gpu-renderer`, see bugs/003); self-disables everywhere else. Destructor deliberately leaks the GPU state/shader (bugs/003 teardown wedge) |
| `ui_style.{hpp,cpp}` | `StyleTable`, `WidgetState`, `parse_ui_styles` — widget colours as pure data (Option-040 port) |
| `ui_focus_math.hpp`, `ui_fade_math.hpp` | Tab-order and fade-curve helpers (Option-040 port) |
| `ecs/systems/ui_render_math.hpp` | Widget-state precedence, the design→window canvas transform, inclusive hit-test, z-order sort — **plus the v2-only `pulse_alpha_scale` / `apply_alpha_scale` and `fit_text_in_rect` (D85)** |
| `ecs/systems/ui_system.{hpp,cpp}` | Hover/press/confirmed-click + Tab/Enter focus. **v2 addition:** publishes a confirmed click's `on_click_fn` to the Blackboard under `UISystem::UI_CLICK_KEY`, so a game with no Lua menu layer can consume its own buttons |
| `ecs/systems/ui_render_system.{hpp,cpp}` | Draws panels/labels/buttons/sliders/checkboxes. **v2 additions:** a render-local `elapsed_` clock driving `UIElement::pulse_hz`, and text fitting — every label and button caption goes through `fit_text_in_rect` so no string can draw outside its widget (D85) |
| `ecs/systems/screen_stack_system.{hpp,cpp}` | The single writer of the screen stack and of every `UIScreen::active` flag |
| `ecs/systems/screen_fade_system.{hpp,cpp}` | Fade-through-black on a screen transition |
| `tests/unit/test_ui_pulse.cpp` | The pulse helpers, incl. the exact-identity guarantee for `pulse_hz == 0` |
| `tests/{unit,property}/test_ui_*`, `test_screen_stack_*` | The Option-040 suite, ported unmodified except for two `-Wall -Wextra` fixes (see §5) |

### Engine — modified from the class original

| File | What it gained |
|---|---|
| `ecs/components.hpp` | `Tint` (rgba + `additive`), `Particle`, `ParticleEmitter` + `EmitterShape`, `RenderLayer`, `Rotation::flip_when_left`, and the UI layer's `UIRect` / `UIElement` / `UIState` / `UIScreen` / `ScreenMembership` (`UIElement::pulse_hz` is v2-only) |
| `ecs/component_storage.{hpp,cpp}` | Storage maps + `get_storage<>` specialisations for `Tint`, `Particle`, `ParticleEmitter`, the four UI components, and the v2 game components (`ShipState`, `Pickup`, `Flash`); `Experience` removed |
| `ecs/destruction.cpp` | Sweeps the same new component types on entity destruction, UI components included — without that, a destroyed widget's `UIElement` survives onto a recycled entity id |
| `ecs/systems/input_system.{hpp,cpp}` | Takes an `SDL_Renderer*` and runs `SDL_RenderCoordinatesFromWindow` so mouse coords are logical-space, not window-space; WASD aliases the arrow keys; publishes the per-frame UI edges (`mouse.down`/`mouse.up`, `ui.escape_pressed`/`tab`/`enter`) and the logical-space `mouse.screen_x/y`. **Escape is now an edge, not an immediate quit** — `main.cpp` decides what it means. Task 7: also publishes `ui.backspace_pressed` (an edge, same as Escape/Tab/Enter) and `ui.text_input` (a per-frame string, reset to `""` at the top of `process_events` and appended to from `SDL_EVENT_TEXT_INPUT`). Both are always reset/published — they cost nothing in a phase that never calls `SDL_StartTextInput`, since that's the only thing that makes SDL emit `SDL_EVENT_TEXT_INPUT` at all. `main.cpp`'s `PHASE_NAME_ENTRY` is the one reader |
| `gamedata_loader.cpp` | Parses the optional top-level `ui_styles` and `screens` blocks. Both are gated on the key being present, so a data file without them creates zero UI entities and raises no error |
| `lua_bindings.cpp` | The `ui.*` global table (`push_screen`, `pop_screen`, `set_label`, `get_value`, `set_disabled`, `widget_id`). Unused by this game — see §5 |
| `ecs/systems/player_control_system.{hpp,cpp}` | `set_speed()` (so a shop purchase applies mid-run) and diagonal normalisation |
| `ecs/systems/render_system.{hpp,cpp}` | Colour-mod / alpha-mod from `Tint`, additive blend mode, `RenderLayer` bucketing, rotation with `flip_when_left`, `render_layers()` + `TiledLayer` (tiled parallax backdrops, with `alpha` for the v2 Phase 5b arena crossfade), the v3 Tier 2 `render_emissive()` — the same walk drawing only `_glow` siblings + additive-tinted visuals into the bloom emissive target (D208) — and the v3 Tier 5 `render_glow_lines()` (D211): immediate-mode world-space neon ribbons via `SDL_RenderGeometry`, camera transform + Y-flip applied in this file (the one world-space flip site), drawn into scene AND emissive. v3 Tier 7 (D213/D214) adds `GlowLine::widths` (empty = uniform, every pre-Tier-7 caller) and `GlowLine::fade_tail`, which ramps per-vertex alpha with the arc-length `u` the ribbon already computes. Projectiles carry NO `Color` component, so this walk skips them and the ribbon is their only visual (D214). v3 Tier 9 (D215) adds `render_particles()`: every additive particle in ONE `SDL_RenderGeometry` call UV-mapped over `v2/glow_disc_64.png`, drawn into scene AND emissive. The walk itself now SKIPS additive-tinted `Color` entities — drawing them there is the SDL fill-rect path, i.e. the hard square that seeded the box halos (bugs/004). Note the disc's steep falloff: the quad is scaled by `DISC_SCALE` so the solid core lands on the particle's real footprint. v3 Tier 10 adds `GlowLine::core_scale` (default 0.35, the pre-Tier-10 constant) — shots ride 0.26 for a hotter nose, and the Tier 11 blast layers set `core = false` outright, because a white-lifted core is what the bloom chain smears into a flat grey ball |
| `project_paths.hpp` | Task 2b + D199: `assets_dir()` gained a `_WIN32`/`RD_PORTABLE` branch resolving relative to `SDL_GetBasePath()` instead of the baked `CLASS_ROOT_DIR`, and a new `user_data_dir()` — `class_root()` on Linux (unchanged on-disk saves), `SDL_GetPrefPath("conradm", "ReactorDrone")` on Windows and on any `RD_PORTABLE` release build. Dev behaviour is byte-identical. See §5 |

### Engine — byte-identical class originals (~150 files)

Everything else: the collision strategy family, Lua, `pathfinding.hpp`, `tile_map.hpp`,
camera, resource manager, timer, sidecar loader, and the entire inherited test suite.

### Game — new in v2

`enemy_path.hpp` · `feedback.hpp` · `flash_system.{hpp,cpp}` · `item_system.hpp` ·
`obstacles.hpp` · `parallax.hpp` · `pickup_system.{hpp,cpp}` · `shield_system.hpp` ·
`shop_system.{hpp,cpp}` — plus 10 new test files.

`explosion_fx.hpp` (v3 Tier 11, D216): pure staged geometry for the layered enemy
explosion — ring radius/alpha curves, circle points, shard spans and seeded shard
angles. The effect entity's own sprite clip is the clock (`current_frame /
(total_frames - 1)`), so the ring and shards need no component, no event and no
state; angles are seeded off the entity id, so a replay throws the same debris.

`debug_adapters.{hpp,cpp}` additionally register the four UI components, so a `J`/`T`
frame dump shows the live menu instead of an apparently empty screen.

### Game — unchanged class originals

`aim_math.hpp` · `damage_apply_system.*` · `debug_*` · `player_aim_system.*` ·
`script_loader.*` · `tower_components.hpp` · `tower_type_config.hpp` ·
`wave_config.hpp` (**dead** — superseded by `WaveDef` in `arena_config.hpp`)

---

## 3. Frame order

The single most load-bearing undocumented fact in the codebase. Sequence as written in
`main.cpp`; line numbers drift, the order does not.

```
input_system.process_events                 — every phase, incl. the renderer coord fix
(scripted keys injected here — the Escape handling below MUST follow them)
Escape edge -> push/pop "pause"             — depth<=1 opens pause, else pops the top
menu_paused = (top screen == "pause")       — NOT is_modal(): the intermission is modal
                                              too and must keep running
sim = (!debug_paused && !menu_paused) || step
screen_stack.process_commands               — consumes LAST frame's ui.cmd.push/pop, so
                                              UIScreen::active is settled before anything
                                              hit-tests or reads a click
ui_system.update                            — hover/press/confirmed click; publishes
                                              UI_CLICK_KEY, which the phase machine reads
[HOOK: prestige]                            — iteration 5 (D128); the arc-complete offer.
                                              ABOVE the phase machine on purpose: the click
                                              that presses PRESTIGE RUN is also a plain
                                              `advance`, which the victory branch reads as
                                              "retry", so both are consumed in one place

if PHASE_PLAYING && sim:
  player_control.set_speed(...)             — pushed every frame; nothing caches move speed
  player_control.update
  [HOOK: dash]                              — iteration 3 (D51); see §6
  player_aim.update                         — writes Rotation.angle
  wave_spawner.update
  [HOOK: boss]                              — iteration 3 (D51); see §6
  wave_just_cleared()  ── READ HERE ──      — a plain getter, reset at the top of the NEXT
                                              update(); shop_due and the arena shift both
                                              depend on reading it in the same frame
  arena shift tick                          — crossfade timer; props swap mid-fade
    [HOOK: arena-vfx]                       — iteration 3 (D51); see §6
  enemy_seek.update
  [HOOK: enemy-fire]                        — iteration 3 (D51); see §6
  [HOOK: specialty]                         — iteration 3 (D51); see §6
  movement.update
  arena circle clamp  (player + enemies)
  obstacle push-out   (player + enemies)    — solid walls get the last word on position
  update_equipment_visuals()                — [Phase 5c] thruster cone, kit parts, shield
                                              field, item aura. AFTER movement + clamp +
                                              push-out (they copy the player's settled
                                              Position — bug 002 was them reading the
                                              pre-movement one) and after player_aim
                                              (they read that angle). Also called in the
                                              intermission branch after its clamp
  items::repulse_enemies                    — runs after the push-out, so a shoved enemy is
                                              re-clamped next frame rather than through a wall
  items::use_consumable (on Q)
  [HOOK: actives]                           — iteration 3 (D51); see §6
  player_fire.update
  collision.update
  projectile_hit.update
  tick_shields                              — BEFORE damage, so a hit this frame restarts
                                              the quiet timer with the last word
  tick_buff
  player_damage.update
  damage_apply.update
  enemy_death.update                        — drop_loot() runs before any sprite/particle
                                              work, so the RNG stream never depends on
                                              whether a sidecar happened to load
  pickups.update
  [HOOK: sustain-spawn]                     — iteration 3 (D51); see §6
  lifetime.update                           — NOTE: does not run in PHASE_SHOP (see §5)
  animation.update
  tick_enemy_tint                           — tie-dye arena only; BEFORE flash_system so a
                                              hit flash still wins for its 0.12s
  flash_system.update                       — last writer of Tint
  destroy_marked_entities
  win/lose/shop decision

else if PHASE_INTERMISSION && sim:          — the between-waves prompt. The drone still
                                              FLIES here (see §5); only combat is held
  apply_move_speed, tick_hazard_glow
  player_control / player_aim / movement
  clamp_to_arena + push_out_of_solids       — player only; no enemies are alive
  pickups.update                            — the whole reason this phase is not frozen
  lifetime.update, flash_system.update
  read UI_CLICK_KEY (or the B key)          — "SHOP OPEN" -> PHASE_SHOP, "NEXT WAVE" ->
                                              PHASE_PLAYING; the key is consumed so it
                                              cannot re-fire on the following frame
  animation.update, destroy_marked_entities
else if PHASE_SHOP && sim:  shop.update, [HOOK: shop-menu], animation.update,
                            destroy_marked_entities
else if sim:                animation.update, destroy_marked_entities, title/restart input
                            — Task 7: PHASE_NAME_ENTRY (6) is handled in this same
                              `else if (sim)` bucket, as a sibling of the PHASE_TITLE
                              branch: reads ui.text_input/backspace/enter/escape,
                              polls a stored net::post_json future (never a discarded
                              temporary — see §5), rewrites the name_entry screen's
                              two dynamic labels by name (the menu_continue pattern)

(v3 Tier 3, D209: after end_frame + update_blackboard, a pending hit-stop
overrides the published delta_time to 0 for its remaining frames — systems
still run and draw/RNG counts are unchanged, but they integrate zero motion.
The camera block also writes camera.zoom = 1 + zoom_punch·trauma² each
simulated frame; legal because CameraControlSystem is not instantiated here.)

every phase, if sim:
  particles.update(emit = PLAYING || INTERMISSION) + destroy_marked_entities
                                            — ages everywhere so trails finish animating on
                                              title/game-over, but only SPAWNS while
                                              playing; lifetimes don't tick outside
                                              PHASE_PLAYING, so emitting there is unbounded
  hud_message_timer tick
  game_hud.update
  [HOOK: minimap]                           — iteration 3 (D51); see §6
  [HOOK: ship-readout]                      — iteration 5, Lane M (D113/D116/D117):
                                              PauseStatsSystem — the pause screen's stat
                                              sheet and the HUD's active-item slot. Outside
                                              the `sim` block on purpose: the pause screen
                                              freezes the sim and still has to be drawn.
  camera trauma decay + follow-camera lookat

render:
  camera.update
  bloom.begin()                             — v3 Tier 1: redirect into the scene target
                                              (no-op when bloom is off/unsupported)
  render_system.render_layers(bg_layers)    — outgoing arena at alpha 1, incoming fading in
  render_system.render
  render_system.render_glow_lines           — v3 Tier 5: arena ring, obstacle
                                              outlines, beam ribbons (list rebuilt
                                              each frame above the render block)
  render_system.render_particles            — v3 Tier 9: every additive particle
                                              as one batched mesh of soft discs
  hud_system.render
  ui_render_system.render                   — menus composite last, over world AND HUD
  (v3 Tier 4: when postfx is live, the whole composite above landed in its
  frame target — main sets it before bloom.begin(), and bloom's resolve
  restores the target captured at begin(). postfx.apply() then draws that
  target through the shader to the backbuffer. --screenshot forces the
  classic renderer, so captures never traverse this path.)
  if bloom.active():
    bloom.begin_emissive()                  — v3 Tier 2: switch to the emissive target
    render_system.render_emissive           — `_glow` siblings + additive Tints only
    render_system.render_glow_lines         — v3 Tier 5: lines bloom too
    render_system.render_particles          — v3 Tier 9: discs bloom too
  bloom.resolve()                           — back to the backbuffer: scene 1:1 + blur
                                              chain additive. The chain reads the
                                              EMISSIVE target, not the scene, so only
                                              authored glow bleeds; screenshot captures
                                              the bloomed frame
  screenshot_system.update
  render_system.present
```

---

## 4. Invariants and gates

- **Y-flip lives in exactly one place *for world space***: `RenderSystem::draw_entity()`.
  World space is bottom-left origin (0,0 bottom-left, +X right, +Y up). Nothing else flips
  world coordinates. The UI layer is a **second, separate coordinate space** with its own
  single flip, `to_sdl_y()` in `ui_render_math.hpp`, used by `UIRenderSystem` (drawing) and
  by `UISystem` via `to_ui_y()` (hit-testing). It applies no camera, zoom or lookat, so the
  two spaces never mix. Two spaces, one flip each — not a violation of the rule, but do not
  read the rule as "one flip in the whole codebase".
- **Text never leaves its widget.** `UIRenderSystem` measures every label and button
  caption against the widget rect through `fit_text_in_rect()` (`ui_render_math.hpp`) and
  shrinks it to fit; the fitted box is always inside the rect, and a collapsed (zero-size)
  rect draws nothing. Text that already fits is untouched, so this is not a layout engine —
  it is a floor. Author rects that fit at full size anyway; the fit is the guard, not the
  plan. Pinned by `CPP/game/tests/unit/test_ui_text_fit.cpp`, which needs no window.
- **SDL3 only.** No SDL2 compatibility shim.
- **Zero warnings** under `-Wall -Wextra -Wpedantic` (vendored deps exempt — Lua's `tmpnam`
  linker warning is expected and is not ours).
- **100% ctest**, `^(Engine|ResourceManager)` and `^Game`.
- **Determinism is a project invariant.** Every RNG draw happens on every code path in a
  fixed order; conditionals decide how many draws are *used*, never how many are *taken*.
  See `EnemyDeathSystem::drop_loot` and `WaveSpawnerSystem::spawn_enemy`. The canary is two
  runs of `./CPP/build/game/game --seed 42 --keys 5:SPACE --stopframe 3000` printing an
  identical summary line.
- **`DEFAULT_MAX_PARTICLES` is 4000 (was 2000) and truncates silently.** Measure before adding an
  emitter. See §5 — the live budget is not as free as it looks.
- **Generators are offline.** `assets/generator/v2/*.py` are run by hand and their output is
  committed. They must never run at build time. They need Pillow, which is not installed
  system-wide here.
- **Enemy sprites are pure luminance.** Their colour comes from `ArenaDef::enemy_tint` at
  spawn (`Color` records it, `Tint` applies it). Never bake an arena colour into enemy art —
  that is the bug Phase 5a fixed.

---

## 5. Known discrepancies and traps

- **`assets_dir()`/`user_data_dir()` branch on `_WIN32` OR `RD_PORTABLE`; a plain dev
  build is untouched on purpose.** `CLASS_ROOT_DIR` is an absolute build-machine path baked in at compile time —
  fine for the class dev workflow (run from any CWD), fatal for a Windows binary handed to
  someone else's PC. On `_WIN32`, `assets_dir()` resolves `<exe dir>/assets` via
  `SDL_GetBasePath()` (the flat install layout `installer/package-win.sh` stages — exe, DLLs
  and `assets/` as siblings) and `user_data_dir()` resolves the per-user prefpath via
  `SDL_GetPrefPath("conradm", "ReactorDrone")`, never under `{app}` (Program Files is not
  user-writable). Neither path is ever printed — the replay canary compares stdout, and a
  resolved filesystem path is exactly the kind of per-run-varying string that would break it.
  The four save call sites (`meta_save.cpp`, `run_save.cpp` x2, `settings_save.hpp`) moved
  from `class_root()` to `user_data_dir()`; `class_root()` itself is untouched and the engine
  test harness (`test_resource_manager.cpp`, `test_sidecar_loader.cpp`, `test_script_system.cpp`,
  `test_lua_manager.cpp` and two property-test files) keeps using the raw `CLASS_ROOT_DIR`
  macro directly to find `CPP/engine/tests/test_assets` — untouched, native-only.
  **D199 (2026-08-13) widened both branches from `_WIN32` to
  `defined(_WIN32) || defined(RD_PORTABLE)`.** `RD_PORTABLE` is a CMake option, OFF by
  default, that release packaging turns ON — so a Linux or macOS *release* build resolves
  assets beside the exe and saves via `SDL_GetPrefPath` exactly as Windows always has, while
  every dev build, `run.py`, the saves/ workflow and the replay canary keep the
  `CLASS_ROOT_DIR` behaviour byte-for-byte. This was the only real blocker to shipping on
  Linux/macOS: without it a non-Windows binary looks for the *build machine's* source tree on
  the player's computer and fails instantly. Verified by running a staged build from an alien
  cwd under a scratch `HOME` (`scripts/verify_branch.sh` section 6 pins it).
- **A baked sprite halo is a BOX unless its tail is cut.** `add_halo` blurs the
  silhouette with a Gaussian whose radius is `frame * spread` (0.18 -> ~92px on
  the 512 working canvas). That tail reached the frame edge, where it was CUT —
  a soft glow with a hard rectangular boundary, plus a flat alpha ~8 wash over
  the entire frame. At gameplay size every entity therefore sat in a faint BOX,
  which is most of what "everything radiates a square" meant. v3 Tier 12 remaps
  the halo's alpha (`floor = 30`, gradient preserved) so it reaches true zero
  inside the frame. **Regenerating art after touching `add_halo` means
  `make_sprites.py` AND `make_backdrops.py --props-only`** — props have their own
  generator and were missed on the first pass.
- **The bloom chain's kernel was a BOX too.** A half-size linear blit is a 2x2
  box average, and a chain of them spreads light into an axis-aligned box. Tier
  12 makes each step (from level 1 down) a four-tap Kawase downsample. Level 0
  keeps its single blit deliberately: it is the largest target in the chain, and
  paying 4x there cost ~10% of frame time for no visible gain.
- **The renderer's DRAW blend mode is global state, and until v3 Tier 9 only
  particles ever set it.** `SDL_SetRenderDrawBlendMode` is renderer-wide, and
  SDL's default is `SDL_BLENDMODE_NONE`, under which an alpha-0 fill writes
  SOLID BLACK rather than nothing. `UIRenderSystem` fills every panel/button
  background with `SDL_RenderFillRect` and never set the mode — it worked only
  because `draw_entity`'s additive-colour path (i.e. every additive PARTICLE)
  set `BLENDMODE_BLEND` on its way past each frame. Tier 9 moved particles to a
  batched `SDL_RenderGeometry` pass, that incidental setter disappeared, and the
  dash button's "rim, no fill" frame immediately painted an opaque black square
  over its own icon. `UIRenderSystem::render` now sets `BLENDMODE_BLEND` itself
  at entry. **Any new system that draws filled rects must set the draw blend
  mode it needs — never inherit it.**
- **A death still played the CLASS-ORIGINAL explosion until v3 Tier 11.**
  `EnemyDeathSystem::effect_sprite()` loaded `assets/images/effect_explosion.json`
  — a placeholder of a grey sphere growing into a rounded SQUARE — straight
  through the v2 art overhaul and every v3 tier. The v2 replacement
  (`assets/images/v2/effect_explosion.json`) existed the whole time and nothing
  referenced it. **When judging how something looks, confirm which asset is
  actually loaded before touching the art**; `v2/` art is not automatically what
  runs.
- **`--dump` and `--trace` are parsed but never consumed.** `CliOptions::dump_frames` /
  `trace_frames` are populated by `cli_parser.cpp` and `main.cpp` never reads them. There is
  no state dump. `--screenshot` *does* work.
- **The particle budget is now 4000 (D84), raised from 2000 at the iteration-3
  integration.** The old cap was exhausted in two distinct ways even before iteration 3.
  Measured over a full 20-wave headless run (seed 42, sampled every 120 frames):

  | Situation | Live particles |
  |---|---|
  | Ordinary combat, any wave | ~13-300 |
  | Mass death, early waves (~12 enemies) | 300-500 |
  | Mass death, wave 20 (~96 enemies) | **2000 — capped, silently truncating** |
  | Any frame with the shop open after a mass kill | ~1900-2000 indefinitely — **fixed**, now 0 |

  Iteration 3 then added four new consumers. Measured per lane, against the old 2000 cap:

  | Situation | Live particles | Lane |
  |---|---|---|
  | Full-arena destruction during a shift | 336 alone / **463** overlapping the shockwave | E |
  | Boss fight, adds + spitter patches, no actives | **301** | D |
  | Actives firing on cooldown, waves 1-5, no boss | **991** | D |
  | Boss wave + actives, sustained | **1998 — exactly at the old cap** | D |

  Worst single consumers: missiles ~130 live (8 x 55/s x 0.3s), laser ~110 (4 x 90/s x
  0.3s), boss aura ~72, mine blast ~91 one-shot. Lane B's dash and minimap add **zero** new
  emitters (the dash drives the existing thruster, ~36 extra live particles).

  4000 is headroom over the worst measured case, **not a measured ceiling** — the frame-rate
  cost has not been verified in a real window. If the wave-50 fight drops frames, cut
  per-effect emission rates (they are data) rather than raising this again.

  The shop case was a leak, fixed in Phase 5: `lifetime.update` runs only in
  `PHASE_PLAYING`, but `particles.update` runs in every simulated phase, so the one-shot FX
  hosts (death bursts, pickup pops, the arena shockwave) that carry a `Lifetime` never
  expired while the shop was open and kept emitting. `ParticleSystem::update` now takes an
  `emit` flag, and `main.cpp` passes `phase == PHASE_PLAYING` — particles still age and
  retire in every phase (so trails finish animating on the title/game-over screens), but
  nothing new spawns outside play. Running `lifetime.update` in the shop branch would have
  been the smaller diff and the wrong fix: it would also expire the player's uncollected
  loot while they shop.

  The wave-20 case is **not** a leak and is left alone: it is simply more simultaneous
  bursts than the budget holds, and it only occurs via the 30s stall force-kill wiping a
  whole wave in one frame. Truncation there is graceful — a slightly thinner explosion.
  Fixing it would mean a bigger global cap or capping death bursts; neither is worth it for
  a degenerate path.

  **Consequence for new emitters:** measure at the moment yours actually fires, not on
  average. The Phase 5b arena-shift shockwave is safe precisely because a shift only fires
  on a *cleared* wave, where the live count is ~13.
- **`make_backdrops.py` used to be non-reproducible.** It seeded from `hash(pal.name)`,
  which Python randomises per process. Now `zlib.crc32`. The committed core/foundry/biolab
  PNGs predate the fix, so the first full regeneration will change them once.
- **`wave_config.hpp` is dead code**, superseded by `WaveDef` in `arena_config.hpp`.
- **The blackboard `"wave"` is stale on the cleared-wave frame.** The spawner publishes it
  before it may increment `current_wave_`, so code running on `wave_just_cleared()` must use
  `current_wave_index() + 1`.
- **There is no audio.** Eight `.wav` files are committed under `assets/Audio/` and nothing
  plays them. No mixer, no `SDL_INIT_AUDIO`. Phase 7.
- **Persistence is two small files, and neither one touches the simulation.**
  `saves/meta.json` is one number, lifetime score (Lane F, D80). `saves/run.json` is the
  mid-run save/quit: run *state* only — wave, credits, score, hull/shield, gear, items,
  ship, difficulty, seed — with no entity snapshot, no `SerializationRegistry` and no
  `LoadSystem` (Lane K, D100). Both are read **once at startup** and applied only on paths
  a fresh run never takes, which is what keeps the replay canary byte-identical whether or
  not a save file exists (D83, D103). Anything that later reads a save from inside a system
  breaks that guarantee.
- **The one exception is the prestige level** (`meta.json`'s second field, iteration 5,
  D127). It is still read once at startup and still applied only at `start_run`, but it
  *scales `config.player`*, so it genuinely changes the simulation. The replay canary is
  therefore reproducible **at a fixed prestige level**, not unconditionally — `start_run`
  prints `Prestige: N` so a headless run states the level it flew at. Compare two runs at
  the same level; a level change is expected to diverge.
- **`PHASE_INTERMISSION` deliberately does NOT freeze the arena.** It was frozen at first,
  copying `PHASE_SHOP`, and that stranded every credit the wave's last kill had dropped:
  visible on the floor, unreachable, gone when the run moved on. It now runs the movement
  half of a frame (control, aim, movement, clamps, pickups, lifetimes, flash) and none of
  the combat half. A cleared wave has no live enemies, so there is nothing to fight anyway.
- **Pause freezes the frame COUNTER, not just the sim.** `timer.end_frame_no_advance()`
  runs whenever `!sim`, which is the pre-existing F1 debug-pause behaviour. Consequence for
  headless tests: a script that pauses and never resumes will never reach `--stopframe` and
  will hang. Worse, scripted `--keys` are frame-indexed, so a *resume* key scheduled after
  the pause frame can never fire. Pause is testable headlessly; resume is not.
- **Hazards cost ~120 permanent particles.** Each hazard carries a 12/s ember emitter with
  a 1.1s lifetime, and the worst arena (Bio-lab) has 9. Against the 2000 global budget that
  is ~6% held for the whole run, on top of combat. Factored into the §5 numbers above.
- **Window resize needs no plumbing, and must not grow any.** `SDL_LOGICAL_PRESENTATION_
  LETTERBOX` fixes the logical surface at 980x660, and the loader is the only writer of
  `window_width`/`window_height`. `UIRenderSystem` uses its constructor copies and
  `UISystem` reads the Blackboard — both see the same numbers, so the drawn rect and the
  clickable rect are guaranteed identical at any window size. Writing a live window size
  into those keys on a resize event would break exactly that guarantee.
- **The `ui.*` Lua bindings are dead code in this game.** They are ported and tested, but
  `main.cpp` instantiates a `LuaManager` purely to satisfy `UISystem`'s constructor and
  keeps no menu scripts. Button clicks are read off the Blackboard via
  `UISystem::UI_CLICK_KEY` instead. An empty Lua state resolves every `on_click_fn` to nil,
  which `UISystem` treats as "no callback", so the Lua path is inert rather than broken.
  Adding a `menu_callbacks.lua` later needs no engine change.
- **UI screens are authored in the 800x600 design canvas**, not in window pixels.
  `ui_canvas_transform` scales and centres that canvas onto the live 980x660 logical
  surface (scale 1.1, x-offset 50). Both `UIRenderSystem` and `UISystem` apply it, so the
  drawn rect and the clickable rect can never drift apart.
- **The HUD text rows are authored in the design canvas too (D87).** `GameHUDSystem`'s
  `Text`+`ScreenPosition` rows are drawn by `HUDSystem` in *window* pixels, not through
  `ui_canvas_transform`, while the hull/shield gauges beside them are widgets that *are*
  transformed. Authored in window pixels, the two halves of one HUD sat in different
  columns (canvas x=16 lands at window x=67.6, not 16). `GameHUDSystem::init` now applies
  the transform itself, so both halves share one coordinate system. Anything new added to
  that HUD must do the same.
- **`"gameplay"` is the screen stack's base sentinel, so it is ALWAYS active (D86).** It is
  never modal and is never popped, which means every widget on the `gameplay` screen renders
  in every phase — including the title screen and underneath the shop panel. Visibility is
  therefore not a stack question: `hud_visible_in_phase()` in `game_hud_system.hpp` is the
  one rule, and `GameHUDSystem` / `MinimapSystem` / `PauseStatsSystem` collapse their rects
  when it is false.
- **`z_order` is sorted GLOBALLY, across every active screen at once** (`sort_widgets_by_draw
  _order`), not per screen. Combined with the point above — `gameplay` is always active — a
  modal's widgets must outrank the HUD's or the hull gauge is drawn on top of the modal
  panel. That is what happened to the widened pause screen (D113), whose widgets are now at
  z 30/40 against the HUD's 10-21.
  Note this is a *different* question from whether the sim runs (see the pause/intermission
  trap): the intermission is modal and both keeps simulating and keeps its HUD.
- **`UIElement::pulse_hz` is render-only and deliberately not Blackboard-driven.**
  `UIRenderSystem` accumulates its own `elapsed_` from `delta_time`. Putting that clock on
  the Blackboard would let a game system observe it, and a replay could then diverge on
  presentation state. `pulse_hz == 0` returns an exactly-`1.0f` multiplier, so every
  non-pulsing widget renders byte-identically.
- **Two Option-040 test files needed `-Wall -Wextra` fixes on import**, since Class-040 did
  not enforce the zero-warning gate: a tautological `uint8_t <= 255` range check in
  `test_ui_render_properties.cpp` (now a `static_assert` on the channel type plus a
  meaningful sentinel check) and a `-Wdangling-reference` from chaining `.value().get()`
  off a returned temporary optional in `test_ui_interaction_properties.cpp`.
- **Running the game picks up real desktop mouse input.** A windowed run on the live display
  will absorb genuine clicks and silently corrupt a scripted run.
  `SDL_VIDEODRIVER=offscreen` isolates it but supplies no mouse, so the drone never aims and
  never lands a shot — a zero-score offscreen run is expected, not a bug.

---

## 6. Iteration-3 lane hooks (D51)

Iteration 3 is built by several agents at once, so every shared-file edit was made
**once, up front**, and each feature lane owns exactly one comment-delimited block
in `main.cpp`:

```
// === HOOK: <name> ===
...the owning lane's calls go here, and nothing else's...
// === END HOOK: <name> ===
```

| Hook | Frame-order slot | Owner |
|---|---|---|
| `dash` | after `player_control.update`, before `player_aim` | thruster dash (#5) |
| `boss` | straight after `wave_spawner.update` | boss every 10 waves (#4) |
| `arena-vfx` | inside the arena shift tick | blockade destroy/arrive animation (#2) |
| `enemy-fire` | after `enemy_seek.update`, before `movement` | enemy projectiles / moon types (#3) |
| `specialty` | after `enemy-fire` | per-arena specialty units (#9) |
| `actives` | after `items::use_consumable` | boss-reward actives |
| `sustain-spawn` | after `pickups.update` | health/shield pickups (#10) |
| `shop-menu` | in the `PHASE_SHOP` block | clickable shop + gear upgrades (#1, #11) |
| `minimap` | after `game_hud.update`, every phase | minimap (#7) |
| `prestige` | after `ui_system.update`, above the phase machine | 30-wave arc + prestige (#14, iteration 5) |

Iteration 5 kept the same convention. `prestige` is **two** blocks by necessity:
the frame-order one above, and one inside `start_run` — the single site where
`apply_ship` / `apply_difficulty` already overlay the pristine `base_config`
(D50). Applying a base-stat buff anywhere else would compound across runs.

`test_scaffolding.cpp` reads `main.cpp` as text and fails if a hook is missing or
renamed, because a lane whose hook has vanished has nowhere to land.

**What the scaffolding added, and why it is where it is:**

- **`EnemyShot`** (tag) and **`EnemyBehavior`** (`kind`/`tier`/`timer`/`cooldown`/`aim`)
  in `game/enemy_components.hpp`, registered in `component_storage.{hpp,cpp}` and swept in
  `destruction.cpp`. One behaviour struct covers the moon shooters, all four
  specialty units and the boss — `behavior_kinds` is the enum, and a `kind` is a
  code constant mapped from a string, never a JSON row index (the D26 rule).
- **`layers::ENEMY_SHOT` (0x20)**, with `PLAYER_MASK` and `OBSTACLE_MASK` widened
  to accept it. Enemy projectiles therefore need **no damage system of their own**:
  they carry `ContactDamage`, and `PlayerDamageSystem` already hurts the drone for
  anything carrying it. A separate bit rather than reusing `PROJECTILE`, whose mask
  is exactly what stops player shots hitting the player.
- **`ShipState`** gained `dash_cd`, `dash_timer`, `active_id`, `active_cd`,
  `gear_levels[8]`; **`PickupKind`** gained `Health` and `Shield`. Both avoid a new
  component type, which is the expensive kind of edit here.
- **`GameConfig`** gained `SustainConfig`, `DashConfig`, `MinimapConfig`,
  `BossConfig` and `std::vector<ActiveItemDef>`, plus `WaveDef::boss`,
  `ArenaDef::specialty_unit/_tier` and the `EnemyType` behaviour fields — all
  parsed in `arena_config.cpp` in the same phase.

Every default is **inert** (`sustain.interval 0`, `minimap.enabled false`,
`actives []`, no wave flagged `boss`, no type carrying a `behavior`), which is why
the phase could be verified by the replay canary staying byte-identical.

### 6a. Lane B — what landed in `sustain-spawn`, `dash` and `minimap` (D56-D58)

- **`sustain_spawn_system.{hpp,cpp}`** — a free function, not a class (the
  `tick_shields` idiom). Its whole state is three Blackboard keys
  (`sustain.timer/count/wave`), so there is nothing to construct in `main.cpp` and
  nothing that can survive a restart holding the wrong value. **No RNG**: the n-th
  placement walks a golden-angle spiral, so the R2 "draw on every path, in a fixed
  order" discipline is not merely followed, it is unnecessary. Health/Shield
  pickups ride the existing `Pickup` component and are collected by
  `PickupSystem`'s two new kind branches, which clamp into `Health.max_hp` and
  `ShipState.shield_max` respectively.
- **`dash_system.hpp`** — header-only free function, same idiom. `ShipState`
  already carries `dash_cd`/`dash_timer`; the only scratch that does not belong on
  a component (the per-dash already-hit list) lives in a `DashState` the hook
  block owns. LSHIFT is read **inside the hook block**, not in main's shared
  key-edge section, so the whole feature is one contiguous diff. `--keys 20:LSHIFT`
  drives it headlessly.
- **`minimap_math.hpp` + `minimap_system.{hpp,cpp}`** — the mapping is a pure,
  engine-free header; the system is a thin shell over a pooled set of blip
  widgets.

  ⚠️ **Correction to the Phase-0 note in the `minimap` hook**: a screen-space
  entity carrying `ScreenPosition + Size + Color` and **no** `Position` is drawn by
  **nothing**. `RenderSystem::render` iterates
  `entities_with_component<Position>()`, and `HUDSystem` only draws `Text`. Adding
  a `Position` hands the entity to `CameraSystem`, which overwrites
  `ScreenPosition` every frame. The HUD *text* rows work only because `HUDSystem`
  has its own `Text`+`ScreenPosition` path. The blips are therefore pooled
  **`UIElement` panel widgets** on the always-active `gameplay` screen — the
  mechanism `GameHUDSystem`'s hull/shield gauges already use. That also buys
  design-canvas coordinates (resolution independence), `z_order` above the frame
  panel, style-driven colours, and survival across `spawn_world`, which
  deliberately skips `UIElement` entities — so the pool is allocated exactly once
  per process and only ever repositioned. A parked blip is a zero-width rect.
