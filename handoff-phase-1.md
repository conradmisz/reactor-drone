# Handoff — Gameplay Upgrade Phase 1 (Waves & enemy size)

Plan: `/home/conrad/.claude/plans/i-will-provide-a-peppy-kahan.md` (§4 Phase 1).

## 1. Phase 1 summary

Take the game from a 6-wave arc to a **20-wave** one with two wave modes (fixed-count
1–11, timed 12–20), make enemies **1.5× bigger** so a mouse-aimed drone can actually
hit them, and change wave advance from "quota spawned" to "quota spawned **and** arena
clear" so the Phase 3 shop always opens on an empty arena. No new components, no
economy, no shop — data plus one system rewrite.

## 2. What shipped

- **Timed waves.** `WaveDef.duration` (`arena_config.hpp:102`): `>0` spawns for that
  many seconds after the delay and ignores `count`; `0` keeps the old fixed-count
  behaviour. Branch at `wave_spawner_system.cpp:128`.
- **Per-wave difficulty multipliers** `hp_mult` / `speed_mult` (`arena_config.hpp:103-104`),
  applied at the two spawn sites (`wave_spawner_system.cpp:83-87`). Scales the existing
  3 enemy types instead of adding new ones (D10).
- **Arena-clear gating (D4).** A wave advances only once its quota/timer is done and no
  `EnemyTag` remains (`wave_spawner_system.cpp:141-166`). This changed **fixed-count
  waves too** — previously a wave could end while enemies were still alive.
- **`WaveSpawnerSystem::wave_just_cleared()`** (`wave_spawner_system.hpp:71`) — true for
  the single update after a wave clears. Nothing consumes it yet; Phase 3's shop entry does.
- **Stall watchdog (R3).** After `wave_stall_timeout` seconds of a finished-but-uncleared
  wave, stragglers get `Health.current = 0` and die normally
  (`wave_spawner_system.cpp:146-155`). Prevents a soft-locked run.
- **Enemies clamped to the arena circle** (`main.cpp:434-451`) — the player's existing
  clamp, now a lambda applied to both. Second half of the R3 mitigation.
- **Enemies render at layer 2 (R5)** (`wave_spawner_system.cpp:90`). They used to default
  to layer 0, i.e. *behind* the walls; at 78 px that reads as a bug.
- **Enemy sizes ×1.5 → 64 / 70 / 78** (`GameData.json` `enemy_types`). `Size`, `Collider`
  and `CircleCollider` all derive from `type.size`, so this is one number each.
- **20 waves** in `GameData.json`: 1–6 unchanged, 7–11 new fixed-count, 12–20 timed.
- **`update()` split** into `spawn_enemy()` + `update()` (`wave_spawner_system.cpp:38,100`).
  The old function returned early through the spawn path, which made "keep evaluating the
  advance condition every frame" impossible to express.
- **Two unit tests** (`tests/unit/test_arena_systems.cpp:105,138`).

## 3. What did NOT ship and why

- **No economy, pickups, shop, items, or upgrade visuals** — Phases 2–5, out of scope.
- **`xp`/`score` on enemies and the XP→upgrade path are untouched.** Phase 2 deletes them.
- **Arena themes still activate at waves 1 / 3 / 5** (`GameData.json` `arenas[].first_wave`),
  so waves 5–20 all play in Bio-lab. Respreading them over 20 waves is a one-line data
  edit but it's a *design* call the plan doesn't make — flagging it, not deciding it.
- **No playtest.** Everything below is headless verification; the balance-feel log is empty
  because nobody has actually played the 20-wave arc yet. First real play is the priority.
- **`enemies_spawned_in_wave()` is meaningless for timed waves** (it counts up with no
  ceiling). Nothing reads it except tests; left as-is.

## 4. Files touched

