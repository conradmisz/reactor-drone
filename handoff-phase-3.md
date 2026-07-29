# Handoff — Gameplay Upgrade Phase 3 (Shop phase & permanent upgrades)

Plan: `/home/conrad/.claude/plans/i-will-provide-a-peppy-kahan.md` (§4 Phase 3).
Previous: `handoff-phase-2.md`.

## 1. Phase 3 summary

Give the currency a sink. A new `PHASE_SHOP` opens on every cleared wave that is a
multiple of 4, or on demand when the player spends a key (D2), and freezes the
arena while a numbered keyboard list (D11) sells six permanent, escalating-price
upgrades. Purchases write into the components the player already has — `Health`,
`WeaponStats`, `ShipState` — so no component was added (D17). Two of the six needed
supporting mechanics: shields (regen after a quiet delay, drained before hull) and
a live move-speed multiplier, which finally removes the ctor-captured `move_speed`
noted in the plan's §1.5. Prices are **provisional** — see §7 and §8.

## 2. What shipped

- **`PHASE_SHOP = 4`** (`main.cpp:78`) and its own branch in the phase machine
  (`main.cpp:566-575`). While it runs, nothing else does: no spawner, no movement,
  no damage, no pickups. Particles and the HUD keep updating (they run outside the
  phase switch), so the frozen arena still breathes.
- **Shop entry, both routes** (`main.cpp:450-453, 543-565`):
  - `wave_spawner.wave_just_cleared() && !all_complete() && current_wave_index() % 4 == 0`
    — read **immediately after `wave_spawner.update`** in the same frame (D15), then
    acted on after the frame simulates, so the transition happens at one place.
  - `B` with `ShipState.keys > 0`, which decrements the key.
  - Loss and victory outrank both, so a wave-20 clear is a victory, not a shop stop.
- **`ShopSystem`** (`shop_system.{hpp,cpp}`) — catalogue, prices, rendering,
  purchases. `open()` spawns Text + ScreenPosition rows (`shop_system.cpp:36-72`),
  `close()` marks them destroyed, `update()` handles one keypress and returns
  "player is done" (`:110-152`). `main.cpp` gets four calls and no shop logic (R7).
- **Escalating prices** — `price_for(index, already_bought)` =
  `price * price_growth^already_bought`, rounded (`shop_system.cpp:23-29`), read
  from `ShipState.upg_counts[index]` exactly as the plan intended.
- **Six upgrades**, effects dispatched by the JSON `effect` string, not by row index
  (`shop_system.cpp:154-190`):

  | Row | Effect | Writes |
  |---|---|---|
  | Hull Plating | `hull` | `Health.max_hp` **and** `.current` (it repairs too) |
  | Shield Capacitor | `shield` | `ShipState.shield_max/.shield_regen/.shield`, arrives charged |
  | Aux Thruster | `speed` | `ShipState.speed_mult` |
  | Overclock | `fire_rate` | `WeaponStats.fire_rate` |
  | Heavy Rounds | `damage` | `WeaponStats.damage` |
  | Twin Barrel | `extra_shot` | blackboard `ship.extra_shots` |

- **Shields.** `tick_shields()` (`shield_system.hpp:24-38`) counts down
  `ShipState.shield_delay` and only then refills at `shield_regen` per second;
  `PlayerDamageSystem` drains the shield before raising a `DamageEvent` and restarts
  the delay on *every* hit, absorbed or not (`player_damage_system.cpp:33-52`). A hit
  fully eaten by the shield still costs i-frames, trauma and a flash.
- **Live move speed.** `PlayerControlSystem::set_speed()`
  (`player_control_system.hpp:57`) — the one engine-side change this phase — called
  each playing frame with `move_speed * ShipState.speed_mult` (`main.cpp:436-442`).
- **Twin Barrel** fires `1 + ship.extra_shots` projectiles in a 0.09 rad fan around
  the aim angle, one spread jitter per volley (`player_fire_system.cpp:51-58`).
