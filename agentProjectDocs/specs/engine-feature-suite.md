# Engine Feature Suite — umbrella spec (branch: `feature/engine-suite`)

Eleven engine-level additions chosen to exploit what this engine uniquely has:
hard determinism, everything-as-data, an inert Lua layer, and the long-term
bespoke-console vision (microcontroller-ish/RTOS target, worst case Pi-class,
cartridge distribution to the owner and friends). This document is the **index**
for the suite: what each feature is, what it adds to the engine, what it must
not touch, and the order it gets built in. Each lane still writes its own spec
from `specs/feature-template.md` before code.

Everything here lives on `feature/engine-suite` only. `master` is the stable
game until the suite (or slices of it) earns a merge.

Decision-id reservations: Phase 0 = **D138**; Wave 1 = **D139–D150**;
Wave 2 = **D151–D165**; Wave 3 = **D166–D180**. Unused ids are burned, as usual.

---

## Ground rules for every feature

- **Inert by default.** Each feature ships behind a `GameData.json` block whose
  default leaves the sim byte-identical — the replay canary
  (`--seed 42 --keys 5:SPACE --stopframe 3000`, twice) must not move at Phase 0
  or at any lane merge with the feature data-disabled. This is the iteration-3
  Phase-0 playbook (`ENGINE.md` §6).
- **R2 draw discipline** for anything sim-side: RNG draws happen on every path
  in fixed order; conditionals choose what is *used*, never how many are *taken*.
- **No new component types unless unavoidable** (Invariant 6). Prefer
  `ShipState` fields, Blackboard keys, `GameConfig` sub-structs, and
  system-owned state.
- **MCU headroom:** fixed-size buffers, no allocation in the hot path, budgets
  as data, and math that survives a fixed-point port (avoid transcendental
  functions per-entity per-frame where a table will do).
- **ENGINE.md updated in the same commit** as any engine change.

---

## The features

### RG. Resonance Grid — the arena is a physics display *(Wave 1, Lane R)*

**Fantasy:** every explosion, dash, death and boss slam sends visible ripples
through the reactor's containment lattice. The arena floor is alive with the
combat — the Geometry Wars trick, reactor-themed. This is the suite's signature
image.

- **Engine addition:** `ecs/systems/resonance_grid_system.{hpp,cpp}` — a
  spring-mass lattice (fixed N×M, ~40×28 at 40 px pitch) updated with damped
  Verlet integration; impulses injected from combat events; drawn as lines/points
  under `RenderLayer` 0, above the parallax backdrops.
- **Touch points:** one render call in `main.cpp` (hook `grid-render`, between
  `render_layers` and `render`); event injection read off the Blackboard
  (`grid.impulse` entries published where deaths/dashes/blasts already happen) —
  publishers are 1-line additions at existing event sites.
- **Determinism stance:** **render-only.** The grid reads sim events and writes
  pixels; no sim system may ever read grid state. Its own PRNG (if any) is
  engine-seeded and separate from the sim stream.
- **Inert default:** `"grid": {"enabled": false}` → system constructs, updates
  nothing, draws nothing.
- **MCU note:** fixed lattice in a flat array, integer/fixed-point springs, one
  pass per frame, zero allocations. Budget: full update < 0.5 ms Pi-class.
