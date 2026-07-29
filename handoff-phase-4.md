# Handoff — Gameplay Upgrade Phase 4 (Items & consumables)

Plan: `/home/conrad/.claude/plans/i-will-provide-a-peppy-kahan.md` (§4 Phase 4).
Previous: `handoff-phase-3.md`.

## 1. Phase 4 summary

Fill the two equipment slots `ShipState` has been carrying empty since Phase 2.
One passive item and one held consumable, both bought on a new second page of
the existing shop (`TAB` flips between UPGRADES and GEAR, so the 1-8 keys keep
covering everything). Three of the four items are one-liners bolted onto the
system that already owns their moment — Magnet Core and Salvager in
`PickupSystem`, Reactive Plating in `PlayerDamageSystem` — which left the
Repulsor Field and the four consumables as the only genuinely new code. The
timed-buff mechanism the plan called out turned out to need almost nothing: only
Overdrive uses it, and it is *read* where it matters rather than written and
restored (D34). No component was added or changed (D17 still holds). Prices are
**provisional and deliberately uniform** — see §3 and §8.

## 2. What shipped

- **Equipment ids as code constants** (`player_components.hpp:44-60`):
  `item_ids::{MAGNET_CORE=0, REPULSOR_FIELD, REACTIVE_PLATING, SALVAGER}` and
  `consumable_ids::{REPAIR_KIT=0, OVERDRIVE, EMP_BURST, PHASE_SHIFT}`.
  `PickupSystem::ITEM_MAGNET_CORE` now *aliases* `item_ids::MAGNET_CORE`
  (`pickup_system.hpp:31-33`) instead of being an independent `0`, so the
  agreement the Phase 3 handoff demanded is enforced by the compiler rather than
  by a comment. `buff_id` reuses the consumable ids (D35).
- **`item_system.hpp`** — new, header-only, `namespace items`:
  - `item_id_for(effect)` / `consumable_id_for(effect)` — the catalogue's
    `effect` string mapped onto those constants (D26 extended to gear).
  - `repulse_enemies(...)` — the Repulsor Field.
  - `use_consumable(...)` — the single "spend Q" entry point, dispatching on the
    catalogue row's `effect` string.
- **The four items:**

  | Item | Where it lives | What it does |
  |---|---|---|
  | Magnet Core | `pickup_system.cpp:34` | Activates Phase 2's dormant magnet steering — **first time this code has ever run** |
  | Repulsor Field | `item_system.hpp:87-116`, called at `main.cpp:541` | Pushes enemies inside `shop.repulsor_radius` outward at the item's `amount` px/s |
  | Reactive Plating | `player_damage_system.cpp:48-56` | Spawns a `DamageEvent` back at whatever rammed the drone |
  | Salvager | `pickup_system.cpp:34-40, 66-72` | Every currency pickup is worth `max(value+1, round(value * amount))` |

- **The four consumables** (`item_system.hpp:124-160`), one `Q` press each:
  Repair Kit (heal, capped at `max_hp`), Overdrive (arms the buff), EMP Burst
  (a `DamageEvent` on every living `EnemyTag`), Phase Shift (writes
  `player.iframes` — no buff machinery at all, `PlayerDamageSystem` was already
  ticking that key down every frame).
- **The timed buff** — `tick_buff()` (`shield_system.hpp:38-56`), a free function
  beside `tick_shields` exactly as D29 asked, called at `main.cpp:557`. Expiry is
  "clear the id"; `PlayerFireSystem` scales `fire_rate` while the buff is live
  (`player_fire_system.cpp:31-39`) rather than the buff writing into
  `WeaponStats` (D34). Re-using a consumable resets the timer — no stacking,
  which falls out of there being one slot.
- **Shop gear page** (`shop_system.cpp`): `TAB` toggles `page_`; page 1 lists 4
  items then 4 consumables on keys 1-8, flat-priced, no escalation, marked
  `EQUIPPED` when held. Buying replaces the slot with no refund. A third header
  row shows both equipped slots on both pages. Rows are allocated for the
  *widest* page at `open()`, so a page flip only rewrites text (`:58-73`).