- **HUD:** a `Shield: N` suffix on the Health row, shown only once a capacitor is
  owned (`game_hud_system.cpp:57-63`), same rule as the key count.
- **Headless summary line now prints Wave and Phase** (`main.cpp:611-620`) — with a
  phase that can freeze the run, "the score stopped rising" was ambiguous between
  shopping, dying and stalling. It is what made §10's shop run verifiable at all.
- **`key_drop_chance` 0.03 → 0.005** (`GameData.json` `economy`), as flagged in
  Phase 2 §6 now that keys finally buy something.
- **Scripted keys `B` and `1`-`8`** (`cli_parser.cpp:34`, `main.cpp:381-390`) — the
  shop is otherwise unreachable from a headless run.
- **Two unit tests** (`tests/unit/test_arena_systems.cpp:177,213`): escalating price
  + max-stacks + insufficient-funds + effect application; shield soak, delayed regen,
  cap, and overflow-to-hull.

## 3. What did NOT ship and why

- **Prices are unvalidated guesses.** The whole point of §8's standing instruction
  was to price against a real credits-per-wave curve, and there still has not been a
  playtest. The catalogue is authored with placeholder numbers and a `//shop` comment
  in `GameData.json` saying so. **This is the first thing to fix, and it is a JSON-only
  edit** (R6). See §8 for what to measure.
- **No items, consumables, or timed buffs** — Phase 4. `ShipState.item_id`,
  `consumable_id`, `buff_id`, `buff_timer` are still permanently `-1`/`0`.
- **No upgrade visuals** — Phase 5. A fully kitted ship looks identical to a bare one;
  the only feedback that a purchase happened is the HUD numbers and the message line.
- **No shop backdrop panel.** The rows are text drawn over the frozen arena. The HUD
  renderer only draws `Text` + `ScreenPosition`; a screen-space rectangle is not a
  thing the render system supports, so a proper panel is Phase 6's job with the
  clickable cards.
- **No refunds, no re-roll, no "sell".** Not in the design.
- **Wave 20 never opens the shop** — clearing it is the victory condition, which
  outranks shop entry. Deliberate, but it means D2's "shop opens after wave 20" is
  effectively 4/8/12/16 only.
- **Phase 1 and 2's open items are still open:** arena themes all activate by wave 5,
  `victory_wave` is 0, timed waves have no enemy-count ceiling, pickups ignore
  obstacles, magnet steering has still never run.

## 4. Files touched

| Path | What changed | Why |
|---|---|---|
| `CPP/game/shop_system.{hpp,cpp}` | **New** — catalogue, escalating prices, row rendering, purchase dispatch | The phase (R7: all of it lives here) |
| `CPP/game/shield_system.hpp` | **New**, header-only `tick_shields()` | Shield Capacitor needs a regen tick |
| `CPP/game/arena_config.hpp` | Added `ShopUpgradeDef`, `ShopConfig`, `GameConfig::shop` | Catalogue is data (R6) |
| `CPP/game/arena_config.cpp` | Parse the `shop` block; clamp to 8 rows | `upg_counts` is 8 wide |
| `CPP/game/main.cpp` | `PHASE_SHOP`; shop branch; entry conditions; B/digit key edges; per-frame `set_speed`; `tick_shields`; `ship.*` blackboard keys; Wave/Phase in the summary line | Wiring only |
| `CPP/game/player_damage_system.cpp` | Shield drains before hull; any hit restarts the regen delay | Shield Capacitor |
| `CPP/game/player_fire_system.cpp` | Fan loop over `1 + ship.extra_shots` barrels | Twin Barrel |
| `CPP/game/game_hud_system.cpp` | `Shield: N` suffix on the Health row | Shields are invisible otherwise |
| `CPP/engine/ecs/systems/player_control_system.hpp` | `set_speed()` | `move_speed` was ctor-captured (plan §1.5/§1.11) |
| `CPP/game/cli_parser.cpp` | `B` and `1`-`8` are valid scripted key names | Headless shop verification |
| `assets/GameData.json` | Added `shop` block + `//shop` note; `key_drop_chance` 0.03 → 0.005 | Catalogue + the Phase 2 follow-up |
| `CPP/game/tests/unit/test_arena_systems.cpp` | 2 new `[shop]` cases + `shop_config()` helper | Cover purchases and shields |