| Path | What changed | Why |
|---|---|---|
| `CPP/game/arena_config.hpp` | `WaveDef` + `duration`/`hp_mult`/`speed_mult`; `GameConfig` + `wave_stall_timeout` | Timed waves, D10 scaling, R3 watchdog knob |
| `CPP/game/arena_config.cpp` | Parse those 4 fields via `json.value(k, default)` | Old JSON still loads unchanged |
| `CPP/game/wave_spawner_system.hpp` | `spawn_enemy()` decl, `stall_timer_`, `wave_just_cleared_`, reset covers both | New state |
| `CPP/game/wave_spawner_system.cpp` | Split spawn/update; timed branch; arena-clear gate; watchdog; multipliers; `RenderLayer{2}` | The phase's whole behaviour change |
| `CPP/game/main.cpp` | Player arena-clamp → `clamp_to_arena` lambda, applied to enemies too | R3 |
| `assets/GameData.json` | Enemy sizes 64/70/78; `waves[]` 6→20; `wave_stall_timeout` | D8, D9, R3 |
| `CPP/game/tests/unit/test_arena_systems.cpp` | 2 new `[waves]` cases + `two_wave_config()` helper | Cover the timed/gating logic |

## 5. New surface area

- **Components added/renamed:** none. Enemies now also carry the *existing* `RenderLayer`.
- **Blackboard keys added:** none. (`wave`, `total_waves`, `all_waves_complete` behave as before —
  `total_waves` is now 20.)
- **New systems:** none. `main.cpp`'s update order is unchanged; the only edit there is the
  clamp loop at `:434-451`, which still sits between `movement.update` and the obstacle push-out.
- **New public API:** `WaveSpawnerSystem::wave_just_cleared()` — a plain const getter,
  reset to `false` at the top of every `update()`. Read it *after* `wave_spawner.update(...)`
  in the same frame or you'll miss the edge.
- **New JSON keys:** `wave_stall_timeout` (top level); `duration`, `hp_mult`, `speed_mult`
  (per wave entry). All optional.

## 6. Tuning values chosen

| Value | Where | Why |
|---|---|---|
| sizes 64 / 70 / 78 | `GameData.json` `enemy_types[].size` | D8, exactly ×1.5 of 42/46/52 |
| waves 7–11: count 20→28, interval 0.40→0.32 | `waves[6..10]` | Continues the existing 6→18 / 0.80→0.40 curve without a step change |
| waves 7–11: `hp_mult` 1.10→1.30 | `waves[6..10]` | Gentle ramp; the player has no shop yet at wave 8 on a first run |
| waves 12–20: `duration` 20→36 s (+2 s/wave) | `waves[11..19]` | Linear, per plan. ~4 min of timed survival total |
| waves 12–20: `spawn_interval` 0.55→0.30 | `waves[11..19]` | Pressure rises while the wave lengthens |
| waves 12–20: `hp_mult` 1.4→2.2, `speed_mult` 1.10→1.30 | `waves[11..19]` | Assumes shop purchases roughly double player output by then — **unvalidated** |
| `wave_stall_timeout` 30.0 | `GameData.json` top level | Long enough that a slow hulk crossing a 1400 r arena is never force-killed (~30 s at 45 px/s is 1350 px), short enough that a real stall is a hiccup, not a dead run |
| enemy `RenderLayer{2}` | `wave_spawner_system.cpp:90` | Same layer as walls/obstacles, below player(3); spawned later so ties draw on top |

## 7. Known bugs & rough edges

- **The whole difficulty curve is a guess.** 20 waves have never been played end to end.
- **Timed waves ignore `count`** — setting both in JSON silently uses `duration`. No warning.
- **The watchdog kills through the normal death path**, so force-killed stragglers still pay
  out score/XP (and, from Phase 2, currency). Acceptable — it only fires on a genuine stall —
  but it is technically free money if a player finds a way to trigger it.
- **Enemies at layer 2 tie with walls.** Order within a layer is insertion order, not sorted;
  it looks right today because enemies are created after the arena props, but an arena swap
  mid-wave re-creates props and could flip that for enemies alive across the swap.