- **HUD gear line** (`game_hud_system.cpp:31, 96-116`) under Credits: equipped
  item name, `[Q] <consumable>`, and a live `OVERDRIVE 5.3s` countdown. Blank
  when nothing is equipped, same no-noise rule as the shield/key readouts.
- **Catalogue in `GameData.json`** — `shop.items[]`, `shop.consumables[]`,
  `shop.repulsor_radius`, plus a `//gear` note recording that the prices are
  guesses and why they are uniform.
- **`TAB` and `Q` are scripted keys** (`cli_parser.cpp:37-38`), which is what made
  §10's headless gear verification possible.
- **Five unit tests** (`tests/unit/test_arena_systems.cpp`, tags `[items]` /
  `[consumables]`): equip/replace/afford on the gear page, magnet pull + salvage
  arithmetic, repulsor rim clamp, plating reflect through a full shield, and all
  four consumables including buff expiry.

## 3. What did NOT ship and why

- **Prices are still guesses, and now there are eight more of them.** No full run
  has been played, so the item/consumable prices are anchored to the *upgrade*
  catalogue — itself unvalidated. They are deliberately **uniform** (120 per
  item, 45 per consumable) so that when the real curve arrives there are two
  numbers to move, not eight. §8 lists what to measure, unchanged from Phase 3
  and now with the gear questions added.
- **No upgrade or item visuals** — Phase 5, as planned. A drone with a Repulsor
  Field looks identical to a bare one; the field has no ring, the magnet has no
  tether, Overdrive has no muzzle change. Right now the HUD text line is the
  *only* indication any of this is equipped.
- **No shop backdrop panel, no clickable cards** — Phase 6, unchanged.
- **No second item/consumable slot, no inventory, no sell/refund.** Not in the
  design (D6/D7), and the "one slot" constraint is what keeps the whole feature
  down to two `int`s on an existing component.
- **Consumables cannot be used from inside the shop.** `Q` is only read in
  `PHASE_PLAYING`. Using a Repair Kit while shopping would be the obvious thing
  to want; it is not wired, and it is a one-line change if a playtest asks for it.
- **Magnet Core does not pull the key drop differently from credits** — one
  magnet speed for all loot, carried per-pickup on `Pickup.magnet_speed` from
  `economy.pickup_magnet_speed`.
- **Phase 1-3's open items are all still open:** arena themes all activate by
  wave 5, `victory_wave` is 0, timed waves have no enemy-count ceiling, pickups
  ignore obstacles, wave 20 never opens the shop.

## 4. Files touched

| Path | What changed | Why |
|---|---|---|
| `CPP/game/item_system.hpp` | **New** — id maps, `repulse_enemies`, `use_consumable` | The two effects with no existing home (R7) |
| `CPP/game/shield_system.hpp` | Added `tick_buff()` | D29: the buff is the same shape as the shield countdown |
| `CPP/game/player_components.hpp` | Added `item_ids` / `consumable_ids` | Fixed ids so `item_id` is never a JSON row index |
| `CPP/game/pickup_system.hpp` | `ITEM_MAGNET_CORE` aliases `item_ids::MAGNET_CORE` | Single source of truth for the id |
| `CPP/game/pickup_system.cpp` | Salvager multiplier at the credit site | D6 |
| `CPP/game/player_damage_system.cpp` | Reactive Plating reflect | D6 |
| `CPP/game/player_fire_system.cpp` | Overdrive scales the fire rate while buffed | D34 |
| `CPP/game/shop_system.{hpp,cpp}` | `page_`, gear rows, `buy_gear`, `equip`, slot header | The gear catalogue needed somewhere to be sold |
| `CPP/game/game_hud_system.{hpp,cpp}` | New gear/buff line | Equipment is otherwise invisible (no visuals until Phase 5) |
| `CPP/game/main.cpp` | `TAB`/`Q` edges; `repulse_enemies`; `use_consumable`; `tick_buff`; `tab_edge` into `shop.update`; four blackboard resets in `spawn_world` | Wiring only |
| `CPP/game/arena_config.{hpp,cpp}` | `ShopUpgradeDef.duration`; `ShopConfig::items/consumables/repulsor_radius`; one shared row parser | Catalogue is data (R6) |
| `CPP/game/cli_parser.cpp` | `TAB`, `Q` are valid scripted keys | Headless gear verification |
| `assets/GameData.json` | `shop.items[]`, `shop.consumables[]`, `repulsor_radius`, `//gear` note | The catalogue |
| `CPP/game/tests/unit/test_arena_systems.cpp` | 5 new cases + `gear_config()` helper | Cover every item and consumable |