## 5. New surface area

**Components: none added, none renamed.** Every purchase lands on `Health`,
`WeaponStats` or `ShipState`, all of which already existed (D17 held).

**New blackboard keys — two, both `ship.*`:**

- **`ship.shield_regen_delay`** (float) — set once from `config.shop.shield_regen_delay`
  at `main.cpp:134`. Read by `PlayerDamageSystem`, which copies it into
  `ShipState.shield_delay` on every hit. Config, not state.
- **`ship.extra_shots`** (int) — the Twin Barrel count, written by `ShopSystem::apply`
  and read by `PlayerFireSystem`. It is on the blackboard rather than in
  `ShipState` **and rather than a catalogue index** so the fire system never has to
  know which row Twin Barrel is. It is the one purchase that does not live on a
  component, so it is the one `spawn_world()` must clear by hand (`main.cpp:299`).

**New systems and update order.** Two, neither of which is a class with state:

```
PHASE_PLAYING:
  set_speed -> player_control -> player_aim -> wave_spawner [read wave_just_cleared HERE]
  -> arena swap -> enemy_seek -> movement -> clamp -> push_out -> player_fire
  -> collision -> projectile_hit -> tick_shields -> player_damage -> damage_apply
  -> enemy_death -> pickups -> lifetime -> animation -> flash -> destroy
                                          ^ new       ^ drains the shield first

PHASE_SHOP:
  shop.update -> (on leave) shop.close + back to PHASE_PLAYING -> animation -> destroy
```

`tick_shields` runs **before** `player_damage` on purpose: a hit this frame gets the
last word on the regen delay, so chip damage can never be free.

**New JSON keys:** top-level `shop` block — `price_growth`, `shield_regen_delay`,
`upgrades[]` of `{name, effect, price, amount, max_stacks}`. All optional; an absent
`shop` block yields an empty catalogue and a shop with no rows (it still opens and
still closes on `B`).

**New public API:** `ShopSystem::{set_config, open, close, update, is_open, price_for}`,
`tick_shields(ComponentStorage&, float)`, `PlayerControlSystem::set_speed(float)`.

**Input:** `B` opens (with a key) / closes the shop; `1`-`8` buy. Edge detection for
both lives in `main.cpp:396-406` beside the existing F1/F2/SPACE edges — `ShopSystem`
never touches SDL. **`SPACE` deliberately does not close the shop**: it is the fire
key, and a player holding it when a wave cleared would never see the shop at all.

## 6. Tuning values chosen

**Every price below is provisional.** They are listed so the next agent can see the
shape of the guess, not because any of them is defended.

