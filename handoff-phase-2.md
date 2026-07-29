# Handoff — Gameplay Upgrade Phase 2 (Economy: currency, pickups, removing XP)

Plan: `/home/conrad/.claude/plans/i-will-provide-a-peppy-kahan.md` (§4 Phase 2).
Previous: `handoff-phase-1.md`.

## 1. Phase 2 summary

Replace the XP/auto-upgrade progression with a single currency economy (D1). Dead
enemies now drop **physical pickups** you walk over (D5) that credit a per-run
`ShipState` on the player; the XP component, the two systems that consumed it and
their JSON blocks are gone. This is the one phase that touches the engine's
component registry, so all of that work — one rename (`Experience` → `ShipState`)
and exactly one genuinely new type (`Pickup`) — happens here, once. No shop, no
items, no upgrade visuals: nothing spends the currency yet.

## 2. What shipped

- **`ShipState`** (`player_components.hpp:32-42`) — one fat struct on the player
  holding *all* per-run shop state: `currency`, `keys`, shield fields,
  `speed_mult`, `item_id`, `consumable_id`, `buff_id`/`buff_timer`,
  `upg_counts[8]`. Phases 3–4 fill the rest; they are declared now so **no further
  engine edit is needed for the whole project**. Occupies the storage slot the
  deleted `Experience` used, so it cost a rename, not a registration.
- **`Pickup`** (`player_components.hpp:54-58`) + `PickupKind` enum
  (`:45`) — `kind`, `value`, `magnet_speed`. The project's only new component
  registration. Registered across `component_storage.hpp:389,495-496,837`,
  `component_storage.cpp:715-717,746`, **`destruction.cpp:61`** (R1 — see §5).
- **Loot drops** (`enemy_death_system.cpp:22-64`, called from `:87-97`). Each kill
  rolls 1–3 currency pickups scattered around the corpse, each worth the enemy
  type's `currency`, plus a `key_drop_chance` roll for a shop key (D2). Pickups
  carry `Lifetime` so uncollected loot expires, and `RenderLayer{4}` so it draws
  above enemies(2) and the player(3).
- **R2 determinism discipline.** `drop_loot` draws a *fixed* number of RNG values
  per kill — `count`, `key_roll`, then `2 × max_drops` scatter values — and uses
  `count` only to decide which pre-rolled offsets get *used*
  (`enemy_death_system.cpp:53-59`). The drop block also runs **before** the
  explosion-sprite load, so a missing sidecar can't shift the sequence
  (`:87-89`). `EnemyDeathSystem::set_economy(cfg.economy, cfg.seed)`.
- **`PickupSystem`** (`pickup_system.{hpp,cpp}`) — magnet steering then a
  centre-distance collection test; credits `ShipState.currency`/`.keys`, spawns a
  particle pop, attaches `DestroyRequest`. Runs in `PHASE_PLAYING` at
  `main.cpp:479`, replacing the two deleted systems in the update order.
- **Magnet steering is written but inert** (`pickup_system.cpp:35,48-57`), gated on
  `ShipState.item_id == ITEM_MAGNET_CORE`, which nothing sets until Phase 4.
- **Deleted:** `experience_system.{hpp,cpp}`, `upgrade_system.{hpp,cpp}`, the
  `Experience` struct, the `Upgrade` struct + its parser, `GameConfig::xp_level2`
  /`xp_multiplier`, JSON `xp_curve` + `upgrades`, blackboard keys `level`,
  `pending_xp`, `pending_upgrades`.
- **`ContactDamage.xp` → `.currency`** (`player_components.hpp:71`) and
  `EnemyType.xp` → `.currency` (`arena_config.hpp:91`), JSON key with it. A rename,
  not new plumbing — the per-enemy values 1/2/4 were already authored.
- **`EconomyConfig`** (`arena_config.hpp:135-150`) + JSON `economy` block. Every
  drop-rate and price-adjacent number is data, never code (R6).
- **HUD:** the Level row became a yellow **Credits** row
  (`game_hud_system.cpp:30,60-67`); the key count appends only once a key has
  actually dropped. `upgrade_message`/`upgrade_message_timer` renamed to
  `hud_message`/`hud_message_timer` — the mechanism is kept as a generic transient
  message channel for Phase 3's shop, but the old name would have been a lie.