- **`victory_wave` is still 0**, so a full run is now all 20 waves. Set it to a lower number
  in `GameData.json` when you want a short test loop.
- **Waves 12–20 have no upper bound on enemy count** — a timed wave with a stuck player can
  pile up ~120 enemies at 0.30 s intervals. Watch the frame time before Phase 5's particles.

## 8. Balance-feel log *(append-only)*

- **2026-07-28 — Phase 1, not yet played.** Headless verification only (no input in scripted
  runs, so score stays 0 by design). Every number in §6 is a first guess. Open questions for
  the first real play: is wave 6→7 a wall now that the arena must be cleared? Do 78 px hulks
  make the arena feel crowded at wave 20's spawn rate? Is 36 s of timed survival too long
  without a shop break in the middle?

## 9. Design-decision log *(append-only)*

Seeded from plan §2:

| # | Decision |
|---|---|
| D1 | Shop **replaces** the XP auto-upgrade system — one economy, not two. |
| D2 | Shop opens every 4 waves (4, 8, 12, 16, 20) plus a rare key drop for on-demand entry. |
| D3 | Per-run only. No persistence, no save file. |
| D4 | Timed waves end when the timer expires **and** the arena is clear. |
| D5 | Physical pickups — dead enemies drop collectibles you walk over. |
| D6 | Items (1 slot, passive): Magnet Core, Repulsor Field, Reactive Plating, Salvager. |
| D7 | Consumables (1 slot, one-use): Repair Kit, Overdrive, EMP Burst, Phase Shift. |
| D8 | Enemy sizes ×1.5 → 64 / 70 / 78 px. |
| D9 | Waves 7–11 hand-authored fixed count; 12–20 timed. |
| D10 | No new enemy types — scale the existing 3 with per-wave `hp_mult` / `speed_mult`. |
| D11 | Shop UI is a numbered keyboard list first; clickable cards in Phase 6. |
| D12 | Handoff = full schema + balance-feel log + design-decision log, carried forward. |

Added this phase:

| # | Decision |
|---|---|
| D13 | Arena-clear gating applies to **all** waves, not just timed ones. Uniform rule, and it is what makes D4's "shop opens on an empty arena" hold for the wave-4 and wave-8 shop stops. |
| D14 | The stall watchdog force-kills via `Health = 0` rather than `DestroyRequest`, so stragglers go through `EnemyDeathSystem` like any other kill. One death path = Phase 2's currency drop needs no special case. |
| D15 | `wave_just_cleared()` is a plain getter cleared at the top of `update()`, not a consume-on-read. Read-only getters that mutate are a 3am bug; the cost is that callers must read it after `update()` in the same frame. |
| D16 | Enemies get `RenderLayer{2}` now (R5) rather than deferring to the Phase 5 visual pass — the sizes were being changed in this phase anyway, which is exactly what makes the bug visible. |

## 10. Verification

```
cd CPP/build && cmake . && cmake --build . -j     # clean: only pre-existing vendored-lua warnings
ctest --output-on-failure                          # 8/8 passed (100%)
./game/tests/game_unit_tests "[waves]"             # 32 assertions in 3 cases, all passed
SDL_VIDEODRIVER=dummy ./game/game --seed 1234 --stopframe 900   # ×2, output identical → deterministic (R2 canary)
```
`GameData.json` re-parsed: 20 waves, 9 timed, sizes `[64, 70, 78]`.
Not run: an interactive play session.

## 11. Phase 2 entry point

Open `CPP/engine/ecs/component_storage.hpp:388` (the `Experience` storage member) and
rename that slot to `ShipState` — plan §1.11 confirms it's a rename across
`component_storage.hpp:388,492-493,835`, `component_storage.cpp:710-712,740`,
`destruction.cpp:60`, so the only genuinely *new* registration in the whole project is
`Pickup`. Do that rename first, before deleting `experience_system.*` — the deletion
breaks two test files (R8) and you want one compile error at a time.