| Value | Where | Why |
|---|---|---|
| `price_growth` 1.5 | `GameData.json` `shop` | Third purchase costs 2.25× the first; steep enough that spreading credits across rows beats stacking one, gentle enough that a favourite build is still affordable |
| Hull Plating 50 cr / +25 hp / max 8 | `shop.upgrades[0]` | Cheapest row: the one purchase that is never wrong. +25 on a 100 hp drone is a visible chunk; 8 stacks tops out at 300 hp |
| Shield Capacitor 90 cr / +30 / max 5 | `[1]` | Priced above hull because it also regenerates. Regen is **derived**, not a second number: `shield_regen = shield_max * 0.2`, so a full recharge is always ~5 s of not being hit regardless of stack count |
| Aux Thruster 70 cr / +0.12 / max 5 | `[2]` | +12% of 260 px/s per stack; 5 stacks = 416 px/s, which is fast but still slower than a 420 px/s magnet pull. Movement is the drone's only defence, so this is deliberately capped low |
| Overclock 70 cr / +0.6 fire rate / max 6 | `[3]` | 4.0 → 7.6 shots/s at full stack. Paired with Heavy Rounds this is where the damage curve is supposed to keep up with `hp_mult` 2.2 at wave 20 — **the single most likely number to be wrong** |
| Heavy Rounds 60 cr / +8 dmg / max 8 | `[4]` | Same +8 the deleted XP upgrade used, so the per-shot progression is at least the one number carried over from a system that shipped |
| Twin Barrel 220 cr / +1 barrel / max 2 | `[5]` | ~3× any other row: a second barrel is a flat ×2 on damage output and needs to be a run-defining purchase, not the obvious first buy |
| fan step 0.09 rad | `player_fire_system.cpp:56` | ~5° between barrels — reads as a widening beam at close range and still covers a 78 px hulk at across-the-arena distance |
| `shield_regen_delay` 3.0 s | `GameData.json` `shop` | Longer than the 0.8 s i-frame window, so shields cannot regen between two contacts with the same enemy; short enough to be full again after crossing the arena |
| shop opens every 4 waves | `main.cpp:452` (`% 4`) | D2. Hard-coded rather than JSON: it is a *design* cadence the wave table is written around, not a balance knob |
| `key_drop_chance` 0.005 | `GameData.json` `economy` | Down from 0.03 per Phase 2 §6. At ~30 kills/wave that is ~1 key every 6-7 waves — a bonus stop between the fixed ones, not a replacement for them |

## 7. Known bugs & rough edges

- **The prices are guesses and the game has still never been played.** Everything in
  §6 could be off by 3×. This is the phase's defining rough edge and it is *not* a
  code problem — see §8.
- **The shop has no visual frame.** Six lines of text over a frozen arena reads more
  like a debug overlay than a shop. Phase 6.
- **Row layout is hardcoded pixel offsets** (`shop_system.cpp:57-70`), inherited from
  `GameHUDSystem`'s approach. A long upgrade name or a 7th row will run off-centre.
  Marked with a `ponytail:` comment naming the ceiling.
- **A shop opened with `B` mid-wave freezes live enemies in place.** That is the
  intended freeze, but a player can open the shop with an enemy touching them and
  leave to an instant hit. Not exploitable in the player's favour, just ugly.
- **The key is spent on *entry*, not on a purchase.** Opening the shop with a key and
  immediately pressing `B` burns it for nothing. No confirmation step.
- **Nothing caps total upgrade stacks**, only per-row `max_stacks`. A long run with a
  generous drop rate could buy every row to max; whether that is a power fantasy or a
  broken curve is exactly what §8's playtest has to answer.
- **`ship.extra_shots` is a blackboard int, not `ShipState`** — the one purchase that
  does not live with the others, and the one that needs a manual reset in
  `spawn_world()`. Justified in §5 (D28), but it is an asymmetry to remember.
- **Shields absorb hazard damage too**, since hazards route through the same
  `ContactDamage` path. Intended, and worth knowing before Phase 4 tunes hazards.
- **A stationary headless drone dies around wave 3**, and the scripted-SPACE canary
  then *restarts the run* (SPACE is also "click to retry"), which is why the 1800-frame
  numbers below are stable while longer runs look non-monotonic. Not a determinism
  bug — but it means long scripted runs measure a fresh run, not a deep one.

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