- **Headless summary line now prints Credits** (`main.cpp:602-612`), which is what
  makes a scripted run a usable R2/R6 canary rather than a score-only smoke test.
- **Four tests**: 3 unit (`tests/unit/test_arena_systems.cpp:48,76,115`) and 1
  property (`tests/property/test_arena_properties.cpp:91`), replacing the deleted
  XP/upgrade cases (R8).

## 3. What did NOT ship and why

- **Nothing spends currency.** No shop, no `PHASE_SHOP`, no upgrades — Phase 3.
  Credits accumulate to a number on the HUD and stop there. This is the intended
  end state of Phase 2, but it does mean the phase is *not* independently
  satisfying to play the way Phase 1 was.
- **No items or consumables** — Phase 4. `ShipState.item_id`/`consumable_id`/
  `buff_id`/`buff_timer` are declared and permanently `-1`/`0`.
- **Magnet Core steering is code, not a feature.** Written here because the
  distance math is already in `PickupSystem`, but unreachable until Phase 4 sets
  `item_id`. It is therefore **untested against a real equipped item** — the unit
  test only asserts the *negative* case (no drift without the item).
- **Pickups use a distance check, not the collision system.** Deliberate: a
  `Collider` needs a new layer bit, a mask edit and a `CollidedWith` sweep, while
  the magnet needs the raw distance anyway. Marked with a `ponytail:` comment at
  `pickup_system.hpp:21-25` naming the ceiling (O(pickups) per frame) and the
  upgrade path.
- **No playtest, again.** Headless only. The balance-feel log below still has no
  entry from a human playing the game, and the drop rate in §6 is a pure guess.
- **Phase 1's open items are still open:** arena themes still all activate by wave
  5, `victory_wave` is still 0, timed waves still have no enemy-count ceiling.

## 4. Files touched

| Path | What changed | Why |
|---|---|---|
| `CPP/engine/ecs/component_storage.hpp` | `experiences_` → `ship_states_`; added `pickups_` map, 2 `get_storage<Pickup>` decls, `CS110_EXTERN(ShipState)`/`(Pickup)` | Rename + the one new registration |
| `CPP/engine/ecs/component_storage.cpp` | Matching `get_storage` defs + `CS110_INSTANTIATE(ShipState)`/`(Pickup)` | Same |
| `CPP/engine/ecs/destruction.cpp` | `remove_component<Experience>` → `<ShipState>`, **added `<Pickup>`** | **R1** — the line that gets forgotten |
| `CPP/game/player_components.hpp` | `Experience` → `ShipState`; added `Pickup` + `PickupKind`; `ContactDamage.xp` → `.currency` | The phase's data model |
| `CPP/game/arena_config.hpp` | Deleted `Upgrade` + `xp_level2`/`xp_multiplier`; added `EconomyConfig`; `EnemyType.xp` → `.currency` | Economy is data (R6) |
| `CPP/game/arena_config.cpp` | Deleted `xp_curve`/`upgrades` parsing; added `economy` parsing | Same, all via `json.value(k, default)` |
| `CPP/game/enemy_death_system.{hpp,cpp}` | XP award → `drop_loot()`; `set_economy()`; seeded drop RNG | D5, R2 |
| `CPP/game/pickup_system.{hpp,cpp}` | **New** — collection + inert magnet steering | D5 |
| `CPP/game/game_hud_system.{hpp,cpp}` | Level row → Credits/Keys row; `upgrade_message` → `hud_message` | XP is gone |
| `CPP/game/main.cpp` | Dropped 2 systems + 3 blackboard keys, added `PickupSystem`; `ShipState{}` in `spawn_world`; credits in the shutdown line | Wiring |
| `CPP/game/wave_spawner_system.cpp` | `type.xp` → `type.currency` at the `ContactDamage` build site | Follows the rename |
| `CPP/game/experience_system.*`, `upgrade_system.*` | **Deleted** (4 files) | D1 |
| `CPP/game/tests/unit/test_arena_systems.cpp` | 2 XP/upgrade cases → 3 `[economy]` cases | R8 |
| `CPP/game/tests/property/test_arena_properties.cpp` | 2 XP/upgrade properties → 1 drop property | R8 |
| `assets/GameData.json` | Deleted `xp_curve`/`upgrades`; `xp` → `currency` ×3; added `economy` | D1, R6 |

## 5. New surface area

**Components — the registry changed. This is the part a cold agent cannot grep for.**