## 5. New surface area

**Components: none added, none renamed.** `ShipState.item_id`,
`.consumable_id`, `.buff_id` and `.buff_timer` — declared empty in Phase 2 (D17)
— are now written. D17 has now paid for itself twice.

**New blackboard keys — four, all `ship.*`, all reset in `spawn_world`
(`main.cpp:301-306`):**

- **`ship.item_amount`** (float) — the *one* number the *one* equipped item
  needs, written by `ShopSystem::equip`. Read as a salvage multiplier by
  `PickupSystem`, as reflect damage by `PlayerDamageSystem`, and as push speed by
  `repulse_enemies`. Only one item is equipped at a time, so one key covers all
  of them; the reader always checks `item_id` first, so a stale value from a
  replaced item can never be applied.
- **`ship.buff_mult`** (float) — Overdrive's fire-rate multiplier, written by
  `use_consumable`, read by `PlayerFireSystem` only while
  `buff_id == consumable_ids::OVERDRIVE`.
- **`ship.item_name`**, **`ship.consumable_name`** (string) — display only, so
  `GameHUDSystem` needs no catalogue pointer. `ShopSystem` renders its own header
  from `cfg_` directly and does not read these.

**New systems and update order.** Two more free functions, no new classes:

```
PHASE_PLAYING:
  set_speed -> player_control -> player_aim -> wave_spawner [read wave_just_cleared HERE]
  -> arena swap -> enemy_seek -> movement -> clamp -> push_out
  -> repulse_enemies -> use_consumable(Q)                        <- new, both
  -> player_fire -> collision -> projectile_hit
  -> tick_shields -> tick_buff -> player_damage -> damage_apply   <- tick_buff new
  -> enemy_death -> pickups -> lifetime -> animation -> flash -> destroy

PHASE_SHOP:
  shop.update(digit, B, TAB) -> (on leave) shop.close + back to PHASE_PLAYING
```

`repulse_enemies` runs **after** the arena clamp and the obstacle push-out, so
"solid wall" keeps the last word on where an enemy ends up; a shoved enemy is
re-clamped on the next frame. `tick_buff` sits with `tick_shields` before
`player_damage` for no reason other than that they are the same kind of thing —
nothing this phase depends on the ordering.

**New JSON keys:** `shop.items[]`, `shop.consumables[]` (both
`{name, effect, price, amount, duration}` — `max_stacks` is parsed but ignored
for gear), and `shop.repulsor_radius`. All optional; an absent block yields an
empty gear page that still opens, flips and closes.

**New public API:** `items::{item_id_for, consumable_id_for, consumable_def,
ship_of, repulse_enemies, use_consumable}`, `tick_buff(ComponentStorage&, float)`,
and a fifth defaulted `toggle_page` parameter on `ShopSystem::update`.

**Input:** `TAB` flips the shop page (shop only), `Q` spends the consumable
(playing only). Edge-detected in `main.cpp:419-426` beside the existing edges.

## 6. Tuning values chosen

**Every price below is provisional and every one is uniform on purpose.** With
one slot each, the choice between items should be about the effect, not the cost;
uniformity also means the first playtest moves two numbers instead of eight.