- **2026-07-28 — Phase 3, shop shipped, still not played.** The instruction above
  was not met: no playtest happened, so the catalogue was authored against a
  guess and labelled as one in `GameData.json`. The scripted canary is unchanged
  from Phase 2 (seed 1234, 1800 frames, SPACE held → 160 score / 19 credits), and
  it cannot reach the shop: a stationary drone dies around wave 3, so the wave-4
  stop is out of reach. Shop entry, pricing and exit were verified instead against
  a **temporary throwaway config** (§10), which proves the mechanism and says
  nothing about balance.
  **What to measure on the first real play, in this order:**
  1. **Credits banked when the wave-4 shop opens.** That single number decides
     whether the 50-90 cr entry prices are right. If it is under ~120 the shop is a
     shrug; if it is over ~400 the first stop buys three upgrades and the curve is
     already broken.
  2. **Credits banked at the wave-8, -12 and -16 stops** — the *shape* matters more
     than any one value. If it is flat, `price_growth` 1.5 is too steep; if it
     doubles each stop, it is too shallow.
  3. **Whether Overclock + Heavy Rounds keep up with `hp_mult` 2.2 at wave 20.**
     If wave 16-20 is a wall with a full catalogue bought, the problem is the
     upgrade `amount`s, not the prices.
  4. **Whether the freeze feels like a break or an interruption**, especially the
     `B`-with-a-key stop mid-wave.
  Every fix for 1 and 2 is a `GameData.json` edit with no rebuild.

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

Added in Phase 2:

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

Added this phase:

| # | Decision |
|---|---|
| D25 | The shop is a **phase**, not a pause overlay. It reuses the existing `phase` int and the "run nothing else" property falls out for free; an overlay would have needed a freeze flag threaded through every system. Cost: while the shop is open the arena is genuinely frozen, including enemies opened onto with a key. |
| D26 | Upgrade effects dispatch on a JSON **`effect` string**, not on the row's index. Row order still selects the `upg_counts` slot, so re-ordering rows re-labels existing purchases — but it can never silently apply the *wrong effect*, which is the failure that would be invisible in a playtest. Ten lines of if/else buys that. |
| D27 | Shield regen is **derived** (`shield_regen = shield_max * 0.2`) rather than a second catalogue number. One number to balance instead of two, and full-recharge time stays constant across stack counts, which is the property a player actually feels. |
| D28 | The Twin Barrel count lives on the **blackboard** (`ship.extra_shots`), not in `ShipState` and not as a catalogue index read by `PlayerFireSystem`. `ShipState` has no free field for it and D17 forbids a new component; a shared index would couple the fire system to JSON row order, which D26 exists to avoid. The cost is one manual reset in `spawn_world()`, named in §7. |
| D29 | `tick_shields` is a **free function in a header**, not a `ShieldSystem` class. Six lines of countdown with nothing to own and nothing to configure. Phase 4's buff timer is the same shape and should go here; promote it to a real system only when that stops being true. |
| D30 | **`SPACE` does not close the shop**, only `B` does. SPACE is the fire key and also the "click to retry" advance; a player holding it when a wave cleared would open and close the shop in consecutive frames and never see it. |
| D31 | Shop entry is **`% 4` hard-coded in `main.cpp`**, not a JSON knob. It is a design cadence the 20-wave table was authored around (D2/D9), not a balance value; making it data would invite tuning the one number that the wave curve assumes. |
| D32 | A hit fully absorbed by the shield still costs **i-frames, trauma and a flash**, and still restarts the regen delay. Otherwise a shielded drone could stand inside an enemy taking free damage-less contact, and shields would read as invulnerability rather than a buffer. |
| D33 | The headless summary line gained **Wave and Phase** (extending D24). A phase that can freeze the run makes "the score stopped rising" ambiguous between shopping, dying and stalling; without this line the §10 shop verification would have been guesswork. |

## 10. Verification

```
cd CPP/build && cmake . && cmake --build . -j     # 0 errors, no new warnings
ctest --output-on-failure                          # 8/8 passed (100%)
./game/tests/game_unit_tests "[shop]"              # 26 assertions in 2 cases, all passed
./game/tests/game_unit_tests "[economy]"           # 18 assertions in 3 cases, all passed
./game/tests/game_property_tests                   # 82674 assertions in 32 cases, all passed
```