- **`ShipState`** — added to the player in `spawn_world()` (`main.cpp:310`). One
  per run, destroyed with the player. Read by `PickupSystem`, `GameHUDSystem`, and
  the shutdown summary. **Phase 3 writes to it from the shop; Phase 4 from items.**
- **`Pickup`** — the project's only new component type. Full registration list, in
  case another is ever added:
  1. `component_storage.hpp:389` — the member map
  2. `component_storage.hpp:495-496` — 2 `get_storage<T>` declarations
  3. `component_storage.hpp:837` — `CS110_EXTERN(T)`
  4. `component_storage.cpp:715-717` — 2 `get_storage<T>` definitions
  5. `component_storage.cpp:746` — `CS110_INSTANTIATE(T)`
  6. **`destruction.cpp:61` — `remove_component<T>`. Miss this and the component
     leaks onto recycled entity ids** (R1). There is now a regression test for
     exactly this: `test_arena_systems.cpp:115`.
- **`Experience` no longer exists.** Any stale reference to it is from before this
  phase.

**Blackboard keys — removed, not added:** `level`, `pending_xp`,
`pending_upgrades` are gone. `upgrade_message`/`upgrade_message_timer` were
**renamed** to `hud_message`/`hud_message_timer` (same semantics: set the string,
set the timer to N seconds, `main.cpp:526-529` ticks it down, the HUD shows it
while >0). Phase 3's shop should use that channel rather than inventing one.
Net new blackboard keys this phase: **none**.

**New systems and update order.** `PickupSystem` sits at `main.cpp:479`, in the
`PHASE_PLAYING` block, in the slot the two deleted systems occupied:

```
... player_damage → damage_apply → enemy_death → pickups → lifetime → animation → flash → destroy
                                   ^ drops loot   ^ collects it
```

Same-frame ordering matters: `enemy_death` spawns pickups and `pickups` runs
immediately after, so a pickup dropped onto a stationary player is collected on
the very next frame, not the next one after that.

**New JSON keys:** top-level `economy` block (8 keys, all optional);
`enemy_types[].currency` (was `xp`). **Removed:** `xp_curve`, `upgrades`.

