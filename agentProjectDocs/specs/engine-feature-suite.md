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

**Execution mode (owner decision, 2026-08-10): hybrid pairs.** One Phase 0
scaffolding session, then **two lanes per session** — paired so one is
sim-side and one is render-only/isolated, so their files can never collide.
Each session builds both lanes, merges them, and re-runs the full gate itself;
a playtest checkpoint lands at every wave boundary. ~8 medium sessions total:

| Session | Lanes | Pairing logic |
|---------|-------|---------------|
| 0 | Phase 0 scaffolding | all shared-file edits, inert, canary-gated |
| 1 | P (#1 timescale) + R (RG grid) | sim + render |
| 2 | Q (#8 director) + S (#10 flight report) | sim + passive — **Wave 1 playtest after** |
| 3 | T (#3 forces) + V (#6 scars) | sim + render |
| 4 | U (#9 destructible) + W (#5 palette) | sim + render — **Wave 2 playtest after** |
| 5 | X (#7 surges) + Y (#2 danmaku) | both sim-side but disjoint hooks/files; surges consume forces (merged in session 3) |
| 6 | Z (#4 chip-synth audio) solo | last and rip-out-able, per project law — **Wave 3 playtest + full-suite playtest after** |

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

---

# Merge Notes

**Read this at merge time, not before.** Per `ai-workflow-rules.md`, a feature
branch does not write `decisions.md` or `progress-tracker.md` — both are
append-heavy and every branch touches them, so editing them on a branch is a
guaranteed conflict. Everything that would have gone there lives here instead,
under the same rules (a decision carries its *why* and what was rejected). At
merge time, on `master`, move the decisions into `decisions.md`, the state into
`progress-tracker.md`, and delete this section.

`ENGINE.md` is deliberately NOT in that arrangement — it is edited rarely and
mid-file, so the suite's §6b/§6c sections were written there directly.

## Decision-id ledger

The suite reserved D138-D180 and has spent **D138-D151**. D152-D180 are burned.
`master`'s next free id is unchanged at **D194**.

## State (for progress-tracker.md at merge time)

- All eleven lanes implemented; every one behind a `GameData.json` flag that
  ships OFF. `--suite` turns the set on.
- **One playtest done (2026-08-12)**, by Conrad, in a window. Results are D151.
  Still unverified after that pass: Temporal Overload (no 3-kill chain happened),
  the revised resonance grid, the flight report, surges, bullet patterns, and
  destructible cover.
- Chip-synth audio is **shelved**: the code builds and is tested, nothing
  constructs it.
- The battle-scar layer is **cut**.
- `bash gate.sh .canary-baseline.txt` runs the whole gate.
- Unrelated flake: `bugs/010-path-property-test-flake.md`, pre-existing on
  `master`, ~5% of ctest runs.

## D138 — Engine-suite Phase 0: eleven inert hooks, one shared FX event vocabulary

Branch `engine-suite-build`, off current `master` (the older
`feature/engine-suite` branch carries the umbrella spec but predates the
gameplay-polish merges, so the spec was brought across as a file rather than the
branch being rebased).

Same playbook as iteration-3 Phase 0 (D51) and iteration 5: every shared-file
edit made **once**, up front, inert, so each of the eleven features lands in one
comment-delimited `// === HOOK: name ===` block and no lane has to touch
`main.cpp`, `arena_config.*` or `GameData.json` while another is building. The
hook table is in `ENGINE.md` §6b; `test_scaffolding.cpp` pins all eleven names
by reading `main.cpp` as text.

**The one new shared surface is `engine/ecs/fx_events.hpp`** — two per-frame
Blackboard lists (`fx.grid_impulses`, `fx.scar_stamps`) that sim-side combat
sites publish to and the two render-only lanes (resonance grid, battle scars)
consume while drawing. Defined **once**, in Phase 0, because both consumers want
the same three moments (deaths, dashes, blasts) and the iteration-5 lesson was
that a vocabulary invented twice diverges. The contract is one-way — nothing
sim-side may read the lists back — which is what keeps those two lanes out of the
determinism argument entirely. `clear_frame()` runs unconditionally at the top of
every frame so a disabled consumer cannot leak.

**Rejected:** a `FxEvent` *component* per event. Invariant 6 makes a new
component type the expensive move (storage member, two specialisations, six
instantiations, a `destruction.cpp` sweep, a `debug_adapters` registration), and
these events live exactly one frame and are never queried per entity. Two
Blackboard vectors are the cheap structural option the standards ask for first.

**Trap found and paid for immediately: the top-level JSON key `"grid"` is already
claimed.** The class-baseline `gamedata_loader.cpp` §4.7 parses a match-3 tile
grid from `data["grid"]` and reads `grid["rows"]` **unguarded**, so the
resonance grid's first data block aborted the loader (an nlohmann assert inside
`operator[]`) before `main()` ran — presenting as a `Game_Unit_Tests` SIGABRT in
an unrelated test case. The block is `"resonance"` instead. The full list of
engine-claimed top-level keys is now in `ENGINE.md` §6b; check it before naming
a new one. Modifying the inherited loader to guard the read was rejected — it is
a protected class-baseline file and the collision is ours to avoid.

Config added to `GameConfig`, all inert: `TimescaleConfig`, `DirectorConfig`,
`GridConfig`, `FlightReportConfig`, `ForceConfig`, `ScarConfig`, `PaletteConfig`,
`AudioConfig`, `std::vector<BulletPatternDef>`, plus `ObstacleDef::hp`
(0 = indestructible), `ArenaDef::surges` and `EnemyType::pattern`. No new
component type.

Verified: clean build (only Lua's `tmpnam`), `ctest` 8/8, replay canary
byte-identical twice **and identical to the pre-Phase-0 baseline** on
`--seed 42 --keys 5:SPACE --stopframe 3000`. Not played in a window — Phase 0
ships no behaviour to play.

## D139-D150 — the engine feature suite, built on `engine-suite-build`

Eleven features from `specs/engine-feature-suite.md`, one entry per lane because
they were built in the spec's paired order and each one is independently
revertable. **Every lane is data-disabled by default**; `--suite` (D141) turns the
whole set on for a playtest. That is what lets the replay canary stay
byte-identical to pre-suite `master` while eleven features sit in the tree.

**D139 — #1 Temporal Overload (Lane P), `game/timescale_system.hpp`.** A free
function in a header (the `tick_shields` idiom) that rewrites the frame's
`delta_time` from a scale that is a pure function of sim state: a 3-kill chain
inside 1.2 s dilates for a beat, and critical hull dilates for as long as it
lasts. Fed the REAL dt, never its own output — feeding it back would make the
ease rate itself dilate and the effect would never recover.
*The frame-vs-seconds audit the spec asked for:* frames still advance one per loop
iteration, so everything frame-indexed is unaffected **by construction** —
scripted `--keys`, `--stopframe`, the F1 pause's `end_frame_no_advance`, and the
particle system's per-phase emit gate. Everything counting seconds (wave delays,
spawn intervals, cooldowns, lifetimes, i-frames) slows together, which is the
feature. Only `PHASE_PLAYING` dilates, so no menu animation is ever slowed by a
fight that is not running. `min_scale` floors it at 0.35 — a scale of 0 would stop
every seconds-based timer in the sim permanently.
**Rejected:** scaling inside `Timer`. `Timer` also owns the frame counter and the
sleep budget, and dilating a *wall-clock* pacer would slow the real frame rate
rather than the simulated one.

**D140 — RG Resonance Grid (Lane R), `engine/ecs/systems/resonance_grid_system.*`.**
A 40x28 damped spring lattice under the entities, kicked by the `fx.grid_impulses`
published at deaths, dashes and pillar collapses. Nodes are independent
oscillators rather than a coupled mesh: the impulse falloff (3.5 cells) is then
the exact reach, which bounds the per-impulse cost, and a coupled mesh's wave
propagation is not visible at 40 px pitch anyway. Semi-implicit integration with
damping as a multiplicative decay, so a stiff spring cannot blow up at any dt, and
`dt` is clamped internally — a 2-second stall frame must not launch the lattice.
Drawn as one `SDL_RenderLines` strip per row and column (68 calls, not 2200), with
per-strip alpha from that strip's peak displacement, which is what makes it read
as a *display* of the combat rather than as decoration.
**Stepped by the REAL dt, not the dilated one**, so bullet time does not turn the
lattice to treacle.

**D141 — `--suite`.** One CLI flag that flips every suite feature on, plus the two
that are inert by *data* rather than by a flag (it authors obstacle HP for the
destructible arena and a surge table + a boss bullet pattern). It exists because
the branch's whole purpose is a merge decision, and a reviewer should not have to
hand-edit `GameData.json` to see what they are deciding about. Everything is
inside `if (opts.suite)` and applied before `base_config` is taken, so a restart
mid-session keeps the suite on and the default path is untouched.
**Trap paid for here:** the new parse branch silently stole `--dev`'s `++i`, so
`--dev` span the argv cursor forever — a hang, not a failure, and it presented as
an unrelated unit-test timeout. `test_cli_parser.cpp` now pins that every flag
combination parses at all.

**D142 — #8 Adaptive Director (Lane Q), `game/director_system.hpp`.** Three EMAs
(damage taken, kills landed, hull remaining) into a stress scalar in [0,1], mapped
onto ONE multiplier over `WaveSpawnerSystem`'s spawn *spacing*. It cannot change
counts, rosters or wave order — the authored table stays the single authority on
what a run contains — which is what keeps it invisible instead of a difficulty
setting nobody asked for. `spacing_mult` is read into a local before the compare
and the subtract, so a mid-wave change can never leave the timer above the
threshold and fire twice. A frozen phase HOLDS the stress rather than decaying it:
the stress that opened the shop is the pacing the next wave should start from.
Publishes `director.stress`, which the chip synth consumes as music intensity.

**D143 — #10 Flight Report (Lane S), `game/flight_report.*`.** Fixed ring buffers
of the flight path (every Nth frame), the hits taken and the kill positions,
composed onto the game-over/victory screens through `minimap_math`'s mapping and
the pooled-widget idiom. Two decisions worth keeping: the report **decimates**
rather than truncates, so a long run shows the shape of the whole flight instead
of its first thirty seconds; and hits are detected from the hull *dropping* rather
than from a publisher at the damage site, which catches every damage source
including ones not written yet (surges, crumbling pillars). Kill positions come off
the shared `fx.scar_stamps` list — one sim event, two render-only consumers.

**D144 — #3 Force-Field Layer (Lane T), `game/force_field_system.*`.**
Fixed-capacity sources, one accumulation pass writing velocity deltas, run before
`movement.update` so a field acts on the frame it exists — and the arena clamp and
obstacle push-out still run after movement, so no field can pull a body through a
wall however strong it is. Inert by SHAPE, not by a flag: no sources means the
pass iterates nothing, so there is no `enabled` bool to check. Per-frame delta is
clamped, because a delta big enough to teleport a body is a delta the push-out
cannot resolve.
**Rejected: folding `items::repulse_enemies` into it**, which the spec asked for.
That helper pushes *positions* after the clamp; the force layer writes
*velocities* before movement. Converting it would retune a shipped item's feel
without a playtest, and this branch exists to answer "should the suite merge?" —
blurring it with a change to an existing item makes that question harder to
answer, not easier. The fold is a one-lane follow-up once the suite is judged.

**D145 — #6 Battle-Scar Layer (Lane V), `engine/ecs/systems/scar_system.*`.** One
`SDL_TEXTUREACCESS_TARGET` accumulation texture at arena scale, never cleared
between frames, stamped from `fx.scar_stamps` and wiped on an arena shift so each
arena keeps its own history. The scorch sprite is generated procedurally at
runtime instead of coming from the offline generator: that keeps the whole feature
inside one file pair with no committed binary and no sidecar. Stamps per frame are
bounded twice — at the publisher (`fx_events::MAX_PER_FRAME`) and here
(`max_stamps_per_frame`) — so the 30-second stall force-kill wiping a wave in one
frame costs a known number of blits. Texture creation failing degrades to drawing
nothing; a cosmetic layer must never take a run down.

**D146 — #9 Destructible Arena (Lane U), `game/crumble_system.*`.** `ObstacleDef`
gained `hp` (0 = indestructible, every shipped row); a positive value gives the
pillar a `Health`, and `ProjectileHitSystem` routes a shot into anything solid
carrying one through the **existing** `DamageEvent` -> `DamageApplySystem` path, so
there is no second damage path to keep in sync. The A* grid is rebuilt from
`CrumbleSystem`'s live obstacle list rather than from the config rows, and
`set_arena` re-seeds that list on every arena apply — each arena arrives whole, so
a run cannot permanently strip the map. The rebuild call lives in `main.cpp` beside
the existing one, so exactly one place knows how the grid is built.
**Deferred:** cracked sprite variants from the offline generator. Damage stages
are a darkening multiply for now, which reads at the size obstacles draw and costs
no new assets; the `ponytail:` comment names the upgrade.

**D147 — #5 Palette Engine (Lane W), `engine/ecs/systems/palette_system.*`.**
*The feasibility gate the spec left open, resolved:* SDL3's 2D renderer has no
shader hook, so a true indexed LUT would need either a CPU pass over ~650k pixels
a frame or an `SDL_RenderGeometry` trick that still cannot express an arbitrary
index remap. What ships is a **duotone resolve**: the frame is captured to a
target texture and drawn twice — once modulated toward the palette's shadow
colour, once added back in its highlight colour — which recolours every pixel from
one table in two draw calls and no per-pixel CPU work. Honest limitation: that is
a *tone map*, not an index remap, so two colours with equal luminance resolve
alike where a real LUT could send them anywhere. On indexed-framebuffer hardware
the true LUT is free and the palette table is already the data it would need.
Hull-critical pushes the mix, so the WORLD browns out instead of a red vignette —
the point of owning the frame's colour. Needs two hook blocks (arm before the
first draw, resolve after the last), the same necessity the prestige hook has.

**D148 — #2 Bullet-Pattern Language (Lane Y), `game/bullet_pattern.*`.** A pattern
is a `GameData.json` row: an op list of ring/fan/spiral/aimed/wait with counts,
speeds, spreads and angular velocities. `op_angles` is pure and engine-free, so
the whole choreography unit-tests without a world. **No RNG anywhere in the
interpreter** — variation is authored — so it cannot perturb the sim stream under
any conditional. Shots spawn through the existing `enemy_fire::spawn_shot`, so a
pattern shot IS an enemy shot (layer, ContactDamage, trail) and needs no damage
system. `MAX_SHOTS_PER_OP` (64) bounds a mistyped `count` in code as well as in
data; the real constraint is the particle budget, since each shot carries a ~10
live-particle trail. `EnemyBehavior` gained `pattern`/`cursor`/`phase` — three
fields on an existing struct, which is the cheap option Invariant 6 asks for
before a new component type. A non-looping pattern retires its emitter
(`pattern = -1`) rather than parking an out-of-range cursor, which the range guard
would otherwise wrap back to op 0 forever.

**D149 — #7 Reactor Surge Events (Lane X), `game/surge_system.*`.** Coolant floods
(velocity drag applied per frame, so it self-cancels the moment a body leaves and
there is no state to restore), a sweeping plasma arc (a moving `ContactDamage`
carrier riding the arc, so the existing collision path handles it), eruptions, and
a gravity storm that registers straight into Lane T's source API and owns no
physics of its own. Two live events maximum, and the damaging effects carry
`ContactDamage` like the Phase-6 hazards, so no new damage system exists.
*Determinism:* the scheduler owns a **private** `mt19937` seeded per run, so
authoring surge content cannot shift a spawn or a loot roll; within its own stream
it takes exactly three draws on every wave tick, before any conditional, so
retuning a `chance` cannot desync a later wave either (R2 / D18/D19). A surge that
fires does move the entity-id cursor, which is a real sim change — and exactly why
the feature stays inert until an arena authors a table.
**Trap paid for here:** `carrier == 0` as a "no carrier" sentinel is wrong —
`EntityManager` hands out 0 as a valid id, so the first entity of a run would have
been invisible to the teardown and left a hazard box on the floor forever. It is a
separate `has_carrier` flag.

**D150 — #4 Chip-Synth Audio (Lane Z), `engine/ecs/systems/chip_synth_system.*`.**
No samples: an 8-voice pool (pulse/saw/triangle/noise) with per-note pitch sweep
and a decay envelope, an SFX table, and a 16-step bass sequencer whose tempo and
voicing follow `director.stress` — the Adaptive Director doubling as a music
director. One file pair; the entire diff outside it is one include, one hook block
and `SDL_INIT_AUDIO` inside `start()`, so a single revert removes the feature
(project law for this lane). **Every trigger site is inside the system**, reading
keys the game already publishes for its own reasons, so no game file has an audio
line in it. Lock-free: the audio thread only ever reads atomics the game thread
writes, and the callback neither allocates nor blocks (a lock in the audio path is
a click, and a click is worse than a dropped note). Its noise voice runs a private
LCG, never the sim's stream.
**Trap paid for here, and it cost a core dump:** `main.cpp` calls `SDL_Quit()` as a
statement, but a function-local static system is destroyed at *process exit* — so
the destructor was tearing down an audio stream after SDL had already gone. It is
guarded on `SDL_WasInit(SDL_INIT_AUDIO)`. Any future engine system holding an SDL
resource in a static has the same exposure.
**Not authored in data:** the SFX note table is code, because these are 1-3 note
blips and a JSON table for them would be more surface area than the sounds; the
`ponytail:` comment names where it moves if sound design wants live tuning. The
eight committed WAVs under `assets/Audio/` remain unused by anything.

## D151 — the first playtest batch: grid revised, scars cut, audio shelved

Conrad played `--suite` in a window on 2026-08-12 — the suite's first contact with
a human. Five notes came back; this is one entry for the batch, numbered by item.

1. **Chip-synth audio: shelved, not deleted.** The `audio` hook block is empty and
   `--suite` no longer enables it, but `engine/ecs/systems/chip_synth_system.*`
   stays in all three CMake lists and keeps its tests, so it still compiles against
   the engine it was written for. **Rejected:** dropping it from the build, which
   is smaller but guarantees it silently stops compiling the first time an engine
   API moves under it — and the whole point of shelving rather than deleting is
   that it comes back cheaply. Re-wiring is the hook block plus one include.

2. **Resonance grid: three defects, one revision.**
   - *"Does not span the entire map"* — a real bug with an embarrassing cause. The
     lattice was a fixed 40x28 at 40 px = **1600 px** across; the arena is radius
     **1400**, i.e. **2800 px** across. It covered 57% of the arena and stopped
     visibly short of the wall. `cols`/`rows` are gone from the config entirely and
     `configure_for_arena()` derives them from the arena span, +2 nodes of margin.
     The old numbers were not wrong so much as *unrelated* to the arena, which is
     the failure mode the "every tunable is data" rule invites when two numbers in
     data have to agree with a third somewhere else. Pinned by a test that asserts
     the span covers the shipped 2800.
   - *"Out of alignment with the parallax"* — structural, and not fixable as
     asked. The backdrop layers are 512² tiles drawn at scroll factors 0.25/0.6/0.9
     — they move *relative to the world* — while the lattice is anchored in world
     space so a ripple lands where the explosion was. The two can only line up at
     one camera position. **Owner's call (asked and answered): stay
     world-anchored.** The pitch moved 40 → 64 (512/8) so the two are at least
     commensurate on frames where both are visible. **Rejected:** pinning the
     lattice to the near backdrop's scroll factor, which would align it perfectly
     and make the ripple drift away from its own explosion — backdrop decoration
     instead of a physics display.
   - *"Too much visual clutter"* — fixed by making the lattice **invisible at
     rest**. Resting alpha is now zero and a strip with no displacement is not
     drawn at all; `ResonanceConfig::a` is the PEAK alpha of a fully-displaced
     strip rather than a resting one, and the alpha curve is `sqrt(lit)` so a
     blast's leading edge reads as an edge rather than as a gradient. This also
     quietly disposes of the alignment complaint: you never see a resting lattice
     next to the backdrop grid to compare them against.
   - *"Only bosses and explosions"* — the per-death and per-dash publishes are
     gone. Three sites publish an impulse now: a mine blast
     (`specialty_system.cpp`), a collapsing pillar (`crumble_system.cpp`) and a
     boss summon volley (`boss_system.cpp`). A pillar is a judgment call — it is
     an explosion in all but name and already spawns a debris burst — and is easy
     to veto. **The seam for "other things explode later" is one line at the
     explosion's spawn site**; the grid needs no knowledge of what exploded.

3. **Palette engine: kept as-is.** No change.

4. **Battle-scar layer: cut.** `scar_system.*`, its hook body, config block, test
   and GameData entry are deleted. The reason is the one that matters — the arena
   reads as floating in space, so scorch marks accumulating on "the floor" never
   made sense, and no amount of tuning fixes a premise. **What survived:** the
   `fx_events` stamp list, because the flight report reads kill positions off it
   and the deferred ghost/replay family wants the same events. It is renamed
   `Stamp`→`Mark` and `fx.scar_stamps`→`fx.kill_marks`, since naming a live
   vocabulary after a deleted consumer is how dead concepts haunt a codebase.

5. **Temporal Overload: still unverified.** No 3-kill chain happened in the
   playtest. Fastest check next time is `--dev --level 12`, where the waves are
   dense enough to chain almost by accident.

Verified: clean build (only Lua's `tmpnam`), `ctest` 8/8 (361 unit cases), replay
canary byte-identical twice and identical to the pre-suite baseline. **Not
re-played** — items 1-4 are all shipped unplayed, and the grid revision in
particular is a visual change that only a window can judge.