| Value | Where | Why |
|---|---|---|
| Items 120 cr each | `shop.items[*].price` | Between Heavy Rounds (60) and Twin Barrel (220): affordable at the wave-8 stop if the wave-4 stop was spent on upgrades, so the first item is a real decision and not a formality |
| Consumables 45 cr each | `shop.consumables[*].price` | Under the cheapest upgrade (50), so restocking one every stop is never the wrong call — a consumable you did not use is a wasted slot, not a wasted run |
| `repulsor_radius` 140 px | `shop.repulsor_radius` | ~2.7 drone widths. Wide enough to be a bubble you can feel, narrow enough that a 78 px hulk still gets inside contact range |
| Repulsor push 35 px/s | `items[1].amount` | **Under the slowest enemy's 45 px/s on purpose.** A push at or above it would hold hulks at the rim forever, and D4's arena-clear gate would soft-lock the wave (R3 all over again). Net approach speeds become spark 60, runner 35, hulk 10 |
| Reactive Plating 25 dmg | `items[2].amount` | One ram kills a base 20 hp spark outright and leaves a hulk at 65. Fires on *contact*, not on hull loss, so it still works behind a full shield — the plating reacts to being rammed |
| Salvager x1.5 | `items[3].amount` | Small int drop values (1/2/4) mean a bare x1.25 rounds the common 1-credit drop straight back to 1. 1.5 plus a `+1` floor gives 1→2, 2→3, 4→6: always visibly better, never a no-op |
| Repair Kit 60 hp | `consumables[0].amount` | 60% of a bare drone's 100 hp; still meaningful at 300 hp with 8 Hull Platings, without being a full heal |
| Overdrive x2 for 8 s | `consumables[1].amount/duration` | The plan's number, kept. 8 s at 4.0 base is ~32 extra shots — one bad moment's worth, not a wave's worth |
| EMP Burst 45 dmg | `consumables[2].amount` | Clears sparks outright *even at wave 20's `hp_mult` 2.2* (20 x 2.2 = 44 < 45). Panic button that reliably thins the swarm without touching hulks |
| Phase Shift 3 s | `consumables[3].duration` | ~3.75x the 0.8 s i-frame window: long enough to walk out of a pile-up, short enough that it is an escape and not a free wave |

## 7. Known bugs & rough edges

- **The prices are still guesses, on top of guesses.** Same defining rough edge
  as Phase 3, now compounded — see §3 and §8.
- **Nothing on the ship shows what is equipped.** The HUD text line is it. This
  is exactly what Phase 5 exists for, but until then a Repulsor Field pushing
  enemies away has no visual cause, which reads as a physics bug rather than an
  item.
- **Buying a replacement item silently bins the old one, no refund, no
  confirmation** — same shape as Phase 3's "the key is spent on entry" edge.
- **`Q` in the shop does nothing, with no feedback.** Not even a "can't use that
  here" message.
- **`TAB` in the shop consumes the frame**, so a `TAB` and a digit pressed on the
  same frame flips the page and drops the purchase. Deliberate (it keeps "the row
  you saw is the row you bought" true) but it will feel like a dropped input to a
  fast player.
- **The Repulsor Field push is unconditional on enemy type.** A wave-20 hulk with
  `speed_mult` above 1.0 shrugs it off almost entirely, which may make the item
  feel dead exactly when it should matter most. This is the item most likely to
  need its number moved after a playtest.
- **Reactive Plating's reflect can kill**, which means it can advance a wave and
  drop loot — worth knowing before anyone tries to score a "no shots fired" run.
- **EMP Burst ignores `armor_multiplier` ordering subtleties**: it raises one
  `DamageEvent` per enemy and `DamageApplySystem` applies armour as usual. Fine
  today because nothing sets a non-1.0 armour multiplier.
- **The buff HUD hardcodes the string `OVERDRIVE`** rather than looking the name
  up, because there is exactly one buff. A second buff makes that wrong.
- **A stationary headless drone still dies around wave 3**, so the scripted
  canary still cannot reach the shop on the real config — §10's gear run needs
  the throwaway-config trick, unchanged from Phase 3.

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

- **2026-07-28 — Phase 4, gear shipped, still not played.** Four phases now. The
  canary is *byte-identical* to Phase 3's (seed 1234, 1800 frames, SPACE held →
  160 score / 19 credits, seed 777 → 145 / 25), which is the intended result:
  Phase 4 adds **no RNG draws at all**, so `--seed` replays are unmoved. Item and
  consumable prices were authored uniform (120 / 45) precisely so they can be
  re-tuned as two numbers once §8's Phase 3 measurements exist.
  **The four numbers still needed from one real play**, in priority order —
  everything below is a `GameData.json` edit with no rebuild:
  1. **Credits banked at the wave-4 shop.** Prices the whole catalogue.
  2. **Credits banked at the wave-8, -12 and -16 stops.** The shape, not the value.
  3. **Total credits earned across waves 1-8 vs waves 9-16.** This is the new one
     Phase 4 needs: it decides whether a 120 cr item competes with upgrades or is
     simply skipped, and whether a 45 cr consumable is a per-stop habit or a
     luxury.
  4. **Whether the run ends with unspent credits.** Unspent credits at wave 20
     mean everything is priced too low and the shop stopped being a decision.
  **Phase 4 questions on top of that**: does the Repulsor Field's 35 px/s read as
  a bubble or as nothing (§7 flags it as the likeliest dead number)? Does the
  Salvager compound so hard that it is the only correct first item? Is one
  consumable per shop stop enough to be worth the 45 cr, or does it get forgotten
  and expire unused because there is no visual reminder beyond one HUD line?

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