**New public API:** `EnemyDeathSystem::set_economy(const EconomyConfig&, unsigned)`,
`PickupSystem::set_economy(const EconomyConfig&)`,
`PickupSystem::ITEM_MAGNET_CORE` (= 0 — Phase 4's item catalogue must agree).

## 6. Tuning values chosen

| Value | Where | Why |
|---|---|---|
| `min_drops` 1, `max_drops` 3 | `GameData.json` `economy` | Per plan. Multiple small drops read as a burst of loot; the same total in one pickup does not. Variance also makes a hulk kill feel different from a spark kill even before the value differs |
| enemy `currency` 1 / 2 / 4 | `enemy_types[].currency` | Unchanged from the old `xp` values — they were already a sane relative worth per type and the shop is not priced yet, so changing them now would be guessing twice |
| `key_drop_chance` 0.03 | `GameData.json` `economy` | ~3% over a 20-wave run with hundreds of kills is far too generous *if* keys stay this common — but D2 keys only matter between the fixed 4-wave stops, and nothing consumes them until Phase 3. **Expect to cut this to ~0.005 once the shop exists.** Flagged, not fixed |
| `pickup_lifetime` 12.0 s | `GameData.json` `economy` | Long enough to cross a 1400-radius arena at 260 px/s for loot you saw drop; short enough that a timed wave doesn't carpet the arena with 12 minutes of uncollected pickups |
| `pickup_size` 16.0 px | `GameData.json` `economy` | A quarter of a 64 px spark. Doubles as the collection radius basis (player r=20 + pickup r=8 = 28 px of grab range), which is forgiving without being a magnet |
| `pickup_scatter` 26.0 px | `GameData.json` `economy` | Inside a 64 px enemy's footprint, so drops land on the corpse rather than around it, and a player already at contact range collects most of them for free |
| `pickup_magnet_speed` 420, `pickup_magnet_radius` 220 | `GameData.json` `economy` | Speed is above the player's 260 px/s so loot catches up rather than trails; radius is ~3.4 arena-body-widths. **Both untested** — nothing equips the Magnet Core until Phase 4 |
| `RenderLayer{4}` on pickups | `enemy_death_system.cpp:50` | Above enemies(2) and the player(3). Loot hidden under a corpse is loot the player doesn't know to walk toward |

## 7. Known bugs & rough edges

- **The economy has no sink.** Credits only go up. This is the defining rough edge
  of the phase and Phase 3 is the fix.
- **Drop rate is unvalidated against prices that don't exist yet.** A scripted
  1800-frame run at seed 1234 gives 16 kills → 19 credits *collected* (see §10),
  but a stationary scripted player collects only what dies on top of it. A real
  player who moves toward loot will earn substantially more. Do not price the shop
  off that 19.
- **`key_drop_chance` 0.03 is almost certainly too high** — see §6. It is harmless
  today only because keys do nothing.
- **The stall watchdog now pays out currency too.** Phase 1 noted force-killed
  stragglers still pay score/XP; they now also drop loot. Same verdict — it only
  fires on a genuine stall — but the free-money surface grew.
- **Magnet steering has never run.** Gated on `item_id == 0`, which nothing sets.
  Phase 4 should treat it as new code, not as code that already works.
- **Pickups ignore obstacles and the arena wall.** They spawn wherever the enemy
  died, including inside an obstacle the player can't reach, and the magnet pulls
  them in a straight line through walls. A pickup stranded inside an obstacle is
  unreachable until its 12 s lifetime expires. Low stakes (loot is fungible and
  expires), but it will look wrong once someone notices.
- **Collection is O(players × pickups) per frame**, unsorted. Fine at tens of
  pickups; a timed wave 20 with 12 s lifetimes could hold a few hundred. Marked
  with the ceiling and upgrade path at `pickup_system.hpp:21-25`.
- **`ShipState` is deliberately a fat struct**, so unrelated features write to the
  same component. That is the trade for one registration instead of eight; it will
  feel wrong the first time two systems touch it in one frame.

## 8. Balance-feel log *(append-only)*

- **2026-07-28 — Phase 1, not yet played.** Headless verification only (no input in scripted
  runs, so score stays 0 by design). Every number in §6 is a first guess. Open questions for
  the first real play: is wave 6→7 a wall now that the arena must be cleared? Do 78 px hulks
  make the arena feel crowded at wave 20's spawn rate? Is 36 s of timed survival too long
  without a shop break in the middle?

- **2026-07-28 — Phase 2, still not played.** First scripted run that actually
  produces kills: seed 1234, 1800 frames, SPACE held → 16 kills, 160 score, **19
  credits collected**. The player never moves in a scripted run, so that 19 is a
  floor, not a rate — it counts only loot that landed within 28 px of a stationary
  drone. Seed 777 over the same run gives 145 score / 25 credits, so the spread
  between seeds is already ±30%.
  Open questions for the first real play, on top of Phase 1's: does walking to
  loot pull the player *into* enemies in a way that makes the economy a risk
  mechanic (good) or a tax (bad)? Are 1–3 pickups per kill visually legible at
  wave 15 spawn rates, or does the arena become a carpet of yellow squares? Is 12 s
  long enough to notice loot dropped across the arena during a timed wave?
  **Do not price the Phase 3 shop until someone has played one full run and
  reported an actual credits-per-wave curve.**

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

Added in Phase 1:

| # | Decision |
|---|---|
| D13 | Arena-clear gating applies to **all** waves, not just timed ones. Uniform rule, and it is what makes D4's "shop opens on an empty arena" hold for the wave-4 and wave-8 shop stops. |
| D14 | The stall watchdog force-kills via `Health = 0` rather than `DestroyRequest`, so stragglers go through `EnemyDeathSystem` like any other kill. One death path = Phase 2's currency drop needs no special case. |
| D15 | `wave_just_cleared()` is a plain getter cleared at the top of `update()`, not a consume-on-read. Read-only getters that mutate are a 3am bug; the cost is that callers must read it after `update()` in the same frame. |
| D16 | Enemies get `RenderLayer{2}` now (R5) rather than deferring to the Phase 5 visual pass — the sizes were being changed in this phase anyway, which is exactly what makes the bug visible. |

Added this phase:

| # | Decision |
|---|---|
| D17 | `ShipState` declares **all** of Phases 3–4's fields now, even though most stay at their defaults until then. Registering a component costs edits in 5 files plus the `destruction.cpp` line; paying that once for a fat struct beats paying it eight times for tidy ones. The cost is a component several unrelated systems write to — accepted, and named in §7. |
| D18 | `drop_loot` draws a **fixed** number of RNG values per kill and uses the count roll only to decide which pre-rolled offsets are *used* (R2). The obvious `for (i < count) draw()` makes the draw count outcome-dependent, which desynchronises every later roll and silently breaks `--seed` replay. The loop looks wasteful; that is the point. |
| D19 | Loot is dropped **before** the explosion sprite is loaded, so a missing/failed sidecar can't shift the RNG sequence. Same reasoning as D18: no draw may sit behind a conditional that isn't part of the game state. |
| D20 | Pickups are collected by a centre-distance test, **not** the collision system. A `Collider` needs a new layer bit, a mask edit and a `CollidedWith` sweep; the magnet needs the raw distance regardless. Ceiling and upgrade path are recorded at `pickup_system.hpp:21-25`. |
| D21 | `ContactDamage.xp` was **renamed** to `.currency` rather than deleted and re-added. The per-enemy 1/2/4 spread was already authored and already correct as a relative worth; a rename keeps that tuning instead of re-guessing it. |
| D22 | `upgrade_message`/`upgrade_message_timer` were renamed to `hud_message`/`hud_message_timer` and **kept**, not deleted with `UpgradeSystem`. The transient-message mechanism is generic and Phase 3 needs it; the old name would have referred to a system that no longer exists. |
| D23 | Magnet steering ships in Phase 2, inert, gated on `item_id == ITEM_MAGNET_CORE`. The distance math is already in `PickupSystem`, so writing it later means re-deriving it. It is explicitly listed as untested code in §3 and §7 so Phase 4 does not mistake it for working. |
| D24 | The headless shutdown line prints Credits alongside Score. A score-only summary can't detect an economy regression, and every later phase's balance work runs through this line. |

## 10. Verification

```
cd CPP/build && cmake . && cmake --build . -j     # 0 errors, 0 new warnings
ctest --output-on-failure                          # 8/8 passed (100%)
./game/tests/game_unit_tests "[economy]"           # 18 assertions in 3 cases, all passed
./game/tests/game_property_tests "[economy]"       # 269 assertions in 1 case, all passed
```

R2 canary — the same seed twice, with scripted fire so kills (and therefore drop
rolls) actually happen:

```
K=$(python3 -c "print(' '.join(f'{f}:SPACE' for f in range(1,1800)))")
SDL_VIDEODRIVER=dummy ./game/game --seed 1234 --stopframe 1800 --keys $K
  → Frames: 1800  Final score: 160  Credits: 19    (identical on both runs)
SDL_VIDEODRIVER=dummy ./game/game --seed  777 --stopframe 1800 --keys $K
  → Frames: 1800  Final score: 145  Credits: 25    (differs → the RNG is engaged)
```

The plain `--seed 1234 --stopframe 900` run from Phase 1 is also byte-identical
across two runs, but it produces **zero kills**, so on its own it does not test the
drop RNG at all — which is why the scripted-fire variant above exists.

`GameData.json` re-parsed: `xp_curve` and `upgrades` gone, `economy` present,
`enemy_types[].currency` = `[1, 2, 4]`.

R1 is covered by an assertion, not just a checklist: `test_arena_systems.cpp:115`
destroys an entity holding `Pickup` + `ShipState` and asserts the recycled id
carries neither.

Not run: an interactive play session. Still.

## 11. Phase 3 entry point

Open `CPP/game/main.cpp:76` and add `PHASE_SHOP = 4` to the `Phase` enum, then
find the `wave_spawner.update(...)` call in the `PHASE_PLAYING` block and read
`wave_spawner.wave_just_cleared()` **in the same frame, immediately after it**
(D15 — it is cleared at the top of the next `update()`). That edge plus
`wave % 4 == 0` is the shop entry condition; the `B`-key-with-a-key path is the
second entry.

Before writing any of the catalogue: the shop's prices are the one number in this
project that cannot be guessed from the code. §8 says it outright — **play one
full run first and write down the credits-per-wave curve**, then price against it.
Everything else in Phase 3 is mechanical: `ShipState` already has `upg_counts[8]`
for escalating prices, `hud_message`/`hud_message_timer` already exist for the
purchase feedback line, and `GameHUDSystem::init/update` is the pattern for
rendering numbered rows as `Text` + `ScreenPosition` entities. No new component
should be needed for the entire phase — if you reach for one, re-read D17 first.