R2 canary — scripted fire so kills (and drop rolls) actually happen, same seed twice:

```
K=$(python3 -c "print(' '.join(f'{f}:SPACE' for f in range(1,1800)))")
SDL_VIDEODRIVER=dummy ./game/game --seed 1234 --stopframe 1800 --keys $K
  → Frames: 1800  Score: 160  Credits: 19  Wave: 3  Phase: 1   (identical on both runs)
SDL_VIDEODRIVER=dummy ./game/game --seed  777 --stopframe 1800 --keys $K
  → Frames: 1800  Score: 145  Credits: 25  Wave: 2  Phase: 1   (differs → RNG engaged)
```

Score and credits are **unchanged from Phase 2**, which is the result you want: the
shop is unreachable this early, so nothing about the existing economy moved.

**Shop entry / purchase / exit, against a temporary throwaway config** (1 hp enemies,
6 two-enemy waves, an unkillable drone) written over `assets/GameData.json` and then
restored from a backup:

```
frame 1100  → Wave: 4  Phase: 1  Credits: 1600     (wave 4 still spawning)
frame 1300  → Wave: 4  Phase: 4  Credits: 1800     (wave 4 cleared → shop opened)
frame 1500  → Wave: 4  Phase: 4  Credits: 1800     (frozen: score and credits static)

with --keys ... 1310:1 1320:1 1330:5 1400:B
frame 1350  → Phase: 4  Credits: 1615    (1800 − 50 − 75 − 60: escalating price, exactly)
frame 1500  → Phase: 1  Wave: 5          (B closed the shop and play resumed)

B pressed at frames 600/620 with keys = 0 → Phase: 1 (no entry; the key gate holds)
```

`GameData.json` re-parsed after restoring: 20 waves (9 timed), sizes `[64,70,78]`,
`currency` `[1,2,4]`, `key_drop_chance` 0.005, `shop.upgrades` = 6.

**Incident, recorded because it cost real work:** a `git checkout assets/GameData.json`
during this phase discarded Phase 1's and Phase 2's *uncommitted* JSON edits. They were
rebuilt from the tuning tables in `handoff-phase-1.md §6` and `handoff-phase-2.md §6`,
which recorded every value; the wave `types` arrays for waves 7-20 and the two `//`
comment strings were **not** recorded anywhere and were re-authored (all three enemy
types per wave, matching waves 5-6). Everything else is value-for-value what the
handoffs specify. **Commit early — the handoffs were the only backup, and they were
good enough only because §6 is a real table.**

Not run: an interactive play session. Three phases in a row now.

## 11. Phase 4 entry point

Open `CPP/game/shop_system.cpp:154` (`ShopSystem::apply`) — that if/else chain on
`def.effect` is where items and consumables get sold, and the pattern to copy. But
the two Phase 4 slots are *equipped*, not stacked, so they want their own catalogue
section rather than more `upgrades[]` rows: `ShipState.item_id` / `.consumable_id`
each hold exactly one id, and `PickupSystem::ITEM_MAGNET_CORE == 0` already fixes the
first item's id — the Phase 4 catalogue **must** agree with it.

The timed-buff mechanism (Overdrive, Phase Shift) is the only genuinely new machinery:
`ShipState.buff_id` + `buff_timer` ticked in `shield_system.hpp` next to
`tick_shields` (D29 — it is the same shape, one countdown on `ShipState`). Apply on
start, restore on expiry, do not stack.

Before any of that: **the prices**. §8 lists exactly what to measure on one full
play, and every fix is a `GameData.json` edit. Phase 4 adds more things to buy on top
of a catalogue nobody has confirmed is affordable — pricing items against unvalidated
upgrade prices compounds the guess.