Added in Phase 3:

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

Added this phase:

| # | Decision |
|---|---|
| D34 | Overdrive is **read** by `PlayerFireSystem` while the buff is live, not applied to `WeaponStats` on use and restored on expiry. The plan called for apply/restore; that pattern breaks the moment anything *else* edits the same stat during the buff — buying Overclock in a shop opened with a key mid-Overdrive would leave `fire_rate` permanently halved on expiry. Reading has no restore step to get wrong. It also deletes the "restore" half of the machinery entirely, which is why the whole timed-buff feature is nine lines. |
| D35 | **`buff_id` reuses the consumable ids** rather than getting its own enum. Overdrive is the only timed buff, so a parallel `buff_ids` enum would have exactly one entry and two things to keep in sync. If a second buff ever comes from something other than a consumable, split it then. |
| D36 | Item and consumable ids are **code constants** (`item_ids`, `consumable_ids`), and the catalogue maps `effect` string → constant. This is D26 taken one step further: `ShipState.item_id` persists across a shop visit, so a JSON re-order under a row-index scheme would silently change which item a player *already owns*. `PickupSystem::ITEM_MAGNET_CORE` now aliases the constant, so the Phase 3 handoff's "the catalogue must agree with it" is enforced by the compiler. |
| D37 | The Repulsor Field is a **soft push** (px/s, clamped at the rim), not `push_circle_out_of_aabb` as the plan sketched. A hard eject would put a floor under enemy distance and make contact damage impossible — the item would read as invulnerability, and D32's reasoning about shields applies twice as hard here. The push is also tuned *below* the slowest enemy speed on purpose so nothing can be held at the rim forever, which would stall D4's arena-clear gate (R3). |
| D38 | Phase Shift writes **`player.iframes` directly** and arms no buff. `PlayerDamageSystem` has ticked that key down every frame since v2 Phase 4; a 3 s i-frame window *is* the effect. Routing it through `buff_id` would have added a second mechanism that does the same thing worse. |
| D39 | The gear catalogue is a **second shop page** (`TAB`), not more `upgrades[]` rows. Fourteen rows do not fit on the 1-8 keys, `upg_counts` is 8 wide, and items/consumables are *equipped* rather than stacked — they have no escalating price and no purchase count, so they do not belong in the same list. A page flip is one key and zero new input machinery. |
| D40 | The shop allocates rows for the **widest page** at `open()` and a page flip only rewrites text. Destroying and respawning `Text` entities on every `TAB` would work, but it puts entity churn on an input event for no gain; unused rows render as empty strings. |
| D41 | **One blackboard key, `ship.item_amount`, serves all four items.** Only one item is equipped at a time and every reader checks `item_id` before using the value, so per-item keys would be four names for one live number. Same reasoning that put `ship.extra_shots` on the blackboard (D28). |
| D42 | Salvager multiplies with a **`+1` floor** (`max(value+1, round(value*mult))`). Drop values are small ints (1/2/4) and the plan's ×1.25 rounds the common 1-credit drop back to 1 — an item that visibly does nothing on the most frequent pickup in the game. The floor costs eight characters and removes an entire class of "is this thing even working?" playtest confusion. |
| D43 | Reactive Plating fires on **contact**, not on hull damage, so it works behind a full shield. The plating is reacting to being *rammed*; gating it on hull loss would make it silently stop working exactly when a player has stacked shields, which is the least discoverable failure mode available. |

## 10. Verification