- **Dependencies:** none. Pairs visually with Battle Scars (#6).

### 1. Temporal Overload — bullet-time *(Wave 1, Lane P)*

**Fantasy:** the killing blow on a hulk, the last sliver of hull, the dash
through a pack — time dilates for a beat and every hit lands harder.

- **Engine addition:** a `timescale` multiplier applied where `delta_time` is
  produced (Timer or the `main.cpp` frame top), plus
  `game/timescale_system.hpp` (header-only free function, the `tick_shields`
  idiom) deciding the target scale from sim events (kill-chain, dash impact,
  hull-critical) and easing toward it.
- **Touch points:** hook `timescale` at the top of the frame; the events it
  reads already exist on the Blackboard or are 1-line publishes.
- **Determinism stance:** **sim-side.** Timescale is a pure function of sim
  state, so replays stay identical — scripted `--keys` are frame-indexed and
  frames still advance one per loop; only `dt` shrinks. **Trap to verify in the
  lane spec:** anything that counts *frames* rather than seconds (wave timers,
  `--stopframe`) is unaffected by design; anything that mixes the two must be
  audited.
- **Inert default:** `"timescale": {"enabled": false}` → multiplier is exactly
  1.0f (the `pulse_hz == 0` exact-identity idiom).
- **MCU note:** one multiply. Free.
- **Dependencies:** none. Amplifies everything else.

### 2. Bullet-Pattern Language — the danmaku engine *(Wave 3, Lane Y)*

**Fantasy:** bosses and elite enemies fire authored bullet-hell patterns —
rings, spirals, aimed fans, pulsing waves — and new patterns are data, not C++.
The "full engine control" flex: a tiny interpreter that turns JSON (or Lua)
into choreography.

- **Engine addition:** `game/bullet_pattern.{hpp,cpp}` — a pattern definition
  struct (emitter op-list: ring/fan/spiral/aimed, counts, angular velocities,
  timing) parsed from `GameData.json`, plus an interpreter ticked from the
  existing `enemy-fire` hook that spawns `EnemyShot` entities through the
  existing spawn path. Optionally, a Lua binding so a pattern can be a script —
  the first real consumer of the inert Lua layer.
- **Touch points:** `EnemyBehavior` gains a `pattern` id (existing struct,
  no new component); `arena_config.cpp` parses a `"patterns"` array; the
  `enemy-fire` hook dispatches to the interpreter when a behavior names a
  pattern.
- **Determinism stance:** **sim-side.** Patterns are deterministic by
  construction (no RNG in the interpreter — variation is authored). If a
  pattern op wants randomness, it draws from the sim stream under R2.
- **Inert default:** `"patterns": []` and no behavior referencing one.
- **MCU note:** interpreter is a switch over a fixed op array; shot counts are
  bounded by data. The projectile *count* budget is the real constraint —
  pattern authoring must respect the collision/particle budgets.
- **Dependencies:** none hard; Temporal Overload (#1) makes dodging patterns
  feel incredible.

### 3. Force-Field Layer — attractors, repulsors, impulses *(Wave 2, Lane T)*

**Fantasy:** a black-hole active that drags a wave into one killable clump;
gravity-well hazards that bend your flight line; kills that shove neighbors
back. Mass and force enter the game's vocabulary.

- **Engine addition:** `game/force_field_system.{hpp,cpp}` — a fixed-capacity
  array of field sources (position, radius, strength, falloff, lifetime) plus
  one accumulation pass writing velocity deltas; the existing
  `items::repulse_enemies` becomes its first client (folded in, not
  duplicated).
- **Touch points:** one call in the frame **before `movement.update`** (hook
  `forces`); sources registered by items/hazards/actives via a free function.
  The arena clamp and obstacle push-out already run after movement and stay
  the last word on position — fields can never push through a wall.
- **Determinism stance:** **sim-side**, pure math over sim state, no RNG.
- **Inert default:** zero registered sources → the pass iterates nothing.
- **MCU note:** capped source count (data), linear falloff option, no sqrt in
  the common case (compare squared distances; use one rsqrt where needed).
- **Dependencies:** none. Consumed by Surge Events (#7); actives/gear get new
  design space.

### 4. Chip-Synth Audio — the sound of the platform *(Wave 3, Lane Z — LAST)*

**Fantasy:** the game *sounds* like the console it dreams of running on. No
samples: every SFX and the music are synthesized live — square/saw/noise
voices, tracker-style sequences — and the music's intensity follows combat.
A few KB of note data instead of MBs of WAVs.

- **Engine addition:** `ecs/systems/chip_synth_system.{hpp,cpp}` — ONE engine
  file pair (Invariant 11): SDL3 audio stream callback, a fixed voice pool
  (~8 voices: pulse ×3, saw ×2, triangle, noise ×2), an SFX table and a
  pattern/tracker sequencer, all parsed from one `"audio"` block in
  `GameData.json`. One CMake block. Trigger sites are a single list of
  Blackboard-event reads inside the system — game code publishes nothing new.
- **Touch points:** `SDL_INIT_AUDIO` + system construction + one `update()`
  call (hook `audio`). That is the entire diff outside the pair — a single
  revert removes the feature (project law).
- **Determinism stance:** **isolated.** Audio reads sim events, writes sound,
  and is never read by anything. The canary is untouched by definition.
- **Inert default:** `"audio": {"enabled": false}` → no device opened.
- **MCU note:** this is the most MCU-native feature in the suite — chip synths
  were born there. Fixed-point oscillators, integer mixing, no allocations in
  the callback.
- **Dependencies:** none hard. The Adaptive Director's (#8) stress value is the
  natural music-intensity input; Temporal Overload (#1) can pitch-bend the mix
  during slow-mo (render-of-sound, still isolated).

### 5. Palette Engine — the Downwell trick *(Wave 2, Lane W)*

**Fantasy:** the whole frame runs through an indexed palette. Arena shifts
become world-wide palette swaps, taking damage flashes the *world*, and new
palettes are unlockables — a collectible system for free, and instant visual
identity per cartridge.

- **Engine addition:** `ecs/systems/palette_system.{hpp,cpp}` — a post-pass
  over the composed frame: render the world to an `SDL_Texture` target, apply
  a palette LUT, present. Palettes are rows in `GameData.json`
  (`"palettes": [...]`), selected by arena/state/unlock.
- **Touch points:** `main.cpp` render block only — redirect the render target
  at frame start, resolve through the LUT before `present` (hook `palette`).
  No game system knows it exists.
- **Determinism stance:** **render-only.** Palette choice may *read* sim state
  (current arena, hull-critical); nothing reads back.
- **Inert default:** `"palettes": {"enabled": false}` → direct-to-backbuffer
  path unchanged, byte-identical frames.
- **Feasibility gate (spec must resolve first):** SDL3 has no shader hook in
  the 2D renderer — the LUT pass is either `SDL_RenderGeometry` tricks, a
  CPU pass over a small logical surface, or (honestly cheapest) palette-*driven
  tinting*: drive the existing `Tint`/arena-tint/backdrop colors from one
  palette table instead of a true post-LUT. The lane spec picks one after a
  measurement; the fallback tint-table version is guaranteed shippable and is
  still the unlockable-palettes feature.
- **MCU note:** on bespoke hardware with an indexed framebuffer, the true LUT
  is literally free — the fallback version ports *forward* into the real thing.
- **Dependencies:** none. Multiplies the Grid's and Scars' visual payoff.

### 6. Battle-Scar Layer — the arena remembers the run *(Wave 2, Lane V)*

**Fantasy:** by wave 25 the floor tells the story — scorch rings under every
death, dash skid-trails, debris fields near the worst fights. Persistent
per-run, wiped on arena shift (each arena accumulates its own history).

- **Engine addition:** `ecs/systems/scar_system.{hpp,cpp}` — one
  `SDL_TEXTUREACCESS_TARGET` accumulation texture at arena scale; events stamp
  pre-generated scar sprites into it (never cleared between frames); drawn once
  per frame under the entities. Scar stamps come from the existing offline
  generator (`make_sprites.py` gains a scar family).
- **Touch points:** one draw call in the render block (hook `scars-render`);
  stamp events read from the same Blackboard events the Grid uses (deaths,
  dashes, blasts). Arena shift clears the texture (existing crossfade site,
  1 line).
- **Determinism stance:** **render-only**, same contract as the Grid.
- **Inert default:** `"scars": {"enabled": false}`.
- **MCU note:** an accumulation buffer is one framebuffer-sized surface;
  stamping is a bounded blit. Cap stamps/frame (data) to bound worst case.
- **Dependencies:** none. Shares event publishers with RG — Phase 0 defines
  the shared `grid.impulse`/`scar.stamp` event vocabulary once.

### 7. Reactor Surge Events — arena weather *(Wave 3, Lane X)*

**Fantasy:** mid-wave, the reactor *does something*: a coolant flood slows a
third of the arena, a plasma arc sweeps a rotating line you must not touch, a
vent erupts, a brief gravity storm bends everything (via #3). The wave loop's
rhythm breaks and you must reposition — the genre's monotony killer.

- **Engine addition:** `game/surge_system.{hpp,cpp}` — a scheduler (per-arena
  event tables in `GameData.json`: which events, wave windows, cadence) plus a
  small library of region effects (slow field, sweep line, eruption, gravity
  storm). Region effects are data: shape + effect id + magnitude + duration.
- **Touch points:** hook `surge` after the arena-shift tick; slows apply
  through the existing speed-mult path, damage through `ContactDamage`
  carriers spawned/despawned by the system, gravity through #3's source API.
  Telegraphs (warning glow) ride the existing particle/hazard-glow idioms.
- **Determinism stance:** **sim-side.** Scheduling draws from the sim RNG
  stream under R2 — the scheduler ticks and draws every wave regardless of
  whether an event fires (draws taken ≠ draws used).
- **Inert default:** `"surges": []` per arena.
- **MCU note:** bounded live events (≤2), region tests are circle/half-plane
  checks.
- **Dependencies:** **needs #3** (gravity storm) — hence Wave 3. Flood/arc/
  eruption would work without it, but the lane builds once, after forces exist.

### 8. Adaptive Director — invisible pacing hand *(Wave 1, Lane Q)*

**Fantasy:** the game breathes. After a near-death scramble the next spawns
hold off a beat; when you're cruising untouched, pressure arrives early. Left
4 Dead's trick, scaled to a wave table.

- **Engine addition:** `game/director_system.hpp` (header-only free function) —
  a stress scalar integrated from recent damage taken, kill rate and hull
  fraction, mapped to a bounded multiplier (e.g. 0.7–1.3, bounds in data) on
  wave-spawn *spacing* only — never counts, never skips the table.
- **Touch points:** hook `director` before `wave_spawner.update`;
  `WaveSpawnerSystem` consumes one spacing multiplier (existing spacing math,
  one multiply). Publishes `director.stress` for other systems (music, grid).
- **Determinism stance:** **sim-side**, pure function of sim state, no RNG.
- **Inert default:** `"director": {"enabled": false}` → multiplier exactly 1.0f.
- **MCU note:** three EMAs and a clamp. Free.
- **Dependencies:** none. Feeds #4's music intensity and RG's ambient hum later.

### 9. Destructible Arena — geometry that dies *(Wave 2, Lane U)*

**Fantasy:** the cover you kite around can crumble. Obstacles have HP and
crack through damage stages; a destroyed pillar becomes debris and an open
sight-line — and the wall you counted on for Ricochet Coils is gone. Every
wave physically reshapes the arena.

- **Engine addition:** `game/crumble_system.{hpp,cpp}` — routes projectile
  hits on obstacle colliders into obstacle HP, swaps damage-stage sprites
  (offline generator gains cracked variants), and on destruction removes the
  collider, spawns debris (existing particle + loot idioms) and invalidates
  the A* grid region.
- **Touch points:** hook `crumble` after `projectile_hit.update`; obstacles
  gain `hp` in their `GameData.json` rows (0 = indestructible, the default);
  the pathfinding grid gets a dirty-region rebuild call (existing
  `tile_map`/`pathfinding` API — lane spec verifies rebuild cost).
- **Determinism stance:** **sim-side.** Damage and destruction are
  deterministic; debris cosmetics draw from the sim stream under R2 only if
  randomized.
- **Inert default:** all obstacle `hp: 0` → nothing anywhere changes,
  including the collision masks already in place.
- **MCU note:** obstacle counts are small (15–20/arena); a dirty-region A*
  rebuild is bounded and infrequent.
- **Dependencies:** none. Interacts richly with Ricochet Coils, mines, #7.

### 10. Flight Report — the run as an artifact *(Wave 1, Lane S)*

**Fantasy:** game over. Instead of just a score: the arena outline with your
entire flight path burned into it, kill density as heat, deaths marked, the
wave-by-wave credit curve — a reactor incident report. On a friends-cartridge
console, this is the screen people photograph and argue over.

- **Engine addition:** `game/flight_report.{hpp,cpp}` — a passive recorder
  (player position every N frames, kills/deaths/credits with positions, fixed
  ring buffers) plus a renderer that composes the report onto the existing
  `game_over`/`victory` screens using the minimap's proven mapping math
  (`minimap_math.hpp`) and pooled-widget idiom.
- **Touch points:** hook `telemetry` in the every-phase block (record) and the
  game-over/victory screen defs in `GameData.json` (display). Optionally a
  `--report out.bmp` dump via the existing screenshot system.
- **Determinism stance:** **passive.** Reads sim state, writes to its own
  buffers, renders on terminal screens. Nothing reads it back.
- **Inert default:** `"flight_report": {"enabled": false}` → buffers still
  advance nothing (recording gated), screens unchanged.
- **MCU note:** fixed ring buffers (e.g. 4096 samples), integer positions.
- **Dependencies:** none. The deferred ghost family would reuse its buffers.

---

## Deferred, not dead: the recorder family

The other half of the brainstorm — a first-class **input recorder** enabling
the Echo Drone mechanic (fight alongside yourself from 8 s ago), watchable
ghost replays of any high score, arcade attract mode on the title screen, and
pass-the-cartridge ghost/bones exchange with zero networking (Devil Daggers /
Trackmania / NetHack-bones lineage). It is the strongest *console-identity*
play the engine can make because determinism makes replays a few KB of inputs.
Deliberately out of this suite to keep the waves shippable; the Flight Report's
buffers and this doc are its landing pad. Revisit after Wave 3.

---

## Execution roadmap

One **Phase 0 scaffolding session**, then three waves of parallel lanes with
exclusive file ownership, then an integration session per wave — the
iteration-3/5 playbook. Waves are sized so each lane fits one focused session;
that is the usage-window control.

**Phase 0 (D138):** every shared-file edit made once, inert — the eleven hook
blocks in `main.cpp` (`timescale`, `director`, `forces`, `surge`, `crumble`,
`pattern`, `telemetry`, `grid-render`, `scars-render`, `palette`, `audio`),
`GameConfig` sub-structs parsed-but-disabled in `arena_config.cpp`, the shared
Blackboard event vocabulary (`grid.impulse`, `scar.stamp`, `director.stress`),
`test_scaffolding.cpp` extended to pin the new hooks. Gate: build clean,
ctest 100%, canary byte-identical twice.

| Wave | Lane | Feature | Owned new files |
|------|------|---------|-----------------|
| 1 | P | #1 Temporal Overload | `game/timescale_system.hpp` |
| 1 | Q | #8 Adaptive Director | `game/director_system.hpp` |
| 1 | R | RG Resonance Grid | `engine/ecs/systems/resonance_grid_system.*` |
| 1 | S | #10 Flight Report | `game/flight_report.*` |
| 2 | T | #3 Force-Field Layer | `game/force_field_system.*` |
| 2 | U | #9 Destructible Arena | `game/crumble_system.*` |
| 2 | V | #6 Battle-Scar Layer | `engine/ecs/systems/scar_system.*` |
| 2 | W | #5 Palette Engine | `engine/ecs/systems/palette_system.*` |
| 3 | X | #7 Reactor Surge Events | `game/surge_system.*` |
| 3 | Y | #2 Bullet-Pattern Language | `game/bullet_pattern.*` |
| 3 | Z | #4 Chip-Synth Audio | `engine/ecs/systems/chip_synth_system.*` |

Merge order within a wave: cheapest/least-shared first (Wave 1: P → Q → S → R;
Wave 2: T → U → V → W; Wave 3: X → Y → Z). Full gate re-run on every merge
commit. Each integration session also updates `ENGINE.md`, `decisions.md`,
`progress-tracker.md` and `project-overview.md`.

**Known cross-lane seams to watch at integration** (the iteration-5 lesson):
the RG/scar event vocabulary (defined once in Phase 0), V/W both touching the
render block (disjoint hooks, verify no shared-file edits), and #7 consuming
#3's source API (Wave-boundary, so the API is merged and stable first).

**After every wave: play it.** The suite is feel-driven; five features have
already shipped unplayed on master and this branch must not repeat that.