```
cd CPP/build && cmake . && cmake --build . -j     # 0 errors, no new warnings
ctest --output-on-failure                          # 8/8 passed (100%)
./game/tests/game_unit_tests "[items],[consumables]"   # 42 assertions in 5 cases, all passed
```

R2 canary — scripted fire so kills (and drop rolls) actually happen, same seed twice:

```
K=$(python3 -c "print(' '.join(f'{f}:SPACE' for f in range(1,1800)))")
SDL_VIDEODRIVER=dummy ./game/game --seed 1234 --stopframe 1800 --keys $K
  → Frames: 1800  Score: 160  Credits: 19  Wave: 3  Phase: 1   (identical on both runs)
SDL_VIDEODRIVER=dummy ./game/game --seed  777 --stopframe 1800 --keys $K
  → Frames: 1800  Score: 145  Credits: 25  Wave: 2  Phase: 1   (differs → RNG engaged)
```

**Byte-identical to Phase 3's numbers**, which is the result you want twice over:
Phase 4 adds no RNG draws, so replays are unmoved, and it changes nothing about
the existing economy.

**Gear page / item / consumable, against a temporary throwaway config** (1 hp
enemies, zero contact damage, 2-enemy waves, 200 cr per drop, a 4000 px pickup
radius so a stationary drone banks everything, `wave_stall_timeout` 1 s) written
over `assets/GameData.json` and then restored from a backup:

```
--keys 5:SPACE                          → frame 500: Wave 4  Phase 4  Credits 3600
                                          (wave-4 clear opened the shop, arena frozen)

--keys 5:SPACE 510:TAB 520:4 530:6 540:B
  frame 535 → Phase 4  Credits 3435     (3600 − 120 Salvager − 45 Overdrive, exactly:
                                         flat gear prices, no escalation, and digit 4
                                         on page 1 bought an *item*, not Overclock)
  frame 545 → Phase 1  Wave 5           (B launched)
  frame 700 → Credits 4635

control, same script without the Salvager purchase (510:TAB 530:6 540:B)
  frame 700 → Credits 4355
  → loot banked over waves 5-6: 1200 with Salvager vs 800 without. Exactly ×1.5.
```

EMP Burst, proving `Q` is wired end to end (buy row 7 on the gear page, launch,
press Q mid-wave):

```
--keys 5:SPACE 510:TAB 530:7 540:B          → frame 605: Score 115  Wave 5
--keys 5:SPACE 510:TAB 530:7 540:B 600:Q    → frame 605: Score 140  Wave 6
```

One `Q` wiped the live wave and advanced the run — the consumable path from SDL
key edge through `use_consumable` to `DamageEvent` to `DamageApplySystem` works
in the real loop, not just in the unit test.

`GameData.json` re-parsed after restoring: 20 waves (9 timed), sizes
`[64,70,78]`, hp `[20,40,90]`, currency `[1,2,4]`, `wave_stall_timeout` 30,
`pickup_size` 16, `key_drop_chance` 0.005, `shop.upgrades` = 6,
`shop.items` = 4, `shop.consumables` = 4, `repulsor_radius` 140. `git diff` on
the file shows **only** the 15-line gear addition.

Not run: an interactive play session. Four phases in a row now.

## 11. Phase 5 entry point

Open `main.cpp:329-345` — the player's thruster `ParticleEmitter`, a 180°
omnidirectional aura. Phase 5's first job is turning it into a directional cone
opposite the aim angle, which is simultaneously the Aux Thruster's upgrade visual
and the fix for "the drone has no visible exhaust".

Everything Phase 4 shipped is currently **invisible** (§7): a Repulsor Field
pushing enemies away has no ring, an equipped Magnet Core has no tether, and
Overdrive changes nothing on screen. Emitters are ordinary entities and need no
new component type, so the per-item props the plan asks for are pure addition —
read `item_ids` in `player_components.hpp:44-52` for what needs a prop.

**Watch `particle_system.hpp DEFAULT_MAX_PARTICLES` (2000).** It truncates
*silently*, and Phase 4 added the pickup pop and (soon) four item effects on top
of bigger enemies and timed waves. Measure the live count before adding emitters.

Before any of that: **the prices, now eight more of them**. §8's Phase 4 entry
lists the four numbers one real play would produce and exactly what each one
decides. Every fix is a `GameData.json` edit with no rebuild.
