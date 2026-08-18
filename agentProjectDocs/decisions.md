# Decisions Log

Append-only. Never rewrite or delete an entry — if a decision is reversed, add
a new one that supersedes it.

**Numbering:** decisions carry stable ids (`D1`…) because code comments,
handoffs and plans cite them. The next free id is **D221** (D152-D179 are also free — see the
D138-D151 heading below). Continue the
sequence; do not renumber.

**Format for new entries:**

```
### D50 — [Short title]  *(YYYY-MM-DD)*
- **Decision:** what was decided
- **Why:** constraints and trade-offs
- **Rejected:** what else was considered and why it lost
- **Supersedes:** Dnn, if any
```

Entries D1–D43 were seeded from `handoff-phase-4.md` §9 (which itself carried
them forward from the gameplay plan); D44–D49 were reconstructed from
`ENGINE.md` §5 and `HANDOFF.md` on 2026-08-09. Both remain valid sources — this
file is now the one that gets appended to.

---

## 2026-07-09 → 2026-07-10 — v2 visual overhaul (Phases 0-4, committed)

- **Phase 0** brought v2 up from the 110 final project as its own repo, not a
  branch — v2 is free to break the graded submission's shape.
- **Phase 1** built the procedural asset pipeline (`assets/generator/v2/`,
  Pillow + stdlib synth). **Generators run offline and their output is
  committed**, so a fresh clone never needs Pillow and the build stays hermetic.
- **Phase 2** added `Tint` (rgba + `additive`), additive blend, `RenderLayer`
  bucketing and `Rotation::flip_when_left` to `RenderSystem`, rather than a
  second renderer.
- **Phase 3** ported a particle system with a hard `DEFAULT_MAX_PARTICLES` 2000
  budget that truncates silently — a bounded global cap was chosen over
  per-emitter budgets.
- **Phase 4** made hit feedback pure math in `feedback.hpp` (trauma decay, shake
  amplitude, flash tint) so the *feel* code is unit- and property-testable and
  the shake stays seeded and replay-safe.

## 2026-07-26 → 2026-07-27 — Arena/feel upgrade (Phases 1-5, committed)

- Aim and movement feel pass; big arena with a follow camera instead of a fixed
  screen; a visible boundary wall so the clamp is legible.
- Terrain became **solid sprite obstacles** with collision, not decoration.
- **One window-size authority.** Window/logical size resolves in a single place;
  every consumer reads it rather than re-deriving it.

## 2026-07-28 — Gameplay Phases 1-4 (committed): 20-wave arc, economy, shop, gear

Decisions seeded from the gameplay plan (D1–D12) and made during Phases 1-4
(D13–D43).

### Plan-level (D1–D12)

| # | Decision |
|---|---|
| D1 | The shop **replaces** the XP auto-upgrade system — one economy, not two. |
| D2 | The shop opens every 4 waves (4, 8, 12, 16, 20), plus a rare key drop for on-demand entry. |
| D3 | Per-run only. No persistence, no save file. *(Later revisited — see D49.)* |
| D4 | Timed waves end when the timer expires **and** the arena is clear. |
| D5 | Physical pickups — dead enemies drop collectibles you walk over. |
| D6 | Items (1 slot, passive): Magnet Core, Repulsor Field, Reactive Plating, Salvager. |
| D7 | Consumables (1 slot, one-use): Repair Kit, Overdrive, EMP Burst, Phase Shift. |
| D8 | Enemy sizes ×1.5 → 64 / 70 / 78 px, so a mouse-aimed drone can actually hit them. |
| D9 | Waves 7-11 hand-authored fixed count; 12-20 timed. |
| D10 | No new enemy types — scale the existing 3 with per-wave `hp_mult` / `speed_mult`. |
| D11 | The shop UI is a numbered keyboard list first; clickable cards deferred. |
| D12 | A phase handoff = full schema + balance-feel log + design-decision log, carried forward. |

### Phase 1 — waves (D13–D16)

| # | Decision |
|---|---|
| D13 | Arena-clear gating applies to **all** waves, not just timed ones. Uniform rule, and it is what makes D4's "the shop opens on an empty arena" hold. |
| D14 | The stall watchdog force-kills via `Health = 0`, not `DestroyRequest`, so stragglers go through `EnemyDeathSystem` like any other kill — one death path, so the currency drop needs no special case. |
| D15 | `wave_just_cleared()` is a plain getter cleared at the top of the next `update()`, not consume-on-read. Read-only getters that mutate are a 3am bug; the cost is that callers must read it in the same frame. |
| D16 | Enemies get `RenderLayer{2}` now rather than deferring to the visual pass — at 78 px, drawing behind the walls reads as a bug. |

### Phase 2 — economy (D17–D24)

| # | Decision |
|---|---|
| D17 | `ShipState` declares **all** of Phases 3-4's fields immediately, most defaulted. Registering a component costs edits in 5 files plus a `destruction.cpp` line; paying that once for a fat struct beats paying it eight times for tidy ones. Cost: one component several unrelated systems write to. |
| D18 | `drop_loot` draws a **fixed** number of RNG values per kill and uses the count roll only to pick which pre-rolled offsets are *used*. The obvious `for (i < count) draw()` makes the draw count outcome-dependent and silently breaks `--seed` replay. The loop looks wasteful; that is the point. |
| D19 | Loot drops **before** the explosion sprite loads, so a missing sidecar cannot shift the RNG sequence. |
| D20 | Pickups are collected by a centre-distance test, **not** the collision system — a `Collider` would need a new layer bit, a mask edit and a `CollidedWith` sweep, and the magnet needs raw distance anyway. |
| D21 | `ContactDamage.xp` was **renamed** to `.currency`, not deleted and re-added — the per-enemy 1/2/4 spread was already correct as a relative worth. |
| D22 | `upgrade_message*` was renamed to `hud_message*` and **kept**. The transient-message mechanism is generic; the old name referred to a deleted system. |
| D23 | Magnet steering ships inert in Phase 2, gated on `item_id == MAGNET_CORE`, because the distance math is already there — and is explicitly listed as untested so Phase 4 does not mistake it for working. |
| D24 | The headless shutdown line prints Credits alongside Score. A score-only summary cannot detect an economy regression. |

### Phase 3 — shop (D25–D33)

| # | Decision |
|---|---|
| D25 | The shop is a **phase**, not a pause overlay. "Run nothing else" falls out for free; an overlay needs a freeze flag threaded through every system. Cost: the arena is genuinely frozen, including a key-opened mid-wave stop. |
| D26 | Upgrade effects dispatch on a JSON **`effect` string**, not a row index. Re-ordering rows can re-label existing purchases, but can never silently apply the *wrong effect* — the failure that would be invisible in a playtest. |
| D27 | Shield regen is **derived** (`shield_max * 0.2`), not a second catalogue number. One number to balance, and full-recharge time stays constant across stacks — the property a player feels. |
| D28 | Twin Barrel's count lives on the **blackboard** (`ship.extra_shots`). `ShipState` has no free field, D17 forbids a new component, and a shared index would couple the fire system to JSON row order. Cost: one manual reset in `spawn_world()`. |
| D29 | `tick_shields` is a **free function in a header**, not a `ShieldSystem` class. Six lines of countdown with nothing to own. Promote it when that stops being true. |
| D30 | **`SPACE` does not close the shop**, only `B` does. SPACE fires and also advances; a player holding it on a wave clear would open and close the shop in consecutive frames and never see it. |
| D31 | Shop entry is **`% 4` hard-coded in `main.cpp`**, not a JSON knob. It is a design cadence the 20-wave table was authored around, not a balance value. |
| D32 | A hit fully absorbed by the shield still costs **i-frames, trauma and a flash**, and restarts the regen delay — otherwise shields read as invulnerability rather than a buffer. |
| D33 | The headless summary line gained **Wave and Phase**. A phase that can freeze the run makes "the score stopped rising" ambiguous between shopping, dying and stalling. |

### Phase 4 — gear (D34–D43)

| # | Decision |
|---|---|
| D34 | Overdrive is **read** by `PlayerFireSystem` while live, not applied to `WeaponStats` and restored on expiry. Apply/restore breaks the moment anything else edits the same stat during the buff (buying Overclock mid-Overdrive would halve `fire_rate` permanently). Reading has no restore step to get wrong — the whole timed-buff feature is nine lines. |
| D35 | **`buff_id` reuses the consumable ids.** Overdrive is the only timed buff; a parallel enum would have one entry and two things to keep in sync. |
| D36 | Item/consumable ids are **code constants**, and the catalogue maps `effect` string → constant. `ShipState.item_id` persists across a shop visit, so a row-index scheme would silently change which item a player *already owns*. `PickupSystem::ITEM_MAGNET_CORE` aliases the constant, so agreement is compiler-enforced. |
| D37 | The Repulsor Field is a **soft push** (px/s, clamped at the rim), not a hard eject. A hard eject would floor enemy distance and make contact damage impossible. It is tuned *below* the slowest enemy speed so nothing can be held at the rim forever, which would stall D4's arena-clear gate. |
| D38 | Phase Shift writes **`player.iframes` directly** and arms no buff — a 3 s i-frame window *is* the effect, and the key is already ticked down every frame. |
| D39 | Gear is a **second shop page** (`TAB`), not more `upgrades[]` rows. Fourteen rows do not fit on 1-8, `upg_counts` is 8 wide, and equipped gear has no escalating price or purchase count. |
| D40 | The shop allocates rows for the **widest page** at `open()`; a page flip only rewrites text. Entity churn on an input event buys nothing. |
| D41 | **One blackboard key, `ship.item_amount`, serves all four items** — one item is equipped at a time and every reader checks `item_id` first. |
| D42 | Salvager multiplies with a **`+1` floor**. ×1.25 rounds the common 1-credit drop back to 1 — an item that visibly does nothing on the most frequent pickup in the game. |
| D43 | Reactive Plating fires on **contact**, not on hull damage, so it works behind a full shield. Gating it on hull loss would stop it working exactly when a player has stacked shields. |

## 2026-07-31 → 2026-08-09 — Visual Phase 5 and the UI/menu layer (uncommitted)

### D44 — Enemy sprites are pure luminance, tinted at spawn  *(2026-07-31)*
- **Decision:** enemy art is generated against the `MONO` palette; colour comes
  from `ArenaDef::enemy_tint` via SDL colour-mod at spawn (`Color` records it,
  `Tint` applies it).
- **Why:** an arena colour baked into the art made enemies vanish into their own
  backdrop — the bug this fixed. One sprite set now works in every arena, and
  Prism's per-frame hue cycle becomes possible at all.
- **Rejected:** four sprite sets, one per arena — four times the art and the
  same bug the moment a fifth arena appears.

### D45 — `ParticleSystem::update` takes an `emit` flag  *(2026-07-31)*
- **Decision:** particles **age** in every simulated phase but only **spawn**
  while `PHASE_PLAYING`.
- **Why:** `lifetime.update` does not run in the shop, so one-shot FX hosts
  never expired and kept emitting — pinned at the 2000 cap indefinitely with the
  shop open. Ageing everywhere also lets trails finish on the title/game-over
  screens.
- **Rejected:** running `lifetime.update` in the shop branch — a smaller diff
  and the wrong fix; it would also expire the player's uncollected loot while
  they shop.

### D46 — The UI/menu layer is ported from Option-040, not written  *(2026-08-09)*
- **Decision:** copy `ui_style`, `ui_focus_math`, `ui_fade_math`,
  `ui_render_math`, `ui_system`, `ui_render_system`, `screen_stack_system`,
  `screen_fade_system` and their whole test suite into `CPP/engine/`.
- **Why:** a tested, already-working widget layer for the cost of an import.
- **Cost, accepted:** Class-040 did not enforce the zero-warning gate, so two
  test files needed fixes on import (a tautological `uint8_t <= 255` check and a
  `-Wdangling-reference`). Expect the same on any further port.

### D47 — Menu clicks are read off the Blackboard, not Lua  *(2026-08-09)*
- **Decision:** `UISystem` publishes a confirmed click's `on_click_fn` under
  `UISystem::UI_CLICK_KEY` as well as dispatching to Lua; `main.cpp` reads that
  key and consumes it. The `ui.*` Lua bindings stay ported, tested and inert.
- **Why:** this game has no menu scripts, and an empty Lua state resolves every
  callback to nil — inert, not broken. `LuaManager` is constructed purely to
  satisfy `UISystem`'s constructor. Adding `menu_callbacks.lua` later needs no
  engine change.

### D48 — Screens are authored as data in an 800×600 design canvas  *(2026-08-09)*
- **Decision:** `ui_styles` and `screens` are optional top-level `GameData.json`
  blocks; rects are authored in a fixed 800×600 canvas that
  `ui_canvas_transform` letterboxes onto the 980×660 logical surface.
- **Why:** both `UIRenderSystem` and `UISystem` apply the same transform, so the
  drawn rect and the clickable rect cannot drift apart, and a data file without
  the blocks creates zero UI entities and raises no error.
- **Related:** `UIElement::pulse_hz` is driven by a render-local `elapsed_`, not
  a Blackboard key — a game system must not be able to observe a presentation
  clock, or a replay could diverge on render state. `pulse_hz == 0` returns an
  exactly-`1.0f` multiplier, so non-pulsing widgets are byte-identical.

### D49 — The wave break is a prompt, and save/load is run-state only  *(2026-08-09)*
- **Decision:** a cleared 4th wave pushes the `wave_intermission` screen and
  enters `PHASE_INTERMISSION` (SHOP OPEN / NEXT WAVE) instead of auto-opening
  the shop. Planned persistence is a flat `RunState` struct saved to
  `saves/slot_N.json`, autosaved at the intermission edge.
- **Why:** the shop stop was an interruption the player never chose; the
  intermission is the one point where the world is provably quiescent, which
  makes it the only safe autosave point too.
- **Rejected:** a world snapshot (`SerializationRegistry` /
  `component_save_table` / `LoadSystem` from Option-030) — restoring a run's
  *stats* is a struct; restoring its *entities* is a serialization framework.
- **Supersedes:** D3 (per-run only, no save file).

### D50 — Difficulty is multipliers over one wave table, chosen per run  *(2026-08-09)*
- **Decision:** a `difficulties` block in `GameData.json` lists named
  `DifficultyDef`s (count / spawn-interval / hp / speed / currency /
  hazard-damage multipliers plus `type_lookahead`). `apply_difficulty` scales a
  pristine copy of the loaded `GameConfig` at run start; the `main_menu` screen
  picks the index. Index 0 is the default and `SPACE` selects it.
- **Why:** the same reasoning as D10 — a second authored wave table per
  difficulty is two tables to keep in sync and one to forget. `type_lookahead`
  covers the one non-scalar the user asked for ("enemy types unlock earlier")
  by merging each wave's roster with the next N, so no per-difficulty `types`
  list has to exist.
- **Also:** every field is enemy-side. The user's answer was explicit that Hard
  must **not** make the player's economy harsher, so no player/shop/economy
  field is scalable, and a unit test pins that.
- **Trap:** `apply_difficulty` is not idempotent. `main.cpp` keeps a `const
  GameConfig base_config` and re-copies it on every run start; scaling `config`
  in place would compound Hard onto Hard on a second run.
- **Rejected:** a difficulty component / Blackboard multiplier read per spawn —
  it would put a branch in the spawn path for a value that cannot change
  mid-run.
- **Balance:** Normal's early waves were rebalanced to the user's "aggressive"
  target (wave 1: 12 enemies at 0.45 s, both early types, +25% enemy speed) and
  Hard sits at ×1.5 count / ×0.7 spacing / ×1.3 hp / ×1.15 speed / ×1.4 credits
  / ×1.5 hazard damage / 2 waves of lookahead. **Provisional — unplayed.**

### D51 — Iteration 3 is scaffolded once, then built in parallel lanes  *(2026-08-09)*
- **Decision:** the 13 Game-Notes items are built by several agents at once, so a
  single serial phase lands **every shared-file edit up front** — both new
  component types (`EnemyShot`, `EnemyBehavior`), the `ENEMY_SHOT` collision
  layer, the new `ShipState` fields, the `PickupKind` additions, the config
  structs + their parse, and nine comment-delimited `// === HOOK: <lane> ===`
  blocks in `main.cpp`. After it, each lane edits only its own new files, its one
  hook, and its one `GameData.json` block.
- **Why:** the shared files are the whole risk. `ComponentStorage` needs a
  storage member, two `get_storage` specialisations, a `CS110_EXTERN`, a
  `CS110_INSTANTIATE` and a `destruction.cpp` sweep per type; `main.cpp` is 1200
  lines that every feature wants to touch. Parallel agents editing those means
  conflicts at best and a silently dropped registration at worst. Paying the cost
  once, serially, is cheaper than five merges.
- **Also:** every scaffolded default is deliberately **inert**
  (`sustain.interval 0`, `minimap.enabled false`, `actives []`, no wave flagged
  `boss`, no enemy type carrying a `behavior`), so Phase 0 could be verified by
  the replay canary staying byte-identical. `test_scaffolding.cpp` asserts the
  inertness, and each lane deletes the line it turns on.
- **Enemy shots need no damage system.** They carry `ContactDamage` and a layer
  the player's mask accepts, and `PlayerDamageSystem` already hurts the drone for
  anything carrying `ContactDamage` — the same path hazards use.
- **Rejected:** one component per behaviour (spitter/miner/bulwark/boss). That is
  four more registrations in three shared files for data that is never present on
  the same entity twice; `EnemyBehavior.kind` is one int.
- **Rejected:** letting each lane add its own component type and CMake entry when
  it needs one. That is precisely the merge conflict this phase exists to avoid.

### D52 — Coin/pickup despawn stays; the plan's "coins never despawn" is reversed  *(2026-08-09)*
- **Decision:** `economy.pickup_lifetime` stays **12.0**. Uncollected drops keep
  expiring on their own `Lifetime`, and there is **no** intermission sweep.
- **Why:** the iteration-3 plan lists "#8 coins never despawn" as a locked-in
  interview answer. The user reversed it afterwards: *"actually, i like that
  money despawns after a while. it adds some risk/reward to deciding to get it or
  not."* Walking into a firefight to reach a coin before it fades is a real
  decision; an immortal pile of credits is not.
- **Consequence:** the intermission block in `main.cpp` still ticks
  `lifetime.update` on purpose — that comment is load-bearing, not incidental.
- **Read this before the plan.** Anyone re-reading
  `plans/create-a-plan-to-polymorphic-gosling.md` will find the stale locked-in
  answer and its Phase 1 bullet. This entry supersedes both.

### D53 — The 50-wave table is generated from a formula, not hand-authored  *(2026-08-09)*
- **Decision:** waves 1-50 come from a linear ramp (#13): 1-25 fixed-count
  (10 → 45 enemies, spawn_interval 0.50 → 0.308, hp_mult 1.0 → 1.48), 26-50 timed
  (duration 20 → 39.2 s, spawn_interval 0.30 → 0.204, hp_mult 1.5 → 2.364,
  speed_mult 1.0 → 1.288). Waves 10/20/30/40/50 carry `boss: true`.
- **Why:** monotone pressure is then true *by construction* rather than by
  proofreading 50 hand-typed rows, and `test_wave_arc.cpp` can assert it. The
  ramp is deliberately flatter than the old 20-wave table — wave 1 drops from 12
  @0.45 s to 10 @0.50 s — because that table front-loaded mob count against a
  shop the player could not yet afford.
- **Arenas cycle twice**, `first_wave` 1/7/13/20/26/32/38/45. The second pass
  reuses every image, tint and backdrop and changes only the layout and
  `specialty_tier` (1 → 2), so 50 waves cost zero new art.
- **Second-pass layouts are PROVISIONAL and mechanical:** each pass-2 obstacle
  and hazard is its pass-1 twin rotated 90° about the arena centre (1600,1600)
  with w/h swapped. In-bounds by construction, recognisably the same theme,
  genuinely different cover. Chosen over hand-designing four arenas blind — the
  game has still never been played past wave ~8.
- **Rejected:** a `victory_wave` change. The HUD reads `total_waves` from
  `waves.size()`, so the "20-wave cap" was never a cap; `victory_wave` stays 0.

### D54 — Shield regen rate is data (`shop.shield_regen_frac`)  *(2026-08-09)*
- **Decision:** `s.shield_regen = s.shield_max * cfg_->shield_regen_frac`
  (0.08 → ~12 s for a full bank) replaces the hardcoded `* 0.2f` in
  `shop_system.cpp`, and `shield_regen_delay` goes 3.0 → 5.0 s.
- **Why:** #12. At 0.2/s with a 3 s delay, shields fully refilled in 5 s of
  disengagement, which made Shield Capacitor strictly the best buy and chip
  damage meaningless. 12 s + 5 s makes backing off a commitment.
- **Read off `ShopSystem::cfg_`**, which the system already holds — no new
  Blackboard key and no second `main.cpp` edit competing with another lane. The
  default lives in `ShopConfig`, so a data file without the key behaves the new
  way rather than the old way.

### D55 — Full shop every 5th wave, superseding D31's `% 4`  *(2026-08-09)*
- **Decision:** `main.cpp`'s shop trigger becomes
  `current_wave_index() % 5 == 0`. **This supersedes D31.**
- **Why:** #4, and it follows from D53. Over 50 waves `% 4` is 12 stops; `% 5` is
  10, one per boss cycle, so a shop stop always lands two waves before a boss.
- **Still hardcoded, still not a JSON knob** — D31's reasoning holds: the cadence
  is a design rhythm the wave table is authored around, not a balance number.
- **Note for the reader:** `progress-tracker.md`'s "temporarily change the `% 4`
  shop trigger to reach a late wave" tip now means `% 5` → `% 1`.

### D61 — The shop is a data-authored screen, not a second UI mechanism  *(2026-08-09)*
- **Decision:** the clickable shop is one `shop` entry in `GameData.json` →
  `screens`, 19 widgets in the 800×600 design canvas. `ShopSystem` resolves them
  by name through `ui.widget_id.<name>` (the `GameHUDSystem` pattern) and reads
  confirmed clicks off `UISystem::UI_CLICK_KEY`, removing the key after handling
  (the pattern `main.cpp` already uses for the intermission and the pause menu).
  One `menu_tick()` call from the `// === HOOK: shop-menu ===` slot; `main.cpp`
  gets a call, not logic (R7).
- **Why:** the user's note was *"the shop isnt a true menu, its just a text
  overlay"* — a presentation complaint. Every mechanism it needs already exists;
  inventing a second widget-resolution or click-dispatch path would have been
  the expensive way to say the same thing.
- **Card labels are rewritten per frame, rects are authored.** The JSON owns
  layout and style; the system owns text. An unused card is hidden by collapsing
  its rect to zero and disabling it — `UIElement` has no visibility flag, and
  adding one is an engine change for a menu that does not need it.
- **Rejected:** keeping the numbered `Text` rows alongside the cards. They would
  overdraw the panel. `menu_build()` destroys them but leaves `open_` true, so
  the rows are the *fallback* for a data file with no `shop` screen — the shop
  still works exactly as before if the block is missing, which is what makes the
  screen safe to hand to a data editor.
- **The `1`-`8` / `TAB` / `B` keyboard path is untouched**, so every existing
  headless script and test still drives the shop.

### D62 — Gear levels apply to the fitted item only  *(2026-08-09)*
- **Decision:** `ShipState.gear_levels[i]` indexes `shop.items`. Only the
  currently fitted item can be levelled; price is `base * price_growth^level`,
  and a level scales the item's published `ship.item_amount` by +25% per level.
  An item with `amount == 0` (Magnet Core, a boolean effect) is refused rather
  than sold a level that scales zero.
- **Why:** `ship.item_amount` is the one number every item consumer already
  reads — repulsor push, reactive reflect, salvage multiplier (D28/D41). A level
  lands on it with no new component, no new key and no consumer change.
- **Rejected: levelling consumables.** Nothing reads a per-run consumable
  amount — `items::use_consumable` takes it straight off the catalogue row — so
  a consumable level would be a number with no effect until `item_system.hpp`
  grows a Blackboard override. That file belongs to no lane in iteration 3;
  `gear_levels[4..7]` are left free for whoever wires it.
- **Rejected: a `shop.gear_amount_step` JSON knob.** The brief was explicit that
  the catalogue data does not change, and an untuned second growth number is a
  knob with no reader. `GEAR_AMOUNT_STEP` carries a `ponytail:` comment naming
  the promotion path.
- **Reusing `price_growth`** rather than a second curve keeps a levelled item
  escalating exactly like a stacked upgrade, which is the price intuition the
  player already has.

### D63 — A screen-locked sprite needs a `Position`, not a `ScreenPosition`  *(2026-08-09)*
- **Decision:** the drone preview is two ordinary world entities (glow disc +
  the player's own sprite) on `RenderLayer` 6/7, whose `Position` is rewritten
  every shop frame by inverting the camera transform, so they land on a fixed
  point in the 800×600 design canvas.
- **Why the plan's recipe does not work:** the iteration-3 plan says a preview is
  a `ScreenPosition` + `Images` entity with **no** `Position`. It would never
  draw. `RenderSystem::render` iterates `entities_with_component<Position>()` and
  only *prefers* `ScreenPosition` for the coordinate once an entity is in that
  list; `HUDSystem` renders `Text` only. The no-`Position` trick is correct for
  the HUD text (a different renderer) and is what a minimap blip must NOT do, but
  a sprite has to carry a `Position` to be seen at all.
- **So the constraint inverts:** `CameraSystem` overwrites `ScreenPosition` from
  `Position` every frame, so the only screen-locked recipe that needs no engine
  change is to compute the world point that maps to the wanted screen point and
  let `CameraSystem` re-derive it. That is `place_on_screen()`.
- **Known ceiling:** the preview is positioned before `camera.lookat` is updated
  later in the frame, so a *moving* camera would smear it by one frame. The arena
  is frozen in `PHASE_SHOP`, so this only shows as a sub-frame wobble while
  screen-shake decays into the shop.
- **Rejected:** an `Images` field on `UIElement`. That is an engine change, a
  component field, and a `UIRenderSystem` branch, for one sprite in one menu.

### D64 — Tooltips are one repositioned label pair, not a widget kind  *(2026-08-09)*
- **Decision:** hovering a card rewrites `shop_tip_name` / `shop_tip_desc` and
  moves the `shop_tip_panel` to sit beside that card. Nothing hovered collapses
  the panel rect to zero and blanks both labels.
- **Why:** `UIState.hovered` is already a pure function of the pointer, written
  by `UISystem` every frame, and `UIElement.rect` / `label_text` are plain
  mutable fields the HUD gauges already rewrite. A `tooltip` element type would
  be an engine change to say the same thing.
- **Collapsed, not blank:** a `panel` with empty text still fills its rect, so
  hiding means zeroing the rect. The labels skip drawing on empty text already.
- **The tooltip column sits right of the card panel**, never over it — an
  overlapping tooltip would sit between the pointer and the card it describes.

### D65 — The UPGRADE page lives in the shop, and the intermission cadence is not this lane's  *(2026-08-09)*
- **Decision:** gear levels are a third page of the shop screen (`UPGRADES` /
  `GEAR` / `LEVELS` tabs), not a separate screen reachable from the intermission.
- **Why:** the plan asked for an UPGRADE panel at *every* intermission with the
  SHOP button only on the 5th-wave stop, but today `main.cpp` raises the
  intermission **only** on that same `% 5 == 0` stop (D55, Lane A) — so "every
  intermission" and "every 5th wave" are currently the same moment. Splitting the
  panel out would have meant editing Lane A's cadence line and the
  `wave_intermission` screen, both outside this lane's file ownership.
- **Consequence for the integrator:** when the intermission is raised every wave,
  the LEVELS page already exists and needs no further work here — only a button
  on the intermission screen that enters `PHASE_SHOP` with `page_ == 2`.

### D80 — Persistence is one number, not a run snapshot  *(2026-08-09)*
- **Decision:** `saves/meta.json` holds a single field, `lifetime_score`. It is
  read once at startup (`meta_load`) and written once per run end (`meta_write`)
  — death, victory, quit-from-pause, or closing the window mid-run, all routed
  through one `bank_run_score` lambda guarded by a `run_banked` flag so a run is
  never counted twice.
- **Why:** the ask was "persistent score storing", not "resume my run". The
  older plan's run-state save (entity snapshot, `SerializationRegistry`,
  `LoadSystem`) is an order of magnitude more code and every bit of it is a
  determinism hazard. One number needs no registry and cannot half-restore.
- **Tolerant by construction:** a missing file, an empty file, malformed JSON, a
  wrong-typed field and a negative total all yield the default. A save file the
  player edited must never be able to stop a run from starting.
- **Rejected:** atomic temp-file + rename. The file is one integer written once
  per run; a torn write costs the player one run's progress and the corrupt-file
  path already recovers from it. Add it if the file ever grows.

### D81 — Unlocks are derived from lifetime score, never stored  *(2026-08-09)*
- **Decision:** `ship_unlocked(ship, lifetime) == lifetime >= ship.unlock_score`.
  The save file records no unlock list.
- **Why:** two records of the same fact desync. Retuning the 4000 threshold in
  `GameData.json` takes effect immediately for every player, and no save file can
  claim a ship the data no longer grants (or lose one it does).
- **Cost, accepted:** lowering a threshold retroactively unlocks; raising one
  retroactively re-locks. For a single-player arcade score that is the intuitive
  behaviour, not a bug.

### D82 — A ship is a `PlayerConfig` overlay, applied where difficulty is  *(2026-08-09)*
- **Decision:** `ShipDef` (name, sidecar, idle_clip, weapon, unlock_score) in
  `arena_config.hpp` with a `ships` block in `GameData.json`; `apply_ship`
  overlays it onto `PlayerConfig` inside `start_run`, immediately after the
  pristine `base_config` is re-copied and before `apply_difficulty`.
- **Why:** a ship differs from another ship in exactly the fields `PlayerConfig`
  already has. No ship component, no ship system, no second application site —
  `apply_ship` is as non-idempotent as `apply_difficulty` and follows the same
  discipline (D50). `ShipState::ship_id` records the choice for the HUD and
  tests; the stats are already baked into `config` by the time the player spawns.
- **Determinism:** the meta-save influences only what the title menu *offers*.
  The chosen ship is deliberately not persisted, so the replay canary for a given
  seed is byte-identical whether or not `saves/meta.json` exists — verified with
  and without the file.
- **UI:** one widget, `menu_ship`, is both selector and lock readout — it reads
  "SHIP: <name> (click to change)" once more than one ship is unlocked, and
  "<name> unlocks at 4000 pts (lifetime N)" while only one is. Cycling walks only
  unlocked ships (`next_unlocked_ship`), so a locked hull is unreachable rather
  than refused. Resolved by name through `ui.widget_id.<name>`, the HUD's
  existing pattern.
- **Rejected:** hiding a second widget when locked — `UIElement` has no visibility
  flag, and adding one is an engine change for a label that can simply say
  something else.
- **Numbers (provisional, unplayed):** Standard 4.0/s x 20 = 80 DPS; Purple
  Gatling 12.0/s x 6 = 72 DPS — 3x cadence for 90% of the sustained damage, the
  10% being the price of a far more forgiving weapon. Spread 0.06 -> 0.12,
  projectile lifetime 1.0 -> 0.8s to bound the on-screen projectile count.

### D83 — The purple ship is purple in name only, for now  *(2026-08-09)*
- **Decision:** the Purple Gatling reuses `player_drone.json`. No tint, no new
  sprite, this change.
- **Why:** a persistent `Tint` on the player does not survive a hit —
  `FlashSystem` *removes* the Tint when a flash expires for anything that is not
  an `EnemyTag` with a `Color`, so the hull would turn purple until first damage
  and then stay standard. Making it stick means editing `flash_system.cpp`
  (outside this lane's file ownership), and generating real purple art means
  `assets/generator/v2/`, which needs Pillow — not installed here, and generators
  never run at build time.
- **Next step:** either a `player_drone_purple` sidecar from the offline
  generator (then it is a one-line `ships[1].sidecar` edit, no rebuild), or give
  `FlashSystem::base_tint` a per-entity resting tint so the player can own one.

---

## Iteration 3 — Lane E: arena transition VFX (#2)

### D76 — One easing curve, in a header, not two lambdas  *(2026-08-09)*
- **Decision:** the backdrop crossfade's `smoothstep` moved out of the render
  block's lambda into `CPP/game/arena_vfx.hpp`, and the prop animation uses the
  same function.
- **Why:** the props animate *against* the backdrop fade. Two copies of the same
  cubic drift the moment either is tuned, and the drift is exactly the "looks
  like a glitch" failure the 1.2s → 5.0s stretch was meant to fix. The helper
  also clamps, which the lambda did not — its callers had to remember
  `std::min(1.0f, ...)`.
- **Rejected:** a second smoothstep next to the props. The instruction to reuse
  was explicit, and it would have been the same six characters of work.

### D77 — Colliders die on the shift frame; pixels die five seconds later  *(2026-08-09)*
- **Decision:** `clear_arena_props`' instant destruction is replaced, for the
  shift path only, by `arena_vfx::teardown_props`: strip `Collider` first and
  unconditionally, then attach a debris emitter and a `Lifetime`, then shrink
  across the window. The initial-snap path still clears instantly.
- **Why:** the two halves of "it is still there" have different owners. The
  simulation must stop seeing a prop the instant the arena is no longer that
  arena — otherwise a crumbling pillar blocks a shot, a dash or an A* path for
  five seconds. The renderer should keep seeing it for exactly that long. Doing
  both at one moment is what made the old behaviour a pop; doing both lazily
  would be a gameplay bug. Verified live: a forced shift shows the prop collider
  count drop to 0 on the shift frame while 121 props keep drawing.
- **`ContactDamage` is left on hazards** rather than removed. With no `Collider`
  it can never produce a `CollidedWith`, so it cannot fire — and
  `remove_component<ContactDamage>` is not instantiated, which would have meant
  editing `component_storage.*`, outside this lane.
- **Incoming props keep their colliders from frame one**, but `scale_prop`
  rewrites `Collider.width/height` alongside `Size`, so the collider is never
  larger than the sprite. The symmetric bug (an invisible wall) closed for the
  price of two lines.
- **Rejected:** a `DyingProp` component. That is four code sites and three CMake
  lists (the exact trap in HANDOFF.md), for a list that `main` already has.

### D78 — The shift-start is a lambda, so the wave-50 boss can call it  *(2026-08-09)*
- **Decision:** the body of the `want != active_arena` branch became
  `begin_arena_shift(int want) -> bool`, declared right after
  `apply_arena_props`. It has no dependence on `wave_cleared` or the spawner and
  refuses out-of-range / already-live / pre-first-arena indices.
- **Why:** Lane D's wave-50 boss shifts the arena **mid-fight**, and the effect
  was previously reachable only from the cleared-wave edge. It is declared above
  the `// === HOOK: boss ===` block, so Lane D can call it without touching any
  Lane E code. Confirmed callable mid-wave: a forced `begin_arena_shift(1)` at
  frame 600 of wave 1 (enemies alive) ran the whole transition cleanly.
- **Rejected:** a blackboard "request a shift" key. Extra indirection, an extra
  frame of latency, and no caller that needs the decoupling.

### D79 — Debris rides the solid props only; measured, not estimated  *(2026-08-09)*
- **Decision:** the debris emitter goes on props that had a `Collider` —
  obstacles and hazards, ~28 in the worst arena — and never on the ~97 decorative
  wall segments. 14 particles/s, 0.8s lifetime, for the length of the window.
- **Why:** the wall ring sits at radius 1400 and is mostly off-camera; emitting
  from all 125 props would have cost ~1300 concurrent particles against a 2000
  cap that ENGINE.md §5 records as *already* truncating at wave 20. Using the
  presence of a `Collider` as the filter means the emitter list and the
  "solid thing" list can never disagree.
- **Measured:** peak **336** live particles for a worst-arena destruction in
  isolation (`test_arena_vfx.cpp` re-measures and fails above 500); **463** in a
  live forced shift, where the debris overlaps the ~250-particle shift shockwave,
  the drone thruster and the outgoing arena's hazard vents.
- **`DEFAULT_MAX_PARTICLES` deliberately not raised** — that is an engine change
  owned by Phase 10, which now has a real number to budget with.
- **Rejected:** one big burst at the arena centre. Cheaper, but it reads as an
  explosion happening *to* the arena rather than the arena coming apart, and at
  radius 1400 the player would usually not see it.
### D56 — Sustain pickups place on a spiral, not on an RNG draw  *(2026-08-09)*
- **Decision:** `sustain_spawn` is a free function with **no RNG at all**. The
  n-th placement sits at `centre + (cos, sin)(n * golden_angle) * radius *
  0.88 * sqrt(frac(n * 0.618))`, and the health/shield split is an exact
  Bresenham-style walk over `shield_weight` (100 placements at 0.35 give exactly
  35 shields). Its whole state is three Blackboard keys —
  `sustain.timer/count/wave`.
- **Why:** determinism is a project invariant, and the way it usually breaks is a
  conditional or reordered draw (the R2 discipline `EnemyDeathSystem::drop_loot`
  has to spell out at length). A spiral cannot have that bug: there is no stream
  to get out of step. It is also better-distributed than uniform sampling — the
  sunflower packing never clumps — and it removed the need for a system object,
  a seed to plumb through `main.cpp`, and a reset hook in `spawn_world` (a lambda
  this lane does not own). New-run detection is instead "the wave counter went
  down", which only ever happens when `spawn_world` has rebuilt the world.
- **Rejected:** a `std::mt19937` seeded from `cfg.seed` held in a system object —
  the plan's original shape. It needs a construction site outside the hook block
  and a re-seed site inside `spawn_world`, and buys nothing a low-discrepancy
  sequence does not already give.
- **Also decided:** a shield cell is placed only once the player owns a capacitor
  (`shield_max > 0`); before that every placement is hull, because an unbanked
  cell is a pickup that visibly does nothing. `PickupSystem` still clamps to
  `shield_max` regardless, so the rule holds for any future producer. Sustain
  pickups carry **no `Lifetime`** — unlike coins (D52, which despawn to make
  grabbing them a risk call), they are the arena's standing offer of a heal and
  `max_live` is what bounds them.
- **Timer epsilon:** the countdown fires at `timer <= 1e-4`, not `<= 0`. A
  countdown of N equal float steps lands a hair either side of zero, which slips
  the placement to the following frame about half the time; 1e-4 s is 0.006 of a
  frame and makes the cadence an exact, testable frame count.
- **Numbers (provisional, unplayed):** `interval 14s`, `max_live 3`,
  `health 25`, `shield 20`, `shield_weight 0.35`, `min_player_dist 220` in a
  1400-radius arena.

### D57 — The dash is held, not edge-triggered, and reads its own key  *(2026-08-09)*
- **Decision:** `tick_dash` (free functions in `dash_system.hpp`, the
  `tick_shields` idiom) fires while LSHIFT/RSHIFT is **held** and `dash_cd` is
  zero. The key is read inside the `dash` hook block — physical state plus a scan
  of `opts.keys` for a scripted `LSHIFT` — rather than in main's shared key-edge
  section above.
- **Why:** `ShipState.dash_cd` is already the gate, so edge detection would add a
  `bool` of state to disambiguate nothing: holding the key simply dashes again the
  moment the cooldown expires, which is what a player holding it wants anyway.
  Reading the key in the hook keeps the entire feature a single contiguous diff in
  a file five other lanes are editing in parallel — the one thing the multi-agent
  protocol most needs to be true.
- **Damage rules** (straight from the playtest note): each enemy takes exactly one
  `DamageEvent` per dash, tracked by a per-dash already-hit list in `DashState`
  (0.15 s is ~9 frames; without the list a dash would delete the swarm). The
  player's first contact is deliberately let through untouched so
  `PlayerDamageSystem` resolves it normally; every frame after it holds
  `player.iframes` above the remaining burst, so ploughing through a crowd costs
  one hit, not one per body.
- **Rejected:** damaging on collision-enter via `CollidedWith` — collision runs
  *after* this hook, so it would be a frame stale and would still need the
  already-hit list for bodies that stay in contact.
- **Particle cost:** zero new entities. The drone's existing thruster emitter is
  driven at 240/s for the burst and restored afterwards: ~36 extra live particles
  at a 0.4 s lifetime, under 2% of `DEFAULT_MAX_PARTICLES`, and it decays well
  before the cooldown is up so dashes cannot stack their cost.

### D58 — Minimap blips are UI widgets, because screen-space entities do not render  *(2026-08-09)*
- **Decision:** the blips are a pooled set of **`UIElement` panel widgets**
  (+ `UIState` + `ScreenMembership{"gameplay"}`) created once and repositioned
  every frame, with a parked blip expressed as a zero-width rect. The mapping
  itself is `minimap_math.hpp`: pure, engine-free, and the only thing that knows
  about arena circles.
- **Why (this overrides the plan):** the plan specified `ScreenPosition + Size +
  Color + RenderLayer` entities with no `Position`. **Nothing draws those.**
  `RenderSystem::render` iterates `entities_with_component<Position>()`, so a
  `ScreenPosition`-only entity is never reached; `HUDSystem` draws only `Text`,
  which is why the HUD *text* rows work and why they are not the precedent they
  look like. Adding a `Position` hands the entity straight back to `CameraSystem`,
  which overwrites `ScreenPosition` every frame — the trap the plan correctly
  named, with no escape on that path. The working precedent is
  `GameHUDSystem`'s hull/shield gauges, which are widgets.
- **What it buys beyond just working:** design-canvas coordinates (so the map is
  resolution-independent through `ui_canvas_transform` for free), `z_order` above
  the frame panel, style-driven colours, and survival across `spawn_world` —
  which deliberately skips `UIElement` entities — so the pool really is allocated
  once per process rather than once per run.
- **Geometry has one authority:** the `minimap` config block. `MinimapSystem`
  writes `x/y/size` over the authored rect of the `minimap_frame` panel, so the
  JSON rect is a placeholder and the two can never disagree.
- **Out-of-arena clamping** clamps the offset **vector's length**, not x and y
  independently: a body past the wall sits on the rim on its true bearing, where
  per-axis clamping would slide it into a corner and point the player the wrong
  way.
- **The cap is loud:** priority order is player, then boss, then the swarm, then
  loot, so overflow only ever drops the least informative blips; the overflow is
  logged whenever it gets *worse* (logging every frame would print 60 lines a
  second and bury everything else).
### D66 — Enemy fire is a float countdown, and needs no damage system  *(2026-08-09)*
- **Decision:** `EnemyFireSystem` ticks `EnemyBehavior::timer` down by `dt` and
  fires at zero. A shot is `Position`+`Velocity`+`Size`+`Color`+
  `Collider(ENEMY_SHOT)`+`ContactDamage`+`Lifetime`+`EnemyShot`+ a trail — the
  `PlayerFireSystem` recipe on a different layer.
- **Why:** a per-entity float is deterministic for free, where a "P(fire) this
  frame" roll would put an RNG draw on every enemy on every frame and make the
  replay stream depend on the live enemy count. And Phase 0 already proved the
  damage path: `PlayerDamageSystem` hurts the drone for *anything* carrying
  `ContactDamage` in its `CollidedWith`, so enemy projectiles cost zero new
  damage code. A second damage path was the obvious wrong build here.
- **Tier asymmetry:** tier 1 straight, tier 2 tracking with a clamped turn rate,
  tier 3 laser — and the laser is the only shot **not** destroyed on contact.
  Piercing *is* the tier-3 upgrade, so it lives in the shot's own `EnemyBehavior`
  tier rather than needing a `Piercing` component.
- **Rejected:** a `MoonTag`/`EnemyShotSpec` component. Registering a component
  type is an edit in three shared files (code-standards); the tier already rides
  on a component the shot has to carry anyway.

### D67 — The moons arrive on a spawn cadence, not in a wave roster  *(2026-08-09)*
- **Decision:** `EnemyType::first_wave` (0 = never) plus a top-level `specialty`
  block with two cadences over the spawn counter: `moon_every_n_spawns` injects
  one unlocked shooter, `every_n_spawns` injects the live arena's specialty unit.
  `by_arena` maps an arena **name** to an enemy_types **name**, resolved to an
  index once at load.
- **Why:** the plan said to gate the moons into wave `types` lists at waves
  3/15/30, but the `waves` block belongs to Lane A and the 50 rows were already
  authored. A cadence gets the same result with a nine-line data block and no
  edit to another lane's file — and it scales to 50 waves without 50 edits.
  Names rather than indices because the arena list and the enemy_types list are
  authored by different lanes, and an index would rot silently if either moved.
- **Consequence, handled:** `type_lookahead` (Hard's "types unlock earlier")
  reads wave rosters, so it would have skipped this second unlock axis entirely.
  `apply_difficulty` now also pulls `first_wave` forward by `type_lookahead` —
  in the one place difficulty scaling happens (D50), never a second.
- **Rejected:** editing the 50 wave rows. Not this lane's block, and a merge
  conflict with Lane A for no gameplay difference.

### D68 — One specialty system, and the splitter lives in the death system  *(2026-08-09)*
- **Decision:** spitter, miner and bulwark are three cases of one
  `SpecialtySystem`; the Prism splitter is ~40 lines inside `EnemyDeathSystem`.
  Second-pass escalation is two multipliers (`tier2_hp_mult`,
  `tier2_speed_mult`), not four new `enemy_types` rows.
- **Why:** all four are "an `EnemyBehavior` countdown that does something on
  expiry" — except the splitter, whose entire behaviour *is* a death event, and
  that is the system already holding the death event's RNG ordering. Splitting it
  out would mean a second pass over the corpse. The tier-2 multipliers follow
  D10: per-wave multipliers over the shared types, never a parallel table.
- **Mine representation:** a deployed mine is `EnemyBehavior{MINER, tier 0}` with
  no `EnemyTag`; tier 0 = the mine, tier >= 1 = the thing that drops mines. Its
  three spare fields carry arm delay, blast damage and trigger radius.
- **Why not a `MineTag`:** three shared-file edits for one bool, against a
  component the mine already needs. Children of a splitter carry **no**
  `EnemyBehavior` at all — that, not a depth counter, is what bounds recursion.
- **Determinism:** the split draws no RNG and runs after `drop_loot`, which is
  the only RNG in that function.

### D69 — The hazard recipe is shared, but `spawn_arena_props` is left alone  *(2026-08-09)*
- **Decision:** `hazard::spawn_patch` in a new `hazard_patch.hpp` is the one
  recipe for "a static thing that hurts the drone and expires". Poison patches,
  mine blasts and the boss's borrowed attacks all call it. `main.cpp`'s
  permanent arena vents were **not** re-pointed at it.
- **Why:** the plan asked for the recipe to be extracted from
  `spawn_arena_props`. The extraction's purpose — no copy-paste of the component
  list — is met by every new caller using the helper. Re-pointing the existing
  vents means editing the exact lambda Lane E's arena-VFX phase is rewriting, in
  parallel, for zero gameplay gain. Marked with a `ponytail:` comment.
- **Next step:** after Lane E merges, pointing `spawn_arena_props` at the helper
  is a ~30-line deletion in `main.cpp`.

### D70 — The boss holds the wave open; the spawner learns nothing about bosses  *(2026-08-09)*
- **Decision:** `WaveSpawnerSystem::set_clear_hold(bool)`. `BossSystem` raises it
  the frame it spawns a boss and drops it only once the reward has been taken.
  Spawning is unaffected — only the wave *clear*.
- **Why:** three problems, one flag. (1) "The wave clears when the boss dies"
  needs the spawner to not finish a wave whose adds happen to be dead. (2) The
  30 s straggler force-kill (R3) would otherwise **execute the boss** the moment
  the adds ran out. (3) The load-bearing one: `BossSystem` runs from a hook
  inside `PHASE_PLAYING`. Without the hold, the wave clears one frame after the
  boss dies, `main.cpp` switches to `PHASE_INTERMISSION`, and the reward screen
  would be pushed onto a phase where this system never runs again — a modal the
  player could never dismiss. The hold keeps the phase in `PHASE_PLAYING` until
  the pick is made.
- **Rejected:** an "undead boss" held at 1 HP (a corpse standing in the arena);
  and teaching the spawner what a boss is (it would need the boss config, the
  behaviour kind and the reward state — all of which are `BossSystem`'s).

### D71 — The reward screen is data, and re-picking is the upgrade  *(2026-08-09)*
- **Decision:** a `boss_reward` screen in `GameData.json` with three buttons
  (`reward_0/1/2` -> `on_active_pick_0/1/2`). `BossSystem` rewrites their labels
  from the `actives` catalogue each time it pushes the screen, resolving widgets
  by name through `ui.widget_id.<name>`. The held active is offered **first** as
  "UPGRADE <name>"; re-picking it multiplies a `ship.active_cd_mult` blackboard
  key by 0.75, floored at 0.35.
- **Why:** the `menu_ship` pattern exactly (D82) — one widget set, relabelled,
  and no second widget-lookup mechanism. The upgrade is a blackboard key rather
  than a sixth `ShipState` float, per D28/D41: it is a single number with one
  reader. `test_boss.cpp` pins the widget names against the string literals
  `boss_system.cpp` compares to, the `test_intermission_screen.cpp` idiom.
- **Note:** the widgets are a data-authored `screens` entry, so the
  "screen-space entities need no `Position`" trap does not apply — that is a
  `RenderSystem` concern, and widgets render through `UIRenderSystem`.

### D72 — Wave 50 is a 9th arena; the mid-fight shift is one wired line  *(2026-08-09)*
- **Decision:** a 9th `arenas` entry, `Singularity`, `first_wave: 50`, using the
  existing `bg_galaxy_*` / `pillar_galaxy` / `vent_galaxy` art with a gold
  `enemy_tint` (255,200,80). `BossConfig` gains `final_mult` and
  `final_summon_bonus`, applied on the **last** boss wave. `BossSystem` latches
  `wants_arena_shift()` once the final boss drops below `shift_hp_frac`.
- **Why gold:** everything else in that arena is black and royal purple; gold is
  the complement that keeps enemies readable against a void.
- **Why `first_wave: 50` and not 51:** it makes wave 50 the void *now*, so the
  themed, extra-hard finale ships even before the mid-fight transition is wired.
  The transition then upgrades a cut into a transformation.
- **Outstanding, deliberately:** this worktree was cut before Lane E's
  `begin_arena_shift(int)` existed, so the hook calls
  `lane_d_arena_shift_stub(idx)` — which sets the *same* Phase-5b crossfade
  fields `main.cpp` already drives on a cleared wave. It is a subset of Lane E's
  behaviour, not a second transition. The merge is one line, flagged with a
  `MERGE ACTION` comment. It is named differently from `begin_arena_shift` **on
  purpose**: a block-scoped lambda of that name would silently shadow Lane E's
  after the merge, and the wiring would look done while doing the old thing.

### D73 — Hard-mode boss lethality is one field  *(2026-08-09)*
- **Decision:** `DifficultyDef::boss_mult`, scaling `BossConfig::health` and
  `contact_damage` together inside `apply_difficulty`. Hard ships 1.6.
- **Why:** the open item from the Phase-B interview ("more lethal ... boss") that
  could not land until the boss did. One field, scaled in the one place
  difficulty scaling happens (D50), keeps `apply_difficulty` the single
  non-idempotent overlay — a boss-specific difficulty path would be a second
  place to forget to re-copy `base_config` from.
- **Rejected:** separate `boss_hp_mult` / `boss_damage_mult`. Nothing in the
  notes wants a boss that is tanky but gentle, and two knobs nobody turns
  independently is speculative generality.

### D74 — The actives are free functions, and the push is extracted, not copied  *(2026-08-09)*
- **Decision:** `actives::tick` in `active_items.{hpp,cpp}` — the
  `item_system.hpp` idiom. The repulsion device reuses the Repulsor Field's shove
  via a new `items::push_enemies_out(storage, px, py, radius, push, dt)`, which
  `items::repulse_enemies` now also calls. Cross-frame state (beam clock, sweep
  angle, sphere timer) is three Blackboard floats.
- **Why:** `repulse_enemies` gates on "is the Repulsor Field equipped", which the
  boss reward is not — so reuse meant splitting the question from the shove. Two
  copies of a clamped push would drift apart the first time either was retuned.
  Blackboard over components per D28/D41; free functions because there is no
  per-frame state to own.
- **Missile marker:** a missile is identified by `ProjectileData.target !=
  NO_TARGET`, since `PlayerFireSystem` always writes `NO_TARGET`. Marked
  `ponytail:` — a missile whose target dies stops homing and flies straight; the
  4 s fuse bounds the cost. Re-acquiring would need a real marker component.
- **Beams** reuse the already-registered `BeamTag` and are recycled per frame,
  the `HealthBarSystem` idiom. The hit test is a pure forward-ray distance, so a
  "beam" can never be a full-length line through the drone in both directions.

### D75 — The actives are on `E`, with no headless alias  *(2026-08-09)*
- **Decision:** `E` fires the two aimed actives, edge-detected by a
  function-local `static` **inside** the `actives` hook. The repulsion device is
  not on a key at all — it auto-fires below 20 % hull.
- **Why:** the `*_prev` key edges at the top of the frame loop are outside this
  lane's hook, and the scripted-key parser (`--keys`) is not this lane's file
  either. A local static keeps every line Lane D adds to `main.cpp` inside a hook
  block, which was the whole point of the Phase-0 scaffolding.
- **Cost, stated plainly:** no `--keys E`, so the actives cannot be driven by a
  headless script. They are covered by unit tests instead, and their particle
  cost was measured with a temporary forced trigger that has been reverted.
- **Threshold:** `field_should_fire` uses strict `<` on 20 %. At exactly 20 % the
  device has **not** fired — it is a *below* 20 % effect, so a drone sitting on
  the line still owns its panic button. Pinned as a boundary unit test.

### D93 — Moons are waning crescents, not discs  *(2026-08-09)*
- **Decision:** `moon_1/2/3` get their own generated sprites (`enemy_moon_*`)
  instead of sharing `enemy_warden`'s octagon. The shape is a lit disc with a
  second disc punched out of its **leading** edge — art faces right, so the horns
  point the way the moon travels.
- **Escalation is silhouette, not colour** (the arena tint owns colour): tier 1
  is a bare crescent, tier 2 adds barbed horn tips, tier 3 is thicker and carries
  a bright focus in the crescent's mouth for the laser.
- **Rejected:** tinting the warden per tier — the tiers would still be the same
  shape, which is what made them unreadable in the first place.

### D94 — Sustain pickups wear sprites; `Color` stays as the fallback  *(2026-08-09)*
- **Decision:** health = a medical cross in a housing ring, shield = a barrier
  crest with a charge chevron. Added as `Images` **alongside** the existing
  `Color`, never replacing it: the render chain is SpriteSheet > Images > Color,
  so the flat colour is a free load-failure fallback.
- **Names are relative to `assets/images/`** (`"v2/pickup_health.png"`), unlike
  sidecar paths which are relative to `assets/` and carry `images/`.

### D95 — Currency is a struck circular coin  *(2026-08-09)*
- **Decision:** hard bright rim, inner ring, one thick credit bar, specular arc.
- **Why those four marks and no more:** the coin is the most common thing on
  screen and is drawn ~26 px. A finer glyph turned to mush at that size (tried a
  bar-through-a-ring first, and looked at it). The hard rim is also what keeps it
  from reading as a projectile — bullets here are soft edgeless glow blobs.

### D96 — Mines look like bombs  *(2026-08-09)*
- **Decision:** dark round body, cap, curved fuse, lit spark, hot orange rim.
  The body is deliberately near-black so the rim and the spark carry the read
  against a near-black arena; a bright body would have been a second coin.
### D97 — Range is `projectile_lifetime`, never `projectile_speed`  *(2026-08-09)*
- **Decision:** the Long Barrel row (`effect: "range"`) adds to
  `WeaponStats.projectile_lifetime` and leaves `projectile_speed` alone.
- **Why they are not interchangeable:** range is the product of the two, but only
  one of them is *only* range. Raising speed also shortens time-to-target, which
  changes how much a player must lead a moving enemy, how easily an enemy can
  side-step a shot, and how the gun feels in the hand — three balance changes the
  player did not buy. Raising lifetime changes exactly one observable: where the
  shot expires. The ask was "increase bullet range", so the field that means only
  range is the one that moves.
- **Cost, stated plainly:** lifetime is also what keeps the shot's trail emitter
  alive, so range is the one of the two new rows that costs particles. Measured
  below (D99). Speed would have been free on that axis and wrong on every other.
- **Rejected: a second `range` field on `WeaponConfig`** multiplied in at fire
  time. It would be a third number describing the same product, and
  `PlayerFireSystem` already reads lifetime straight onto `Lifetime`.
- **`max_stacks: 3` at +0.30 s** caps the shot at 2.1 s / ~1050 px, comfortably
  inside the 1400-radius arena — a shot that outranges the arena is a shot whose
  last level bought nothing.

### D98 — A ricochet is a counter on `ProjectileData`, not a new component  *(2026-08-09)*
- **Decision:** `ProjectileData` gains `int bounces`. `PlayerFireSystem` stamps it
  from `ship.bounces` (the `ship.extra_shots` Blackboard pattern, D26 — no
  catalogue index reaches a system), and `ProjectileHitSystem` spends one per
  surface, destroying the shot only once the budget is empty.
- **Why not a new component:** `ComponentStorage` uses explicit instantiation —
  a new type is four code sites plus three CMake lists, all shared files, for one
  integer that belongs to the per-shot record that already exists.
- **Two surfaces, one helper.** Obstacles arrive through `CollidedWith`; the arena
  ring does **not** — it is a position clamp in `main.cpp`, not a collider — so a
  ricocheting shot tests the ring itself. `ProjectileHitSystem::set_arena` is one
  wire-up line in `main.cpp`; nothing is set means obstacles still bounce and the
  ring does not.
- **The normal is not derived twice.** `push_circle_out_of_aabb` (obstacles.hpp)
  already resolves a penetrating circle to the nearest clear centre, and the
  direction it moved the circle *is* the outward normal. `bullet_bounce.hpp` reads
  it off that resolution rather than re-deriving a face test that could disagree
  with the drone's blocking.
- **Not re-colliding with the surface it left** falls out of the same call: the
  shot is placed on the resolved clear centre plus 0.5 px of clearance, so the
  next broadphase reports no overlap. Belt and braces, a shot with bounces left
  that the helper *refuses* (clear of the box, or already travelling away from it)
  is left alone rather than destroyed — otherwise a stale contact would kill the
  ricochet the frame after it worked.
- **Enemies outrank walls in the same frame.** `ProjectileHitSystem` now scans the
  whole `CollidedWith` list for an enemy before considering a surface, where it
  used to take whichever came first. A ricochet must never eat a hit, and the old
  order was list-order luck.
- **Deterministic by construction:** the reflection is pure arithmetic. No scatter,
  no jitter, no RNG draw added on any path.
- **Rejected: extending `Lifetime` on a bounce.** It would make the bounce row a
  second range row, double the particle cost, and let a shot ping-pong forever.
  The bounce count is the only thing bounding the shot beyond its unchanged fuse.

### D99 — Measured particle cost: bouncing is free, range is not  *(2026-08-09)*
- **Measurement** (`SDL_VIDEODRIVER=dummy`, seed 42, fire held frames 120-2999,
  3000 frames, peak live `Particle` entities via temporary instrumentation since
  reverted):

  | loadout | peak particles |
  | --- | --- |
  | stock weapon | 274 |
  | 3x Long Barrel | 351 |
  | 3x Long Barrel + 3x Ricochet Coils | 351 |
  | 3x Long Barrel + 6x Overclock + 2x Twin Barrel | 1176 |
  | the same + 3x Ricochet Coils | 1195 |

- **Ricochet Coils costs +19 particles (1.6 %) at the worst loadout and 0 on its
  own.** A bounce does not touch `Lifetime`, so a bouncing shot holds its trail
  emitter for exactly as long as a non-bouncing one — it just spends that time
  somewhere else. The lane brief's worry ("bouncing shots live longer") does not
  hold *because* of the D98 decision not to extend the fuse.
- **Long Barrel costs +77 alone**, and the 1176 figure is what a fully upgraded
  gun costs whether or not this lane exists — Overclock and Twin Barrel already
  shipped. Against the 4000 cap and the 1998 measured on a boss wave, the headroom
  is real but the two are additive: a boss wave with a maxed gun is the case to
  re-measure if the cap is ever approached again. **`DEFAULT_MAX_PARTICLES` was
  not raised.**
- **Both rows are `upgrades`, not `items`.** `items` + `consumables` is capped at
  8 by the `1`-`8` gear keys and `arena_config.cpp` truncates past it — two more
  item rows would have silently deleted two consumables. `upgrades` had exactly
  two slots free in `ShipState.upg_counts[8]`, and stacking with `price_growth` is
  already the levelling mechanism for that page, so "levelable like the rest"
  needed no new machinery at all. The 8 authored `shop_card_*` widgets now hold
  exactly 8 upgrade rows.
---

## Iteration 3 — Lane K: run save/quit + pickup placement

### D100 — The run save is a second file, and it stores state, not a world  *(2026-08-09)*
- **Decision:** `saves/run.json` (`CPP/game/run_save.{hpp,cpp}`) holds the run's
  *state* — seed, difficulty index + name, ship id, 0-based wave, score, hull and
  hull max, shield/regen/delay, credits, keys, speed mult, item/consumable/active
  ids, extra shots, `upg_counts[8]`, `gear_levels[8]`, and the five weapon
  numbers. No entity snapshot, no `SerializationRegistry`, no `LoadSystem`.
- **Why a second file and not a field on the meta-save:** D80's file is the one
  number that outlives *every* run and is written at every run end; this one is a
  run in progress and is written only when the player asks. Merging them would
  make the lifetime score hostage to the resume format's version check.
- **Resume goes through `start_run`.** `start_run(difficulty, const RunSave*
  resume)` gained one parameter and nothing else: the pristine `base_config`
  re-copy, `apply_ship`, `apply_difficulty`, `set_config` and `spawn_world` are
  all shared with a fresh start, so `apply_difficulty` keeps the single
  non-idempotent application site D50 requires. Only three things happen on the
  resume branch: `config.seed` is taken from the save, `wave_spawner
  .resume_at_wave(saved wave)`, and `run_save_apply` overlays the numbers onto
  the world `spawn_world` just built.
- **Why the wave restarts from the top:** the save records a wave *number*, so
  there is nothing half-spawned to reconcile — which is exactly what makes the
  no-snapshot decision cheap rather than merely smaller.
- **Weapon stats are stored as stats, not as a purchase history to replay.** The
  shop's apply-an-upgrade path is private and belongs to another lane, and the
  number the run actually has is the number worth restoring. `upg_counts` is
  still saved, so prices keep escalating from where the player left them.
- **Tolerant per field, not per file.** `jget` checks the JSON type of each key
  and falls back individually, so a truncated save still resumes; a wrong
  *version* is treated as no save at all, because silently reading a future
  file's fields is how a save format starts lying. `run_save_apply` additionally
  ignores nonsense: a missing hull leaves the freshly spawned full-health drone
  alone rather than resuming you as a corpse.
- **Cleared on death and victory only**, never in `bank_run_score` — that lambda
  also fires on quit-from-pause and on closing the window, and a player who
  pressed SAVE and then QUIT must find their run still there.
- **Known, accepted:** `enemy_death.set_economy(config.economy, config.seed)` is
  called once at startup, so a resumed run's *drop* RNG follows the launch seed
  rather than the saved one. Re-calling it in `start_run` would also re-apply the
  economy at a second site, which D50 exists to prevent, for a stream nothing
  compares against.
- **Rejected:** porting `SerializationRegistry` / `LoadSystem` (an order of
  magnitude more code and every line of it a determinism hazard — the same
  reasoning as D80); autosave at the intermission edge (SAVE is an explicit act,
  and a cadence is one extra call site whenever it is wanted).

### D101 — Loot placement is a spiral nudge, not a rejection loop  *(2026-08-09)*
- **Decision:** `loot_place::blocked` / `loot_place::nudge_free` in
  `enemy_death_system.{hpp,cpp}`. The scattered point comes off the RNG exactly
  as before; if it overlaps something, the coin walks a fixed golden-angle spiral
  of 16 candidates and takes the first free one. Reach is `pickup_scatter * 3`.
- **Why, and this is the whole point:** a rejection loop that re-draws until it
  finds a free spot would consume a *variable* number of random values per kill
  and desynchronise every later roll — the single easiest way to break this
  project (ENGINE.md §4; `drop_loot` spells the discipline out at length). The
  spiral draws nothing at all, so the question of "how many draws did the search
  take" cannot arise. Same reasoning as Lane B's sustain spiral (D56), reached
  independently from the opposite direction: B avoided a stream, K avoids
  perturbing one.
- **Pinned by test:** `test_loot_placement.cpp` kills an enemy in two
  identically-seeded worlds — one empty, one blanketed in hazards so every
  candidate is rejected — then kills a second enemy in clean space in each. The
  second kill's coin positions are asserted **identical**. That test fails the
  moment anyone puts a draw inside the search.
- **What counts as blocked:** `Collider.layer & (OBSTACLE|HAZARD)` (which covers
  arena pillars, permanent vents, Bio-lab poison and mine blasts through D69's
  shared recipe), `EnemyBehavior{MINER, tier 0}` (a deployed mine carries *no*
  collider — D68 — so it needs its own test), and any existing `Pickup`. Coins
  are in storage the instant they are created, so a scatter cannot stack on
  itself.
- **What is deliberately NOT a blocker:** enemies and projectiles. Loot under a
  live enemy resolves itself, and treating every collider as solid would nudge
  most coins in a crowded arena for no gameplay gain.
- **Fallback is the drawn point.** Nothing free within reach means the coin stays
  where it fell. A worse placement is better than a search that could run long.
- **Rejected:** clamping loot to the arena circle at the same time. The drawn
  scatter could already land slightly outside; that is a separate (and much
  rarer) complaint, and folding it in here would hide which change did what.

### D102 — CONTINUE is one inert widget, not a screen that appears  *(2026-08-09)*
- **Decision:** `menu_continue` on `main_menu` is a single button that main.cpp
  relabels every title frame: a `default_button` reading
  `CONTINUE - wave N, <difficulty>` when a usable save exists, and an empty
  `subtitle` when one does not. A click with no save is consumed and ignored.
- **Why:** `UIElement` has no visibility flag and adding one is an engine change
  for a label that can simply say nothing — the wall D82 hit with `menu_ship`,
  and the same answer. It also means the title screen has one layout, not two.
- **Placement:** below the main-menu panel rather than inside it. The panel is
  full, and moving another lane's widgets to make room during a parallel restyle
  is a merge conflict for no gain.
- **The pause SAVE button is the same idea:** `pause_save` shows `SAVE`, becomes
  `SAVED` (or `SAVE FAILED`) on click, and is reset to `SAVE` every time Escape
  opens the screen — the confirmation is the only feedback a save has, and a
  stale one from last visit would be a lie. The label `Save & Options: coming
  soon` became `Options: coming soon`; Save is no longer coming soon.

### D103 — The save is proved harmless to determinism, not assumed to be  *(2026-08-09)*
- **Decision:** `saves/run.json` is read exactly once, at startup, into a
  `RunSave` whose only consumers are the CONTINUE widget's label and the resume
  branch of `start_run`. `run_save_apply` returns immediately unless
  `present` is set, and a fresh run never sets it.
- **Why stated as a decision:** "the save does not affect the sim" is the kind of
  claim that rots. Writing down *where* the boundary is makes a future autosave,
  or a save read from inside a system, an obvious violation rather than a subtle
  one. This is the D80-D83 discipline applied to a much larger file.
- **Verified:** the replay canary
  (`--seed 42 --keys 10:SPACE --stopframe 3000`) run twice with no
  `saves/run.json` and twice with a fully populated one — all four summary lines
  byte-identical. Plus a unit test that `run_save_apply` on a non-`present`
  struct with tempting contents changes nothing.
### D85 — Text is fitted to its widget in the renderer, once, for everything  *(2026-08-09)*
- **Decision:** `fit_text_in_rect()` in `ui_render_math.hpp` measures a rendered
  string against its widget rect and returns a box that is **always inside** it,
  uniformly scaled down (never up) and centered on the cross axis.
  `UIRenderSystem` calls it on both the label and the button path. Labels are
  left-aligned with no padding; buttons are centered with a 10px (design-canvas)
  inset.
- **Why:** the player reported four separate "text outside the box" bugs — the
  title-screen hint running off the *screen*, the ship-unlock line overflowing
  the panel on both sides, the pause footer, and a shop card. They are one bug:
  the renderer positioned text from `rect.x`/`rect.y` using the texture's own
  size and never looked at `rect.w`/`rect.h` at all. Hand-tuning four strings is
  what produced the state the user was complaining about; the fifth string would
  have broken next week.
- **Shrink, not wrap or ellipsis.** Wrapping needs per-word measurement, a line
  breaker and a rect whose height is authored for N lines; an ellipsis destroys
  information the player is trying to read (a price). Shrinking is ~15 lines,
  keeps the whole string, and degrades gracefully. The rects are authored to fit
  at full size anyway — the fit is the floor, not the layout plan.
- **Never scales up**, so every label that already fitted renders byte-identically
  to before the change. That is what made this safe to land on six screens at once.
- **A zero-size rect returns `visible = false`.** Collapsing a rect is how this
  codebase hides a widget (D58, D61, and now D86); the fit must not resurrect a
  hidden widget's text at the collapse point.
- **Pure, and takes metrics rather than a font**, so the whole contract is unit
  tested with no window: `CPP/game/tests/unit/test_ui_text_fit.cpp` sweeps ~1500
  size combinations and asserts the box is inside the rect for every one.
- **Rejected: an `overflow` field on `UIElement`.** A per-widget policy for a
  behaviour that has exactly one right answer.

### D86 — The HUD is hidden by phase, not by the screen stack  *(2026-08-09)*
- **Decision:** `hud_visible_in_phase(phase)` (in `game_hud_system.hpp`) is the
  single visibility rule: the arena HUD is drawn in `PHASE_PLAYING` and
  `PHASE_INTERMISSION` and in no other phase. `GameHUDSystem` collapses its six
  gauge widgets and blanks its text rows; `MinimapSystem` collapses the frame and
  parks every blip.
- **Why:** `"gameplay"` is `ScreenStackSystem`'s base sentinel — always on the
  stack, never modal — so its widgets rendered on the title screen and under the
  shop panel. That was the "menu gets overlaid on top of the hp and shield bars"
  report, and it was never going to fix itself from the stack.
- **Phase, not `is_modal()`.** The intermission is modal *and* flies the drone, so
  a modality test would hide the hull bar exactly when the player is dodging under
  the prompt. This mirrors HANDOFF trap 7 — `sim` is keyed off
  `stack.back() == "pause"` for the same reason — but is a *separate* question:
  visibility and simulation are not the same predicate and are not shared.
- **Collapse, not a visibility flag.** `UIElement` has none; adding one is an
  engine change and a `UIRenderSystem` branch to say what a zero-size rect already
  says (D58, D61). `GameHUDSystem` caches the authored rects once, so hide/show is
  lossless.
- **The pool is parked, not destroyed.** The 120 blip widgets are allocated once
  per process (D58) and stay allocated while hidden.
- **Consequence:** the "REACTOR DRONE - click to start" banner is gone. It was a
  second copy of the heading the `main_menu` panel already carries, drawn in a
  44px face that ran off the right of the window.

### D87 — Both halves of the HUD are authored in the design canvas  *(2026-08-09)*
- **Decision:** `GameHUDSystem::init` applies `ui_canvas_transform` to its
  `Text`+`ScreenPosition` rows, so its coordinates are 800x600 design-canvas
  coordinates like every widget rect in `GameData.json`.
- **Why (and the resize answer):** the gauges are widgets and go through the
  canvas transform; the text rows go straight to `HUDSystem` in window pixels.
  Authored in window pixels, "x = 20" for the score and "x = 16" for the HULL
  label were 20px and 67.6px from the left edge — two columns in one HUD, which is
  visible in the user's screenshots as the score sitting left of and above
  everything else. Pinning the logical surface at 980x660 hides this from *window*
  resizing, but it never was a resize bug: it is a two-coordinate-systems bug that
  a non-1.0 canvas scale makes visible at any window size.
- **Not a new mechanism:** the transform is the one the renderer already uses, so
  the two cannot drift.

### D88 — The menus are laid out on one grid, with one column and one type scale  *(2026-08-09)*
- **Decision:** every screen in `GameData.json` is re-authored on a 4px grid with
  a single left-aligned content column inset 24px from its panel edge, a 44px
  minimum hit target on every button, a three-step type/colour scale
  (`title` > `subtitle` > new `caption`), and a 2px `rule` under each heading.
  New styles: `caption`, `rule`, `card`, `shop_tab`, `minimap_frame`.
- **Why left-aligned, not centered:** the renderer left-aligns labels and centers
  buttons. Rather than add a centering mode, the layout commits to a flush-left
  column — which is also the faster read for a stack of short lines, and gives
  every screen the same optical left edge.
- **State feedback is now a real contract:** hover, press and disabled differ in
  *both* background and text on every interactive style. The shop's current tab is
  marked `disabled` (so it cannot be re-clicked), so `shop_tab`'s disabled state is
  authored as the **selected** look — bright fill, dark text — rather than as the
  grey "broken" look every other disabled style uses.
- **One primary per screen:** exactly one widget carries `pulse_hz`, and the shop
  cards use the new flatter `card` style so eight rows do not read as eight calls
  to action.
- **Panel opacity raised to 242.** A modal that lets a bloom-heavy arena strobe
  through it was the single largest legibility loss on every screen.
- **Rejected: centering labels via a new `element_type`.** An engine change to buy
  what a layout decision gives for free.

### D89 — The shop's tooltip becomes a fixed detail pane  *(2026-08-09)*
- **Decision:** the tooltip label pair no longer follows the hovered card. It sits
  in one fixed slot in the shop's right column, under the drone preview, and shows
  the current page's one-line explanation when nothing is hovered.
- **Why:** D64's repositioning was correct about the *mechanism* (a relabelled,
  repositioned pair, not a new widget kind) and this keeps that. But a pane that
  travels 44px per row jumps on every hover, and its 44..486 travel range slid it
  straight through the ship preview it shares the column with. A pane the eye can
  find once is worth more than proximity to a row the cursor is already on.
- **Idle state carries the page hint**, which is also #5's answer: the LEVELS page
  was a list of prices that never said what a level buys.
- **Supersedes D64's "collapsed when nothing is hovered"**; the collapse path
  remains for the case where the pane has nothing at all to say.

### D90 — "REACTOR SHIFT", and the shop title stops repeating itself  *(2026-08-09)*
- **Decision:** the arena-change banner reads `"<Arena> — REACTOR SHIFT"` (#13,
  the user's own wording). The shop heading is `REACTOR SHOP` on every page; the
  page name lives in the tab strip only.
- **Why:** "arena shift" is engine vocabulary — the player never sees the word
  "arena" anywhere else. And `REACTOR SHOP - UPGRADES` above a highlighted
  `UPGRADES` tab said the same word twice in 40px of vertical space.
- **Only one occurrence exists** (`main.cpp`'s `begin_arena_shift`); the identifier
  `begin_arena_shift` is deliberately unrenamed — it is code vocabulary, cited from
  D72/D76-D79 and two specs, and renaming it would churn four documents to change
  nothing the player sees.

### D105 — The boss is a real "Capital Drone Carrier", and an `Images` never wears an atlas  *(2026-08-10)*
- **Decision:** `assets/images/v2/enemy_boss_carrier.png` — one 256px frame, drawn
  at 512 and downsampled — replaces `v2/enemy_hulk.png` on the boss entity.
- **Why:** the boss wore an `Images{"v2/enemy_hulk.png"}`, and an `Images` wearer
  draws the **whole texture**. `enemy_hulk.png` is a 4x4, 14-frame atlas, so the
  boss rendered as a literal grid of hexagons (iteration-5 item #10). This is the
  general trap, not a boss bug: `Images` = a single full image, `SpriteSheet` =
  an atlas. Anything wearing an atlas through `Images` draws the contact sheet.
- **Drawn MONO like every enemy**, so `BossSystem`'s per-arena `enemy_tint` still
  themes it — Foundry orange, Core magenta, Bio-lab violet, and so on.
- **Single frame on purpose.** Animating it would mean a `SpriteSheet` + sidecar,
  i.e. the same atlas the fix removes, for a 260px ship that already carries four
  rotor rings, two flight decks and an engine bank.
- **Rejected: shrinking the hulk atlas to one frame.** It is a legitimate 14-frame
  enemy sprite; the boss was the thing pointing at it wrongly.

### D106 — Boss adds wear the spawner's art  *(2026-08-10)*
- **Decision:** `BossSystem`'s summoned adds get the `SpriteSheet`/`Animation`
  pair from `enemy_types[0]`'s sidecar, loaded once per summon volley.
- **Why:** they carried a `Color` and a `Tint` and nothing else, so every boss
  fight was the carrier ringed by flat magenta squares — the `Color` fallback that
  `WaveSpawnerSystem` only ever means to use when a sidecar fails to load.
- **A failed load still falls back to the rect**, exactly as the spawner does; the
  load is wrapped and cannot throw into the frame.
- **Ponytail ceiling:** one sidecar JSON read per volley (every 6s), not cached.
  Cache it if it ever appears in a profile.

### D107 — One drone vocabulary: rotors, booms, a lit chassis  *(2026-08-10)*
- **Decision:** the player and the four generic enemies (spark / runner / hulk /
  warden) are rebuilt in `make_sprites.py` around a shared `rotor()` + `boom()`
  pair: rotor pods with **blades that turn with the frame phase**, structural
  booms, and a lit chassis on top. Items #8 and #11.
- **Why:** every body was a single polygon, so nothing read as a machine and the
  player's "drone" was an arrow. The spinning blades are what buy the read — the
  march animation was previously only a brightness pulse.
- **Nothing else moved:** same 128px frames, same 8 march + 6 death frames, same
  4 columns, same MONO luminance, same sidecars. `test_manifest.py` passes and no
  `GameData.json` row changed, so the four specialty units inherit the new art for
  free through the atlases they already share.
- **The moon shooters are untouched** (D93 is recent and deliberate); their
  generator output is byte-identical after this change.
- **Rejected: per-unit sprites for the specialty units.** That is a `GameData.json`
  `enemy_types` edit and four more atlases to serve four rows that already read.

### D108 — The drone's shots are red  *(2026-08-10)*
- **Decision:** player projectiles and their trails are `(255,70,60)` (item #1).
- **Why:** they were `(120,225,255)` — the same cyan as the hull, the HUD text and
  the Core arena's primary. A shot in flight read as part of the ship. Red is the
  one hue no arena palette and no `enemy_tint` uses, so player fire is now the
  only red thing on the screen.
- **Still a literal in `player_fire_system.cpp`**, where the old cyan was. It is
  not a per-arena colour and it is not a playtest knob — it is "the player's fire
  is red". Moving it into `GameData.json` would be a config key with one value.

### D109 — Moon shooters fire from the mouth, and turn to face what they shoot  *(2026-08-10)*
- **Decision:** the shot spawns at `moon_muzzle_frac(tier) * size` ahead of the
  entity centre along the aim, and the shooter gains a `Rotation` set to its aim
  every frame (`flip_when_left = false`, pure rotation). Item #3.
- **Why:** the shot spawned at the centre, and the crescent (D93) is a lit disc
  minus a bite offset *toward* the facing — on the axis the body ends **behind**
  the centre, so shots left through the moon's back. And enemies carry no
  `Rotation` at all, so the sprite was drawn mouth-right forever: fixing only the
  offset would have been right one direction in four.
- **`moon_muzzle_frac` keeps the derivation, not the three answers**: the horn
  tips are the circle-circle intersection `hx = (r²-br²+dx²)/(2dx)`, which is the
  middle of the mouth's aperture. The `(r, dx, br)` table mirrors `make_sprites.py`
  so the two can be diffed when the art changes.
- **Render-only for the facing:** nothing reads `Rotation` back on an enemy, so
  this cannot reach the simulation. The muzzle offset *does* move a shot's origin,
  which is a deliberate simulation change and is covered by the canary.
- **Rejected: baking the offset into the sprite** (drawing the crescent off-centre
  so the centre *is* the mouth). It would move the collider off the silhouette.

### D113 — The pause menu overlapped because a widget was appended onto a full column, not because text overflowed  *(2026-08-10)*
- **Diagnosis first, from a screenshot.** `--screenshot 100` on a paused frame,
  read back as pixels, shows `SAVE` painted across `ESC also resumes.` and
  touching `Options: coming soon`. The authored rects say the same thing:
  `pause_save` is `y 204..250`, the ESC caption `y 218..242`, the options caption
  `y 192..216`. Every string was inside its **own** rect — `fit_text_in_rect`
  (D85) did its job and is untouched. Lane K simply appended a widget to a column
  whose vertical budget was already spent, and nothing in the build, the tests or
  the renderer could see it.
- **Decision:** the whole `pause` screen is re-authored on the D88 grid — a
  520x532 panel, the title and rule at the top, a reserved 480..128 band for the
  stat sheet, and RESUME / SAVE / QUIT as one row of three 48px buttons. The dead
  `Options: coming soon` label is deleted rather than relocated.
- **The fix that stops the third report is the test, not the layout.**
  `test_pause_screen.cpp` loads the shipped `GameData.json` and asserts no two
  pause rects intersect — the code-placed stat lines included. A future lane that
  drops a button on top of a caption now fails `ctest`, which is the only thing
  that would have caught either of the first two.
- **z_order is global.** `sort_widgets_by_draw_order` sorts every active screen's
  widgets together, and `gameplay` is always active (D86), so the wider pause
  panel was drawn *under* the hull gauge at z 12. Pause widgets are now z 30 (the
  panel) / 40 (everything on it). Only pause needed it: the shop hides the HUD by
  phase, and the intermission panel is centred clear of the gauge column.
- **Rejected: making the panel small enough to dodge the HUD.** It would cap the
  stat sheet at four lines to work around a draw-order bug.

### D114 — Health packs are green on the radar, read off `Pickup.kind`  *(2026-08-10)*
- **Decision:** `MinimapSystem` picks `minimap_health` (the Bio-lab neon green,
  `80,255,140`) for a blip whose `Pickup.kind` is `Health`, and keeps
  `minimap_pickup` amber for everything else.
- **Why one branch and not a second scan:** the pickup pass already has the
  entity in hand; asking it one more question costs a component lookup and no
  extra iteration. Shield packs deliberately stay amber — the report was about
  health, and a third colour on a 96px radar starts costing more than it tells.
- **Colour comes from a style id**, never a literal in the system (ui-context).

### D115 — The minimap's margins are equalised in WINDOW pixels, so its rect runs past the design canvas  *(2026-08-10)*
- **Decision:** `minimap.x` moves 688 -> 731.3, which puts the map's right edge
  20px from the *window* edge, matching the 20px the top edge already had.
  `x + size` is therefore 827.3 — past the 800-wide design canvas, on purpose.
- **Why the map looked off-centre while the numbers looked right:** at 688 the
  map sat 16px from the canvas's right edge and 18px from its top, which reads as
  equidistant on paper. But `ui_canvas_transform` letterboxes 800x600 onto the
  980x660 window at scale 1.1 with a **50px x-offset and no y-offset**, so the
  real margins were 68px and 20px. The player judges the window edge; the canvas
  edge is invisible to them.
- **Kept in data, not computed in the system.** Deriving x from the transform
  every frame would make an authored balance number dead config, and the window
  size is pinned by the project's one-window-size authority. `ponytail:` the
  ceiling is exactly that — if the window ever becomes resizable, this number has
  to become a computation in `MinimapSystem`, which is already the geometry
  authority (D58).
- **Two existing assertions changed** (`test_minimap.cpp`,
  `test_ui_text_fit.cpp`): "fits the design canvas" becomes "fits the window, with
  equal top and right margins after the transform", which is what #4 actually
  asks for.

### D116 — The active-item slot is three authored widgets, relabelled — not a sprite  *(2026-08-10)*
- **Decision:** the bottom-left slot (#13) is a 100x100 `minimap_frame` panel plus
  two labels on the `gameplay` screen. `PauseStatsSystem` writes a short tag
  (`MISSILES` / `LASER` / `REPULSOR`) and, along the bottom of the square, the key
  and the cooldown (`[E] 13s`, `[E] READY`).
- **Why no icon:** this project has no icon art and no icon font (ui-context), so
  the alternatives were a new generator sprite or a text tag. The tag also carries
  the two things the item's picture could not — whether it is ready, and what
  fires it.
- **`AUTO`, not `[E]`, for the repulsion device.** It has no key by design (D71):
  it fires itself below 20% hull, and printing a key the player would press is a
  lie they would act on.
- **Hidden the way everything else here is hidden** — a zero-size rect, gated on
  the same `hud_visible_in_phase` (D86) as the rest of the arena furniture, plus
  "an active is actually held". Labels sit at z 21, above the frame's 20, or the
  frame paints over them.

### D117 — The pause stat sheet is a pure function plus a pooled label strip  *(2026-08-10)*
- **Decision:** `pause_stats::stat_lines()` turns a flat `Snapshot` into at most
  17 strings; `PauseStatsSystem` owns 17 pooled `UIElement` labels on the `pause`
  screen, created once and relabelled every frame. The line geometry is
  `pause_stats::line_rect(i)`, which the screen test compares against the
  authored rects.
- **Why the labels are code and not JSON:** 17 near-identical widget blocks in
  `GameData.json` is the sort of authored repetition that goes stale the first
  time the row height changes, and the strip has no per-row knobs a playtest
  would want. The minimap blip pool (D58) set the precedent for a code-created
  widget pool on a data-authored screen.
- **Why widgets and not HUD `Text`:** `UIRenderSystem` composites after
  `HUDSystem`, so a text row would be drawn *underneath* the pause panel (D63 is
  the same trap from the other side).
- **Only purchased upgrades are listed, with the cumulative effect** ("Hull
  Plating x2   +50 hull"), phrased from the catalogue's `effect` string (D26) so
  a re-ordered catalogue cannot mislabel a row. An unpurchased row is absent, not
  greyed: the sheet answers "what am I flying", not "what could I buy".
- **Prestige (#14) is a line that does not exist yet.** `Snapshot.prestige` is
  read from `blackboard["meta.prestige_level"]`, which nothing sets today, so the
  line is absent. Lane O publishes that key and the row appears — no edit to
  either lane's files. This is the only part of #5 that is not shipped.

### D120 — The dash moves to SPACE, and the title screen keeps its start key  *(2026-08-10)*
- **Decision:** the thruster dash fires on **SPACE**, edge-triggered, on a **10 s**
  cooldown (`dash.cooldown` 2.5 -> 10.0). LSHIFT survives as the scripted/headless
  alias only. Supersedes D57's "held while LSHIFT is down".
- **Why the two SPACEs cannot collide:** the dash hook runs only inside
  `PHASE_PLAYING && sim`, and the title / game-over branches run only when it does
  not — one frame, one meaning. The one press that could span both is "press at
  the title, still held on the run's first frame", and that is why the trigger is
  the same one-frame `space_edge` the title consumes rather than the held key: the
  edge is already spent, so the press that started the run cannot also dash.
- **Held-to-repeat is gone on purpose.** At 2.5 s a held key re-dashing off
  cooldown was a feature; at 10 s it would fire a dash the player pressed for ten
  seconds ago.
- **LSHIFT stays scripted** because a scripted `SPACE` also means "start the run"
  (`scripted_advance`), so a headless script would otherwise have no way to ask
  for a dash on its own. That is how this was verified — see the spec.
- **Rejected:** consuming `space_edge` in the dash hook. It would have made the
  hook's ordering load-bearing for the title screen, which is exactly the coupling
  the hook blocks exist to avoid.

### D121 — The mine blast is 100px, not 150  *(2026-08-10)*
- **Decision:** `MINE_BLAST_SIZE` 150 -> 100 (a box, so 75px -> 50px of reach).
- **Why:** at 75px the blast still caught a drone that had already read the mine,
  turned and left; the trigger radius (90) is the tell, and the punish reached
  further than the tell. It stays a constant rather than becoming a JSON knob for
  the reason D68 gave for the whole block.

### D122 — A mine is destroyed by shooting it, without becoming an enemy  *(2026-08-10)*
- **Decision:** an **armed** mine detonates where it stands when a player
  `ProjectileTag` overlaps it, and consumes that shot. No `Health`, no `Collider`,
  no `EnemyTag`.
- **Why not the obvious "give it Health and let ProjectileHitSystem hit it":**
  that path is gated on `EnemyTag`, and `EnemyTag` is what the wave-clear check,
  the minimap, the arena clamp and `drop_loot` all key off. A mine wearing it
  would stall a cleared wave and pay out coins. A `Collider` alone would also make
  it a surface that eats ricochets. The direct overlap test in the branch that
  already owns the mine's other trigger is the smaller, safer diff.
- **Detonating rather than fizzling** is deliberate: shooting a bomb should set it
  off, and the blast only carries `HAZARD_MASK = PLAYER`, so a mine popped from
  range is a safe clear. In the drone's face it is not, which is the tradeoff.
- **The arm delay gates both triggers**, so a mine dropped into a stream of fire
  is not deleted on the frame it lands.

### D123 — A purchase shows up on the drone's engine plume  *(2026-08-10)*
- **Decision:** `upgrade_visuals.hpp` maps `ShipState.upg_counts` onto a 0-4 tier
  and writes it onto the player's existing thruster emitter every playing frame:
  cold cyan and small at tier 0, hot white-gold, bigger, longer-lived and ~3x
  denser at tier 4. The shop's drone-preview glow reads the same ramp, so the
  purchase is visible in the frame the credits are spent.
- **Why per-frame and idempotent** rather than applied inside `ShopSystem::apply`:
  `run_save` restores `upg_counts` on a resumed run, so a purchase-time hook would
  need a second application site at `start_run` — the mistake D50 names.
- **Tier 0 is asserted equal to the emitter `spawn_world` writes**, or a fresh
  drone would change look on its first frame.
- **The dash keeps the emission rate** while a burst is burning; only colour, size
  and lifetime are rewritten then. `tick_dash` restores the tier's rate, since
  that is the value it captured.
- **Rejected:** tinting the hull (`Flash` removes a player `Tint` when a hit flash
  expires, so it would survive until the first hit), and new sprite art (Lane L
  owns the generator). ponytail ceiling: swap `look_for` for a kitted-hull sprite
  when that art exists.

---

## Iteration 5 — Lane O: 30 waves + prestige (#14)

### D125 — The arc is 30 waves, regenerated from the same formula  *(2026-08-10)*
- **Decision:** `waves` drops 50 -> 30 rows: 1-15 fixed-count (10 -> 40 enemies,
  spawn_interval 0.50 -> 0.32, hp_mult 1.0 -> 1.42), 16-30 timed (duration
  20 -> 36 s, spawn_interval 0.30 -> 0.216, hp_mult 1.45 -> 2.15, speed_mult
  1.0 -> 1.25). Bosses on **10 / 20 / 30**; 30 is the finale.
- **Why:** the same linear-ramp family as D53, sampled over 30 rows instead of 50,
  so monotone pressure is still true *by construction* and `test_wave_arc.cpp`
  keeps asserting it. Regenerated by formula and spliced by line range — 30
  hand-typed rows is the failure mode D53 was written to avoid.
- **The endpoint is dialled back ~10%** (old wave 50 was hp 2.364 / speed 1.288 /
  0.204 s). The `% 5` shop cadence is unchanged, so the arc now has **6** shop
  stops instead of 10: the player reaching the last wave has banked ~60% of the
  upgrades they used to. Same curve, slightly lower ceiling.
- **Bosses stay on the tens** rather than being re-spaced. Waves 10/20/30 are
  literally the old bosses 1-3, at the same `health_growth` ordinal, so no boss
  number needed retuning — the third one simply becomes `final` (x1.8 HP, +4 adds
  per summon) because `boss_ordinal` derives that from the table.
- **Rejected:** truncating the 50-wave table at 30. The arc would end on the old
  wave-30 pressure — a finale softer than the fight two waves earlier.
- **Rejected:** re-spacing bosses to 8/16/24/30 for a 4-boss arc. It desynchronises
  the boss cadence from the `% 5` shop cadence, which is the one relationship the
  table was authored around (D55).

### D126 — Everything wave-indexed rescales with the table, in one pass  *(2026-08-10)*
- **Decision:** arenas re-key to `first_wave` 1/4/8/12/16/19/23/27/**30**, and the
  moon shooters' injection waves 3/15/30 become **3/9/18**.
- **Why:** both are wave *fractions*, not wave numbers. Leaving the Singularity at
  50 would make it unreachable, and leaving moon_3 at 30 would introduce the
  hardest shooter tier on the final wave only, where it reads as noise next to the
  boss. Scaling everything by 30/50 in one pass is how the two passes still bracket
  the fixed/timed boundary: pass 1 covers the 15 fixed-count waves, pass 2 covers
  the timed ones, and the void is the finale.
- **Resolves the D53 open item by shrinkage, not authoring:** waves 26-50 were the
  1-25 layouts rotated 90 degrees. That second pass now spans waves 16-29 — half as
  many waves, same mechanical layouts. They are still **PROVISIONAL** and still
  deserve a real playtest; nothing here makes them designed.

### D127 — Prestige level is stored; everything it implies is derived  *(2026-08-10)*
- **Decision:** `saves/meta.json` gains exactly one field, `prestige`, beside
  `lifetime_score`. The hull/speed/damage multipliers are computed from it in
  `prestige.hpp` and never written.
- **Why:** D80/D81's discipline is "store the irreducible fact, derive the rest".
  A prestige level records a *choice* — nothing else in the game can reconstruct it,
  so unlike ship unlocks it has to be stored. What it *buys* is exactly the kind of
  thing that desyncs when duplicated: retuning +10% per level must take effect for
  every existing save, not just new ones.
- **Tolerant like the rest of the file:** the level is clamped to 0..5 on load, so a
  hand-edited `"prestige": 999` is a level 5, and a pre-prestige save file loads at
  level 0 rather than failing.
- **Rejected:** a third save file. One integer joins one integer.
- **Rejected:** deriving prestige from lifetime score (e.g. one level per 10k). It
  would hand out the buff to a player who never chose to restart, which is the whole
  bargain — and it would make "prestige" a synonym for "score", not a decision.

### D128 — The offer is a screen, and it is handled above the phase machine  *(2026-08-10)*
- **Decision:** finishing the arc raises the `prestige_offer` screen (panel, live
  `prestige_line` label, PRESTIGE RUN button). The `HOOK: prestige` block sits
  **after `ui_system.update` and before the phase machine**, and consumes both
  `UI_CLICK_KEY` and the frame's `advance` flag when the button fires.
- **Why:** the victory branch treats any `advance` (click or SPACE) as "retry", and
  the click that presses PRESTIGE RUN *is* an `advance`. Consuming both in one place
  is the only arrangement where the two can never fire on the same frame — the same
  trap the title screen hit with SPACE (D50's note) and the shop hit with B (D30).
- **Why a screen and not a HUD banner:** the victory banner lives in
  `game_hud_system.cpp`, which iteration 5 gave to another lane, and the screen
  stack + `UI_CLICK_KEY` is already the house mechanism for every other choice the
  player makes (D47, D71, D102). `main.cpp` rewrites the level line every frame the
  way `BossSystem` rewrites its reward buttons — the level is never authored in JSON.
- **Known edge, accepted:** Escape at the victory screen pops the offer and it does
  not come back until the next win (`UIElement` has no visibility flag; D82 hit the
  same wall). Retry still works.

### D129 — The buff lands at the one `start_run` site, and is linear  *(2026-08-10)*
- **Decision:** `apply_prestige(config.player, meta.prestige)` runs in `start_run`,
  immediately after `apply_ship`, from the freshly re-copied pristine `base_config`.
  Per level: **+10% hull, +5% speed, +8% damage**, linear, capped at 5.
- **Why:** exactly D50's rule. `apply_prestige` is as non-idempotent as
  `apply_ship` and `apply_difficulty`; a second application site (say, on the
  restart-click path) would compound silently and only show up as "the game feels
  easier the longer you leave it open". A unit test pins the non-idempotence as a
  documented property rather than a surprise.
- **Linear, not compounding:** 1 + 0.10L is a number the player can predict from a
  menu line; 1.1^L is not, and it makes level 5 worth more than levels 1-4 combined.
- **"Strips upgrades" is free:** shop purchases live on the per-run `ShipState` that
  `spawn_world()` rebuilds. No teardown code, no reset list to forget.
- **The three rates are constants in `prestige.hpp`, not a `GameData.json` block.**
  A JSON knob costs a `GameConfig` field plus a parse in `arena_config.cpp` —
  shared-lane surface during a parallel iteration — for three numbers nobody has
  played yet. Marked `// ponytail:` with the upgrade path.

### D130 — Prestige is the first persistent value that reaches the simulation  *(2026-08-10)*
- **Decision:** the replay canary is defined **at a fixed prestige level**, and
  `start_run` prints `Prestige: N` on its own line so a headless run states the level
  it flew at.
- **Why:** Lane F could promise a byte-identical replay with or without
  `saves/meta.json` (D82/D83) because lifetime score only changed what the *menu
  offered*. A prestige bonus changes `config.player`, so that promise cannot hold
  across levels and pretending otherwise would make a real divergence look like a
  determinism bug. Verified: two runs at level 0 are byte-identical, two runs at
  level 3 are byte-identical, and level 0 vs level 5 genuinely diverges (at
  `--stopframe 1150` the stock drone is dead and the level-5 drone is alive).
- **Its own line, not appended to Lane F's "Run start:" line**, so the canary's
  comparison target grows rather than changes.

### D131 — Prestige state is Lane O's, the screen that shows it is Lane M's  *(2026-08-10)*
- **Decision:** the public surface for anything that wants to *display* prestige is
  `prestige_summary(level)` plus the Blackboard key `PRESTIGE_LEVEL_KEY`
  (`"prestige.level"`, written once per run by `start_run`).
- **Why:** the stat overview (#5) belongs to another lane in the same iteration.
  A getter on a header and a Blackboard key is a contract neither lane has to edit
  the other's file to honour — the same shape as every other cross-system read here.
- **Rejected:** a `PrestigeSystem`. There is no per-frame work: the level is read
  once at startup, applied once at run start, and displayed from a string.

### D132 — The prestige seam is pinned by a test, because neither lane could see it  *(2026-08-10)*
- **What broke.** Lane O owns the prestige state and Lane M owns the sheet that
  shows it (D131). They shipped disagreeing on both halves of the contract:
  M read `bb.get_or<int>("meta.prestige_level", 0)`, O wrote
  `bb.set<double>(PRESTIGE_LEVEL_KEY /* "prestige.level" */, ...)`. Wrong key AND
  wrong type, so the row silently read 0 and never appeared. Both lanes' gates
  were green — file ownership kept them from ever compiling against each other.
- **Decision:** the integrator fixes the *consumer*, not the producer: O's
  `PRESTIGE_LEVEL_KEY` and `prestige_summary()` are the published accessor named
  in its merge report, so `pause_stats.cpp` now includes `prestige.hpp` and reads
  through the constant. A key typed twice is a key that desyncs twice.
- The sheet renders `prestige_summary(level)` as a whole line rather than a
  label/value pair — #5 asked for the *bonuses*, and "PRESTIGE 2" alone is the
  level without the thing the player wanted to read.
- **The test is the actual deliverable.** `test_pause_screen.cpp` now writes the
  blackboard exactly as `start_run` does and asserts the sheet reads it back.
  A cross-lane seam with no test is a bug that waits for a playtest.
- **Rejected:** changing O's key to M's name (M's `meta.` prefix is a lie — the
  value is the *run in progress*, not the save file), and leaving M reading a
  literal (it is the third copy of a string that must match one other place).

---

## Iteration 6 — the modular drone

### D133 — The chassis is redesigned to wear the kit, and the kit is followers  *(2026-08-10)*
- **Decision:** `player_frames()` is replaced with a slimmer, longer chassis whose
  rotor pods sit further outboard, and which draws its **hardpoints empty** — two
  flank rails and a tail socket. Each shop upgrade row gets a single-frame overlay
  (`kit_*.png`) authored in the chassis's own 128-space, worn by a **follower
  entity** that copies the player's position and rotation every playing frame.
- **Why a redesign and not retrofitting:** the shipped drone had no room. Parts
  bolted onto it collided with each other and with the pods, and the fix kept
  being "move this part somewhere worse". Giving the hull an authored station per
  part — muzzle, collar, flank rails, spine, tail socket, tail corners — is what
  makes seven simultaneous parts legible at 52px.
- **Why followers and not baked variants:** `max_stacks` runs 2..8 across eight
  rows. Baking the combinations is ~1.6M sprites. Followers cost one entity per
  part, created once per run.
- **Why not components on the player:** an entity renders exactly one sprite
  (`SpriteSheet > Images > Color`), and the player's is the chassis.
- **Parked is zero size**, not destroy/create — D58's pooled-blip idiom.
- **Presence, not level:** a part shows when its row has been bought at all. The
  plume ramp (D123) already encodes *how much* was spent; making both encode the
  same thing would be two visuals driven by one number.
- **Rejected:** compositing the kit into one texture at purchase time. It needs an
  engine-level surface composite and a second application site at `start_run`
  (run_save restores `upg_counts`) — the mistake D50 names.
- **The chassis halo is tighter than the old drone's** (0.085/120 vs 0.14/150):
  the slimmer hull leaves more empty frame, and at the old spread the four pods'
  halos merged into a haze over the whole tile, which read on a dark floor as a
  lit square rotating with the ship. Confirmed against captured frames.

### D134 — The shield is a live field, and its four states are one strip  *(2026-08-10)*
- **Decision:** the Shield Capacitor has **no static part**. It is a ring at radius
  70 in the chassis's 128-space, drawn from a 21-frame strip: `hum` 0-7, `hit`
  8-11, `down` 12, `regen` 13-20. The sprite is 192px and worn at
  `FIELD_SIZE_MULT` (1.5x the player size) — that ratio is what holds the ring
  clear of the hull instead of on it.
- **Why not a bolt-on:** a shield *has state*. A static overlay says "you own a
  capacitor" and then lies for the rest of the run — it looks identical whether
  the shield is full, broken, or three seconds into a recharge.
- **One strip, no clips, no `Animation` component:** the four behaviours are a
  loop, a one-shot, a static, and a progress bar. Only the first is what an
  `Animation` models. `main.cpp` writes `SpriteSheet.current_frame` from
  `upgrade_visuals::field_frame`, which is pure and unit-tested against every
  (state, phase, fraction) including out-of-range input.
- **Regen is indexed by FRACTION, not elapsed time**, so the ring is a readout of
  `shield / shield_max` rather than an animation that happens to run alongside it.
- **The hit window reuses `shield_delay`** — PlayerDamageSystem already sets it to
  the full quiet time on every hit and `tick_shields` counts it down, so "struck a
  moment ago" is "delay is still near the top". No second timer to desync.
- **The bloom is directional:** `PlayerDamageSystem` publishes
  `player.hit_bearing` and the ring rotates to it. Write-only and cosmetic —
  nothing in the sim reads it, so the canary cannot move. The rest of the ring is
  radially symmetric, so the rotation is invisible except during a bloom.
- **Rejected:** baking eight bearings of the bloom. One rotation beats eight
  frames of the same art.


### D135 — The field manual is data-driven, media-real, and derives its palettes  *(2026-08-10)*

`docs/features.html` was redesigned from scratch (direction chosen by owner
interview: "refined neon arcade" — evolve the identity, don't replace it).
Three structural calls:

- **Content is one JSON blob** (`<script type="application/json" id="gamedata">`)
  — enemies, upgrades, gear, actives, arenas, keybinds, reference rules, the
  wave-band map. ~90 lines of vanilla JS render everything at load. Game update
  = edit data, never markup. Prose stays hand-authored inside the SECTION
  fences. **Rejected:** a Python docs generator (build step for one page) and
  hand-authored card HTML (drifts, and was the old page's slop tell).
- **Media is real captures**, produced headlessly: `--screenshot` + scripted
  input (`docs/media/capture.sh`), with a temporary capture buff in
  GameData.json (damage/fire-rate/hull up, contact damage down, credit values
  up) reverted via `git checkout` after the run. Stills PNG, clips animated
  WebP in `<img>` (no ffmpeg on this machine; ImageMagick does WebP).
  **Rejected:** placeholder frames — the single loudest "unfinished" signal.
- **Each arena palette is 5 authored colours; everything else is derived** via
  `color-mix(in oklab, ...)`, and the accent tokens are `@property`-registered
  so the palette switch crossfades. Fonts are vendored OFL variable fonts
  (Orbitron / Space Grotesk / JetBrains Mono), latin subsets, ~65 KB total —
  page works offline. Motion is native CSS scroll-driven (reveals + the rail's
  charge bar), gated behind `@supports` + `prefers-reduced-motion`; no JS
  animation libraries. **Rejected:** CDN fonts (offline break), AOS/GSAP-style
  libs (the research pass flagged them as generated-page tells).

Verified with headless Chromium (Playwright, installed to the user cache):
zero console errors, all four palettes, mobile strip, bestiary gauges, arena
bands, both clips. The relative-numbers rule (see the file header) survives
unchanged; the bestiary gauges are 0-5 relative boxes, not stats.

### D136 — Arena props are one bespoke shape per theme, and the wall faces inward  *(2026-08-10)*

The wall ring, obstacles and hazard vents shared two sprites (a rounded square
and a spoked circle) re-tinted five ways, and the boundary reused the obstacle
art. Replaced with fifteen sprites — `{wall,pillar,vent}_{theme}` — in
`make_backdrops.py`, authored at 4x supersample and BOX-downsampled (the
`make_sprites.py` pattern). No RNG, so regeneration is byte-stable.

- **Walls are a new, directional family.** Outer plating at the image top, lit
  inner face at the bottom, and `spawn_arena_props` now adds
  `Rotation{a - π/2, 0, false}` so each of the 97 segments faces the ring
  tangent — the boundary reads as one continuous rampart instead of a ring of
  crates. Seam rule: each segment's outer ~26px (of 96) on both sides is plain
  full-width banding, so the ring's 20-unit overlap lands on identical pixels.
  Cosmetic only; the walls still have no collider and the arena clamp is
  unchanged. **Rejected:** radially symmetric art with no rotation (can't do
  "architecture"), and per-segment art variants (nothing selects variants).
- **Obstacles are designed for their real stretch.** The engine stretches one
  PNG to every layout AABB, and Foundry's layouts are 1:3.6 bars — so
  Foundry's girder is built to elongate into a beam, while Core (cooling
  stack), Bio-lab (specimen tank), Prism (crystal cluster) and Galaxy
  (cracked monolith) live on square-ish footprints.
- **Hazards differentiate by silhouette**, because `main.cpp` force-tints them
  additive red and only shape/luminance survive: breach star / lava grate /
  acid pool / shard spikes / void rift.
- `make_backdrops.py --props-only` regenerates the fifteen without touching
  the backdrops (the committed core/foundry/biolab bg_* still predate the
  seed_for fix and a full run would rewrite them).

Verified: build clean, ctest 8/8, replay canary byte-identical twice (and
again after the capture-buff revert), and headless captures — the Foundry
rampart curves continuously, the Core→Foundry shift dissolves old props and
grows the new wall mid-fight, and the red-tinted vents keep their shapes.

### D137 — The title is a hub, and title screens replace instead of stacking  *(2026-08-10)*

Main-menu suite (spec: `specs/main-menu-suite.md`), shipped as three
gate-green phases. The calls that will matter later:

- **Title screens use CLEAR_TO, never PUSH.** They fully overlap on the
  canvas and a stacked lower screen still renders — the hub's pulsing PLAY
  bled through run_setup's panel. The stack's modality machinery is for
  gameplay overlays; the title is a state machine of full screens.
  **Rejected:** an engine visibility flag (out of scope by spec, D82's wall
  stands) and per-screen opaque cover panels (fights the alpha-242 style).
- **The "ghost" ui_style is how a button hides.** Buttons always fill their
  bg rect with blend NONE, so the Lane-K blank-subtitle trick painted an
  opaque black bar the moment the widget moved inside a panel. ghost's bg IS
  the panel colour; captions are blanked too because the text path ignores
  style alpha.
- **Slots: first-empty for fresh runs, own-slot for loaded ones, newest
  (saved_at) for CONTINUE.** saved_at is caller-set wall clock, write-only
  into the sim (D80). Legacy run.json migrates to slot 1 once, never
  overwriting.
- **Settings gate the applied effect, not the computation.** The shake rng
  draws whether or not shake is shown, so a toggle cannot shift a single
  later draw; the minimap reuses its zero-size hide. Defaults = pre-settings
  behaviour, so canaries and old scripts are unaffected.
- **Scripted-input limitation, now on record:** menu_paused stops the frame
  counter, so a --clicks/--keys frame after ESC never arrives — scripts must
  schedule pause-screen clicks AT the frozen frame. This bit Phase B's E2E
  and explains why Lane K verified SAVE by unit test only.
- SPACE keeps meaning "Normal run, now" from any title screen: it is the
  replay canary's entry path and its meaning is pinned by the baseline diff.

### D138-D151 — Engine feature suite  *(merged 2026-08-15)*

Merged onto this branch from `engine-suite-build`. The entries themselves are
NOT duplicated here: they live in `specs/engine-feature-suite.md` (the umbrella
spec, one section per lane) and `handoff-engine-suite.md`, with the frame-order
consequences in `ENGINE.md`. This heading exists so the id range is not a
silent hole between D138 and D180.

Covered: timescale/bullet time (D139), resonance grid (D140), `--suite` (D141),
adaptive director, flight report, force fields (D144), destructible arena,
palette engine (D147), surges, bullet patterns (D148-D149), chip-synth audio
(D150), first playtest batch (D151 — grid revised, scars cut, audio shelved).

**Everything ships inert**: `director`, `resonance`, `timescale`, `palettes`,
`audio` and `flight_report` are `enabled: false` in GameData.json, and `forces`
is inert by shape (no registered sources, so the pass iterates nothing).
`--suite` flips them all on at once for a playtest. That is what keeps the
replay canary byte-identical to pre-suite master — proven by `gate.sh`.

**D152-D179 are still free.** D180+ is the gameplay-polish/distribution range
below; next free there is D221.

### D181-D191 — Gameplay-polish batch: readability, shop UX, boss waves  *(2026-08-10)*

Twelve playtest items shipped on `feature/gameplay-polish` (branched off
master; the engine-suite branch separately reserves D138-D180, so this batch
starts at D181). One entry — the calls are small and share one theme: the
game should *show* its state, not make you infer it.

- **D181 — equipment visuals run after movement, not after aim.** The kit /
  field / aura follow block copied the player's pre-movement Position and
  rendered a frame behind (bug 002). It is now `update_equipment_visuals()`,
  called after clamp + push-out in both the playing and intermission
  branches. **Rejected:** per-follower interpolation — the followers are
  copies, not physics.
- **D182 — a boss wave spawns nothing but the boss.** The spawner's quota
  reads a `boss_engaged_` latch raised by `set_clear_hold`; suppression can
  therefore never skip a boss the resume path hasn't spawned yet. Adds
  pacing stays data (`boss.summon_interval` / `summon_count`).
- **D183 — timed pickups blink out their last 3 s**, hard on/off Tint alpha,
  period 0.6 s → 0.12 s, driven only by remaining Lifetime. **Rejected:**
  alpha fade (feedback.hpp: pops on both render paths).
- **D184 — player shots are the complement of the ship's hull hue**
  (`ShipDef.color` in GameData → Blackboard `ship.shot_*`), supersedes
  D108's literal red; the Standard drone's complement + brightness offsets
  reproduce D108's exact `{255,70,60}`, so the default look is unchanged.
- **D185 — every enemy shot is red.** One hue always means "hurts you";
  tiers read by speed/size/tracking/pierce and brightness, not hue.
  **Rejected:** keeping the pink/amber/violet tier palette.
- **D186 — every enemy faces its heading** (Rotation from steered velocity
  in EnemySeekSystem; shooters' aim-facing writes win later in the frame).
  The scout hub got a nose in `make_sprites.py` so it has a front to point.
- **D187 — poison patches wear a red-rimmed blotchy cloud sprite**
  (`hazard_poison.png`); the flat green rect stays as load fallback.
- **D188 — pause sheet stat pips**: 5 circles, `round(5*owned/max_stacks)`,
  and upgrades print the percent they add. Strings only; the 17-label pool,
  rects and z untouched.
- **D189 — shop purchases are press-and-hold (1 s)**, click only selects; a
  6 px fill strip rides the held card and the detail pane says HOLD to buy.
  `UIState.pressed` is sticky for the whole press, so no engine change. The
  1-8 digit path keeps instant buy for headless scripts.
- **D190 — the shop preview wears the kit** (same 7 followers as the flying
  drone) and hovering an UPGRADES row lights the part it would buy. The
  shield ring is not previewed: live-state animation, no static frame.
- **D191 — shop cards are a two-column table** of pooled labels over
  caption-less hit-target buttons: name/price at a fixed left edge (fixed x
  is the alignment; no tab stops, D85 stands), pip meter right, restyled
  `pip_gain`/`pip_loss` on hover preview. **Rejected:** space-padding
  against the proportional font (drifts, the pause sheet's old hack).
- **D192 — the second playtest batch (12 items, one entry).**
  1. *Hold-to-buy is the whole card.* D189's 6 px full-brightness strip read
     as decoration; the progress is now a dim wash (`hud_hold`, alpha 60)
     sweeping the card's full rect at z 40, under the D191 text columns.
  2. *A violet ship gets a violet atlas.* `ShipDef.color` was catalogue-only
     and every ship wore the cyan chassis, so "Purple Gatling" flew in blue.
     `player_frames()` now takes hull/accent/trim and emits a second atlas,
     `player_drone_violet`. **Rejected:** a runtime `Tint` — the art bakes
     cyan and cyan × violet is blue, which is the bug, not the fix. The
     defaulted call is byte-identical, so `player_drone.png` did not change.
  3. *The boss is a drone, not a ship* — HUD banner, boss-bar label, comments
     and `docs/features.html`.
  4. *A bomb hurts the swarm too.* The mine blast fans one-shot `DamageEvent`s
     over a circle at detonation. **Rejected:** widening `HAZARD_MASK` — the
     patch would then also hit the enemy that dropped it every frame it lived,
     and the mask is what keeps the arena vents player-only.
  5. *The blast is a mushroom cloud with a red rim* (`hazard_blast.png`),
     worn by the mine and by the boss's borrowed miner signature; the flat
     orange rect stays only as the load fallback.
  6. *The boss's spit wears the D187 cloud* — the one patch site that release
     missed, so the fight still threw green squares.
  7. *Poison reads as gas.* Twelve uneven lobes, Gaussian-blurred, over a 2 px
     alpha-105 red haze instead of a 5 px opaque rim on six equal lobes (which
     outlined every lobe and read as a flower). Bubbles deleted.
  8. *The boss has a health bar.* `BossSystem` publishes `boss.hp_frac` /
     `boss.name`, cleared at the top of every `update()` so each early-out
     collapses it; `GameHUDSystem` owns the three bottom-centre widgets.
     Bottom-centre because the top strip already carries HULL, the wave
     counter and the minimap, and the label needs a row of its own.
  9. *Primary fire runs off a battery* (`battery` block: 12 s of continuous
     fire, 3 s empty→full at a constant rate). Emptying it LATCHES a lockout
     that clears only at full — that lockout is the entire cost of holding the
     trigger. Two floats on `ShipState`, two rates on the Blackboard, so
     `PlayerFireSystem` stays config-blind (the `ship.extra_shots` pattern).
     Third bar in the HUD gauge stack; the text rows moved down 20 px.
  10. *A dash charge per boss.* `dash.charges` seeds `ShipState.dash_max`;
     `BossSystem` grows it on every kill. One cooldown clock refills the stack
     one charge at a time, and spending a second charge does not rewind the
     one already regenerating. Persisted in `RunSave` so a resumed run does
     not re-earn them.
  11. *The currency is a UNIT.* `pickup_coin.png` is now a hex data-chit with
     circuit traces, not a struck coin; every user-facing "Credits" is
     "Units" (HUD, shop, how-to-play, intermission, headless summary). The
     gold hue is unchanged, so nothing downstream had to move.
  12. *Loot lingers*: `pickup_lifetime` 12 → 26 s. 12 s expired mid-fight.
- **D193 — the third playtest batch.** Numbering follows the playtest feedback
  items, not the lanes that implemented them.
  1. *Loot lingers, again*: `pickup_lifetime` 26 → 14 s. D192 #12 over-corrected
     — 26 s left the arena carpeted in uncollected units and killed the
     risk/reward of the fade (D52). 14 s is the pre-D192 12 s plus the 2 s the
     mid-fight complaint actually needed. Pinned in `test_wave_arc.cpp`.
  13. *Units are worth more*: a flat `CREDIT_BASE_BONUS = 2` added to every
     currency drop in `drop_loot`. **Rejected:** a multiplier (the per-type
     values are 1-3, so ×N widens the spread instead of lifting the floor) and
     re-tuning every `enemy_types.currency` field by hand.
  10. *Passive credit vacuum from wave 15*: currency pickups inside 140 px drift
     to the drone at their own `magnet_speed`. It reuses the Magnet Core's
     steering branch with a shorter radius, so the item still owns the long
     (220 px) reach and the all-kinds pull. Two constants on `PickupSystem`,
     no JSON block, wave read from the Blackboard's existing `"wave"`.
  12. *BIG UNITs from wave 15*: 3+ enemies dying in the SAME frame within 180 px
     of each other upgrade that kill's drops to a 15-value unit drawn at 1.6×.
     Same `pickup_coin.png`, no new `PickupKind`, no new sprite. "Same frame" is
     the whole time window — `EnemyDeathSystem` already sees every death of the
     frame in one pass, so the cluster test is a distance count over a pre-pass
     list. **Rejected:** a rolling multi-frame window (needs per-system state
     for a heuristic) and a tinted variant (gold × anything reads as dirty gold,
     the D192 #2 lesson). The pre-pass draws no RNG, so the drop stream and the
     replay canary are untouched.
  2. *The ability row*: the boss-item slot and a new dash button are both 64×64
     on one baseline (was a single 100×100 box), the slot now reads `EMPTY` /
     `BOSS` on the phase gate instead of collapsing to zero when nothing is
     held, and `SPACE` is captioned under the dash. The booster is the glyph
     `▲`, not art — `UIElement` has no texture path, so a real icon would mean a
     new renderer route for one widget. **Open:** whether `▲` reads as a
     booster at 64 px. **Answered by the next playtest: no** — see the revision
     below.
  11. *Dash cooldown indicator*: a horizontal wipe under the glyph, reusing
     `set_bar` — the same fill-over-bg vocabulary as the hull/shield/battery
     gauges. **Rejected:** radial, which needs a new renderer path for one
     widget. `main.cpp` publishes `dash.cooldown` to the Blackboard so the HUD
     stays config-blind. **Superseded — see the revision below.**

  **Revision (next playtest, items 2/11 again).** Three findings, one change:

  - *Smaller still*: both boxes 64×64 → **48×48**, dash moved x 88 → 72, and the
    empty slot says **`ITEM`** on one line instead of `EMPTY` / `BOSS` on two —
    a box that is visibly empty does not also need the word "empty".
  - *The `▲` did not read as a booster, and the horizontal wipe did not read as
    a cooldown.* Both were "no new renderer path" compromises, and the escape
    from that constraint is that a HUD icon does not have to be a **widget**.
    `UIElement` still has no texture path (and is not getting one), but a normal
    sprite entity drawn in *screen* space needs no engine change at all — it is
    the arrangement the shield field already uses: a `SpriteSheet` with no
    `Animation`, whose `current_frame` is written per frame from a pure picker
    (`dash_sweep_frame`, beside `field_frame`). So the button's face is now two
    generated sprites: `hud_boost.png` (an actual rocket, authored nose-**up**;
    `rocket_sprite` faces right only because the runtime spins it) and
    `hud_dash_sweep.png`, a 16-frame clock wipe that greys out the **whole** box
    and sweeps clockwise back to clear. Ready = the dial is parked entirely,
    rather than drawn full — "can I dash?" is answered by the button's own state
    with no second strip of furniture inside it.
  - **The trap:** `CameraSystem` writes `ScreenPosition` for *everything* with a
    `Position`, and `RenderSystem` only reaches entities that *have* a
    `Position` — so a screen-space sprite cannot simply be created and left
    alone (this is the D58 minimap note, from the other side). The placement is
    therefore written in `main.cpp` immediately **after** `camera.update` and
    before the draw, overwriting `ScreenPosition` last. The rect it maps is
    `hud_dash_frame`'s **live** `UIElement.rect` through `ui_canvas_transform`,
    so the sprites inherit the authored geometry *and* `GameHUDSystem`'s phase
    gate (which collapses that rect to zero width) for free — no second copy of
    the layout and no second visibility rule.
  - Both frames moved to a new `hud_slot_frame` style: `minimap_frame`'s rim
    with no smoke behind it. The UI composites *after* the world, so the
    translucent fill was a 43 % veil over the sprites underneath it. Greying the
    box out is the dial's job now, so the frame has no reason to be dark.
  - Removed by this: `hud_dash_icon` / `hud_dash_cd_bg` / `hud_dash_cd_fill`
    widgets, `DASH_CD_FULL_W`, and the `dash.cooldown` Blackboard key —
    `main.cpp` reads `config.dash.cooldown` at the one site that now needs it.
  8. *SPACE no longer fires*: the engine's `input_system.cpp` writes
     `input.fire` from `SDL_SCANCODE_SPACE` and `PlayerFireSystem` OR'd it into
     `firing`. Cut in the game system, not the engine — `Input.fire` is
     engine-general and still reaches Lua via `engine.get_input`; "SPACE is dash
     only" is a Reactor Drone rule.
  9. *Dash reaches further*: `dash.speed` 900 → 1060 (distance 135 → 159).
     Speed, not duration — `test_lane_n.cpp` pins `duration == 0.15` and the
     burst window governs the i-frame and once-per-enemy damage rules.
  4. *Hold-to-buy fills the row*: this was a colour bug, not geometry. The D192
     bar already spanned the card rect, but `card.pressed` flipped the whole row
     to solid cyan and swallowed it. `hud_hold` is now light blue at alpha 120
     and `pressed` is a dark tint, so the sweep is the only thing that moves.
  5. *Stats live in the preview pane*: D190/D191 put pip previews on every store
     row, which was the wrong read of the request. A `DRONE STATS` panel now
     sits in the right-hand pane; idle rows read `value ●●○○○` and hovering an
     upgrade shows `now > after`. Keyed off the catalogue `effect` string
     mirroring `apply()` (D26) — no row index reaches a stat. The drone preview
     shrank to make room.
  3. *Missiles*: `MISSILE_AOE` 130 → 200 with the VFX reach bound to the damage
     radius instead of a fixed 120 px, a proper `projectile_rocket.png` flown
     nose-first off `Rotation`, and a second 16-rocket salvo 0.45 s later offset
     half a step so the waves interleave. `MISSILE_FUSE` 34 → 56 came with it:
     a rocket that physically touches its target is taken by the single-target
     path and never detonates, and the bigger rocket made contact sooner.
  6. *The bubble shield was cropped square*: `_shield_scale()` **stretched** the
     128-unit authoring space to fill the 192 px frame instead of treating the
     bigger frame as a wider window onto it, so the visible window stayed 128
     units wide, the r=70 ring overran all four edges and Pillow cropped it
     flat. One authoring unit now maps to one frame pixel (26 px of margin), and
     `FIELD_SIZE_MULT` 1.5 → 2.25 keeps the on-screen diameter unchanged.
     Verified per frame: bbox (26,26,167,167), zero lit border pixels, was
     (0,0,192,192) with 90 lit pixels an edge.
  *Second round of the same playtest (the row was still too big and there was no
  way to reach late waves quickly):*
  - *Ability row shrunk again*: 48×48, and the boss slot just reads `ITEM`.
  - *Dash cooldown is a circular dial*, not a bar: a 16-frame clock wipe that
     greys the whole box, plus a real `hud_boost.png` booster icon replacing the
     `▲` glyph. `UIElement` still has no texture path, so both ride sprite
     entities — the same frame-index-written-per-frame idiom as the shield
     field. **Trap found:** `ScreenPosition` is NOT camera-free —
     `CameraSystem::update` writes it for every entity with `Position`+`Size`,
     and `RenderSystem` only iterates entities that HAVE a `Position`. So the
     sprites carry a dummy `Position` to get iterated and their `ScreenPosition`
     is overwritten after `camera.update`, immediately before the draw.
     **Rejected:** inverse-transforming the camera (more code, lags under
     shake).
  - *Dev/god mode* behind `--dev` / `--god`: units pinned to 999999 each frame,
     **B** opens the shop any time (waiving the key cost on the existing
     `key_entry` → intermission → shop transition, not a second entry path),
     **F5** skips a wave via the save-resume `resume_at_wave` seek, and
     `--level N` picks the starting wave — it was parsed but entirely unused
     before this. **Rejected:** short-circuiting the shop spend path, which has
     three separate `currency < cost` sites; pinning the balance keeps all three
     real and touched no shop code. Everything is inside `if (opts.dev)`, so the
     default path draws no RNG and adds no per-frame state — canary verified.
- **D194 — read-only assets vs. writable user data split (Task 2b).**
  `CLASS_ROOT_DIR` is the build machine's absolute source path, baked into the
  binary at compile time. Fine for the class Linux workflow (deliberately lets
  the game run from any CWD, D-unnumbered but documented in `ENGINE.md` §5), but
  it made the cross-compiled Windows exe unable to find `GameData.json`,
  sidecars, scripts, images, fonts or saves on any PC but the one that built it
  — the exe crashed before the title screen on a clean machine. Split
  `project_paths` into `assets_dir()` (read-only, ships beside the exe) and a
  new `user_data_dir()` (writable, saves/settings) so each can resolve
  differently on Windows: `assets_dir()` via `SDL_GetBasePath()` against the
  flat install layout (exe + DLLs + `assets/` siblings, `installer/package-win.sh`'s
  contract), `user_data_dir()` via `SDL_GetPrefPath("conradm", "ReactorDrone")`
  since Program Files is not user-writable. Both are `_WIN32`-only branches;
  Linux/dev behaviour is byte-identical (`class_root()` either way), and neither
  path is ever printed to stdout — the replay canary compares the summary line,
  and a resolved filesystem path is exactly the kind of per-run-varying string
  that would break it. **Rejected:** an env var or CLI override for the assets
  path (speculative — nothing in this project needs to relocate assets
  independent of the exe) and moving saves under `{app}`\\saves (not
  user-writable once installed to Program Files). Verified by staging the real
  Windows cross-build, hiding the repo's own `assets/` and `saves/` directories
  so the baked source path cannot be used as a fallback, and running the staged
  exe under Wine to completion, headless and windowed with a rendered
  screenshot; the save landed at
  `~/.wine/drive_c/users/$USER/AppData/Roaming/conradm/ReactorDrone/saves/meta.json`.

### D195 — Identity rides `MetaSave`; registration is server-unique; ESC always defers, never silently registers  *(2026-08-11)*
- **Decision:** player identity (`player_id`, `player_name`, `registered`) is
  three new fields on the existing `MetaSave`, not a second profile file.
  `player_id` is a `generate_uuid()`-produced id, written immediately at first
  startup (before any name is ever chosen) so it survives a player who skips
  registration and relaunches. Name uniqueness is enforced server-side only
  (case-insensitive, `POST /register`'s 409) — the client never pre-checks or
  caches a name list. `PHASE_NAME_ENTRY` (6) is entered once, before the title,
  only when `!meta.registered && net::enabled()`; ESC at any point in that
  phase returns to the title leaving `registered` false and touching neither
  `player_name` nor `meta_write` — a skip always retries next launch and can
  never half-register. Renaming (`N` at the title) reuses the exact same
  phase/submit path, pre-filled with the current name, because the backend
  upserts by `player_id` — there is no separate rename endpoint or code path
  to drift from registration.
- **Why:** `MetaSave` already has the one property this needed — a
  garbage-tolerant load path (missing file, malformed JSON, wrong types all
  fall back to defaults) — so duplicating that tolerance in a second file would
  only be a second place for it to rot out of sync (the same reasoning D80 used
  to keep `MetaSave` to "the only state that outlives a run"). Server-side
  uniqueness (not a client-side reservation/check step) keeps the client dumb
  and avoids a second source of truth for "is this name taken" that could
  disagree with the database under a race between two players. The ESC/skip
  rule exists because a half-registered state (id present, name written,
  `registered` false, or vice versa) would be strictly worse than "ask again
  next launch" — nothing downstream (Task 8's score submission) can assume a
  meaningful `player_name` unless `registered` is true, so the two fields are
  written in the same breath as the 200 response, never separately.
- **Rejected:** a client-side name blocklist/cache (still races the server,
  and the server is the source of truth regardless); writing `registered=true`
  optimistically before the server confirms (would let an offline player
  "register" a name nobody else can ever take); a separate `on_rename_click`
  code path (would double the surface Task 9's leaderboard has to trust).
- **Verified:** `[meta]`-tagged unit tests (round-trip, old-save defaults,
  UUID shape) green; `runTestsAll.py` all-green; warning-free build; the
  replay canary run twice byte-identical (`net::enabled()` is false whenever
  `--stopframe` is set, so headless never touches `PHASE_NAME_ENTRY` or the
  network); manual walkthrough against the live backend — first launch, typing,
  200 registration, relaunch-skips, a real 409 (hand-edited `player_id` +
  `registered:false` retrying the same name), ESC-skip, and `N`-rename with
  ESC-cancel — all observed directly, screenshotted, and logged in
  `task-7-report.md`.

### D196 — Score submission rides the existing `bank_run_score` guard; a run submits exactly once regardless of which of its five call sites fires  *(2026-08-11)*
- **Decision:** the `POST /score` call lives inside `bank_run_score` itself
  (main.cpp), immediately after the local `meta.lifetime_score` accrual it
  already guarded with `run_banked` — not at any of the five sites that call
  it (death, victory, pause-quit, pause-to-menu, window-close/shutdown). That
  guard is what makes the submission exactly-once: every call site can fire on
  the same frame or in sequence and only the first actually runs the body.
  Gated on `net::enabled() && meta.registered`; an unregistered or headless
  player still banks locally with zero network calls and no status text
  implying failure. The submitted score is read into a local (`run_score`)
  once, before either the meta write or the POST body is built, so nothing
  downstream can apply a value the request didn't actually send (the trap
  Task 7's Critical 1 hit with `pending_name`). `pending_score`, the future,
  is declared outside the frame loop like Task 7's `pending_register`, polled
  once per frame unconditionally (not phase-gated, since a submit can still be
  in flight after the player has already backed out to the title), and
  `abandon_future()`'d — Task 7's shutdown-safe graveyard, reused rather than
  building a second one — at every point a new run could start (both
  `start_run` and the gameover/victory retry branch) and at process shutdown,
  since a fast retry or a fast quit can both catch the previous run's request
  still connecting. The status line ("Submitting score..." /
  "Score submitted!" / "Score not submitted (offline)") is Blackboard state
  (`score_submit_status`) that `GameHUDSystem` renders by reusing
  `message_entity_` — the gameplay-hint text row, already blanked on every
  non-gameplay phase — rather than adding a third status widget for the two
  phases (game-over, victory) that ever have something to say there.
- **Why:** the alternative was a `submitted` bool alongside `run_banked`, but
  that would be a second flag tracking the same "has this run's end already
  been handled" question the guard already answers — two flags that must
  never disagree is exactly the kind of drift this codebase avoids (the same
  reasoning D80 used for `run_banked` itself, one call site instead of one per
  caller). Polling unconditionally rather than only in PHASE_GAMEOVER/VICTORY
  matters because pressing ESC or clicking MENU moves the player back to
  PHASE_TITLE while the request from a pause-quit or pause-to-menu bank is
  still in flight; a phase-gated poll would leave it dangling until the next
  run's `abandon_future()` call, which is correct but would show no status
  text at all in the meantime.
- **Rejected:** a second "already submitted" flag (drifts from `run_banked`,
  see above); polling only while `PHASE_GAMEOVER`/`PHASE_VICTORY` is active
  (misses the pause-quit/to-menu paths, which return to the title the same
  frame); a dedicated status-line widget (a third piece of screen furniture
  for two phases when an idle one already sits there).
- **Verified:** `runTestsAll.py` 8/8 green; warning-free build (only Lua's
  vendored `tmpnam`); the replay canary run twice byte-identical
  (`net::enabled()` is false whenever `--stopframe` is set, so headless banks
  locally and makes zero network calls). Manual walkthrough against the live
  backend with a registered test player (`ZZZ_TASK8_TEST`): a real windowed
  run driven to victory via `--dev --level 20` + synthetic F5 wave-skips
  showed "Submitting score..." then "Score submitted!", confirmed landed with
  `curl .../top?mode=high`. A second real run's `POST /score` count was
  verified 1:1 against `runs_played` by pointing `NET_BASE` at a local
  logging HTTP server for two consecutive runs (2 runs → 2 requests, reverted
  after). The offline path was exercised by pointing `NET_BASE` at an
  unreachable address: the line stayed "Submitting score..." until the 8s
  `CURLOPT_TIMEOUT` elapsed, then flipped to "Score not submitted (offline)"
  with the UI still fully responsive throughout. Closing the game window
  (`WM_DELETE_WINDOW`) while that same unreachable request was in flight
  exited the process in 0.24s, proving `abandon_future()` — not the 8s
  timeout — bounds shutdown. `net_config.hpp` was restored to the live
  endpoint and rebuilt before considering the change complete.

### D197 — Leaderboard screen (Tasks 8+9): one live future, always re-abandoned on tab switch, is the whole stale-tab guard; per-row defensive parsing over a whole-response reject  *(2026-08-11)*
- **Decision:** `PHASE_LEADERBOARD = 7` (own phase, not a title sub-screen,
  because it needs its own ESC handling — same shape as `PHASE_NAME_ENTRY`,
  Task 7 Minor 1). `L` at the title (hidden from the hint line and a no-op
  when `net::enabled()` is false, so headless/scripted runs never reach it —
  architecture.md Invariant #4) fetches `GET /top?mode=high`; `TAB` inside the
  screen toggles `high`/`total`. `pending_top` (declared outside the frame
  loop, polled with `wait_for(0s)`, per `net/http_client.hpp`) is the single
  point of truth: `fetch_leaderboard(mode)` is the ONLY place it is assigned,
  and every call abandons whatever was already in flight (`abandon_future`)
  and sets `lb_mode` in the SAME edge as issuing the new request. There is
  therefore never more than one live future, and it always targets whatever
  `lb_mode` currently is — a response for a mode the player has since
  switched away from was abandoned before its replacement request even went
  out, so it can never land in the wrong tab. This is a smaller mechanism
  than Task 7's captured-string approach (`pending_name`) because leaderboard
  fetches have no "identity" beyond which mode they're for, and mode and
  future are updated atomically together.
- **Why:** the alternative — capturing the requested mode in a side variable
  and comparing it to `lb_mode` on arrival, mirroring `pending_name` — adds a
  second piece of state that must always agree with `lb_mode`, for no benefit
  here: unlike a name (arbitrary player input), a mode fetch's target and its
  future are set in the exact same statement, so they can't drift apart.
  Response parsing skips one malformed row (`try`/`catch` per row: bad
  `name`/`score` types, non-array `"rows"`) instead of discarding the whole
  response on a single bad row — a public, unauthenticated `/top` endpoint
  can return anything, and one garbage row should not blank a working list.
  Player names are also sanitised on arrival, not trusted from the wire:
  bytes below 0x20 (control bytes, which includes `\n` — the exact character
  that would otherwise inject a fake extra row into the `"1. name  score\n"`
  format the Blackboard contract commits to) and 0x7F are stripped, and the
  cleaned name is clamped to 40 chars, both before the row is ever formatted.
- **Rejected:** a captured-mode side variable per fetch (no observable
  difference from comparing `lb_mode` directly, given the atomic-update
  argument above — added state with no new guarantee); failing the whole
  response on any row-level parse error (one hostile/malformed row taking
  down an otherwise-good leaderboard); trusting `UIRenderSystem`'s
  overflow-shrink alone to make an unbounded name safe (it prevents a visual
  crash, not a `\n` re-parsed as a row separator by the row-splitting code
  that reads `lb_rows` back at render time).
- **Verified:** `runTestsAll.py` 8/8 green; warning-free build (only Lua's
  vendored `tmpnam`); the replay canary run twice byte-identical. Manual
  walkthrough against the live backend (`DISPLAY=:1`, real windowed run,
  keys driven via XTest since scripted `--keys` has no letter-key vocabulary):
  `L` opened the screen showing "Loading..." then real rows
  (`ZZZ_TASK8_REVIEW 80`, `ZZZ_TASK8_TEST 0`); `TAB` switched the header to
  CUMULATIVE with the same two rows (both players have exactly one submitted
  run, so cumulative == highest, consistent with "cumulative ≥ highest");
  ESC and the BACK button both returned cleanly to the title. Six rapid `TAB`
  presses in under 200ms landed on the mode the toggle count predicts, with
  no wrong-tab flash and no hang (process still running throughout).
  `net_config.hpp` was pointed at a local mock server (reverted after) for
  three states unreachable on the live backend: `{"rows":[]}` rendered "No
  scores yet."; an HTTP 500 rendered "Could not reach the leaderboard.";  and
  a row with `score:"notanumber"` next to a valid row rendered only the valid
  one (`"1. OK GARBAGE  42"`) — the rank counter only advances on rows that
  parse. A targeted injection case — a valid-score row named
  `"EVIL\nFAKE. ROW  9999"` next to a 200-character name — rendered as a
  single row `"1. EVILFAKE. ROW 9999  77"` (no phantom second row from the
  stripped `\n`) and a name clamped to 40 chars, confirming the sanitiser
  runs before formatting. `net_config.hpp` was restored to the live endpoint
  and the binary rebuilt, tests and canary re-run against that final build,
  before considering the change complete.

### D198 — Live-ops dashboard is two read-only routes on the existing Worker, not a separate service; polled HTML with no build step and no chart library  *(2026-08-12)*
- **Decision:** `GET /dashboard` serves one self-contained HTML page (inlined
  CSS + JS, zero external requests) and `GET /stats` returns four aggregate D1
  queries in a single `env.DB.batch`. Both live in `backend/src/worker.js`
  beside the routes the game already calls; the page is a string constant in
  `backend/src/dashboard.js`. The page polls `/stats` every 15 s (paused while
  the tab is hidden) and redraws — the activity chart is hand-rolled inline SVG.
  `/stats` returns `totals`, a gap-filled 14-day `daily` series, the top 100
  `players` (LEFT JOIN, so a registered pilot with zero runs still appears) and
  the 25 most recent runs. It deliberately never returns a `player_id`; the
  only identifiers that leave are the names `/top` already makes public.
- **Why:** the Worker + D1 are already deployed and hold the data, so a second
  service, a build step, or a framework would all be pure overhead for a page
  that renders four queries. Reading at request time (rather than storing
  aggregates) keeps `scores` append-only, which is what makes the per-player
  table possible at all.
- **Rejected:** a chart library (inline SVG is ~40 lines and has no supply
  chain); a static-fixed `viewBox` stretched to fit (`preserveAspectRatio=none`
  distorts labels and corner radii — the chart now draws at the SVG's real
  pixel width and redraws on resize); server-side rendering of the HTML with
  the data inlined (a second code path for the same numbers, and no live
  refresh); auth on `/dashboard` (it exposes strictly less than the already
  public `/top`; if it ever needs gating, Cloudflare Access in front of the
  route beats app code); storing per-player aggregates on write (a denormalised
  copy that can disagree with `scores`, which is D81's rule applied here).
- **Verified:** `backend/test.sh` green with six new assertions, including one
  that fails if `/stats` ever leaks a `player_id` and one pinning the daily
  series at exactly 14 entries. Screenshotted at 1280 px light, 1280 px dark
  and 390 px mobile against both a seeded local D1 and live production — no
  console errors, no page-level horizontal overflow, empty-database state
  renders without NaN. The single series colour passed the data-viz palette
  validator in both modes. Live end-to-end: four headless bot clients banked
  19 runs through the real `bank_run_score` → `POST /score` path and the
  dashboard reflected each one.

### D199 — Cross-platform distribution rides one compile-time switch (RD_PORTABLE), not per-OS path code; Linux ships a tarball with bundled SDL3 + launcher, macOS an ad-hoc-signed .app  *(2026-08-12)*
- **Decision:** a single CMake option `RD_PORTABLE` (default OFF) switches
  `project_paths.hpp` onto the codepath Windows always shipped with:
  `SDL_GetBasePath()`-relative assets and `SDL_GetPrefPath("conradm",
  "ReactorDrone")` saves, on every OS. Dev builds are byte-for-byte unaffected
  — CLASS_ROOT_DIR, project-root saves, run.py, canary and all docs stay true.
  Release CI builds with `-DRD_PORTABLE=ON`. Linux: `fetch-linux-deps.sh`
  builds the pinned SDL3 stack from source (same versions as
  fetch-win-deps.sh), `package-linux.sh` stages exe + the three SDL3 .so's +
  assets + PRIVACY.md + a `run.sh` that sets LD_LIBRARY_PATH; shipped as a
  tar.gz (artifact zips strip exec bits) and an itch `linux` channel. macOS:
  brew deps, `package-mac.sh` builds ReactorDrone.app (assets in
  Contents/Resources, where SDL_GetBasePath points for bundles), dylibbundler
  rewrites the SDL dylibs into Contents/Frameworks, ad-hoc `codesign -s -`
  (arm64 refuses unsigned binaries outright); zipped with `ditto`; two arches
  via runner matrix (macos-latest=arm64, macos-13=x86_64), itch channels
  `mac-arm64`/`mac-x86_64`.
- **Why:** the only real porting blocker was CLASS_ROOT_DIR baking the build
  machine's source path into non-Windows binaries; everything else (SDL3, Lua,
  libcurl, nlohmann) is already portable. One flag beats per-OS path code and
  keeps the class dev workflow untouched.
- **Rejected:** patchelf/$ORIGIN rpath on Linux (run.sh + LD_LIBRARY_PATH is
  equivalent and needs no extra tool); bundling libcurl/TLS on Linux (means
  shipping a CA story; every distro has libcurl); SDLTTF_VENDORED=ON (the
  release tarball ships no harfbuzz sources — system freetype/harfbuzz instead,
  same linkage as the dev build); AppImage (a tarball + run.sh is the same
  double-click-ability for a fraction of the tooling); universal mac binary
  (two plain arch builds via matrix are simpler than lipo-merging brew deps).
- **Verified (Linux, the full CI path locally):** pinned deps built from
  source; game built against them with RD_PORTABLE=ON; staged; copied to an
  alien directory and run with a scratch HOME — assets loaded beside the exe,
  save landed in XDG (~/.local/share/conradm/ReactorDrone), repo saves
  untouched, ldd confirms the bundled .so's are the ones loaded, and a real
  windowed run rendered actual gameplay (HUD, arena, enemies) from the staged
  assets. Dev build re-verified after the header change: canary byte-identical
  to the pre-change baseline, ctest 8/8. **macOS is authored but CI-verified
  only** — no Mac hardware here; the workflow smoke-tests both arches headless
  from an alien cwd on the runners, and the first tag push is the real test.
- **Known limits:** mac bundles are ad-hoc signed, not notarized — first launch
  needs right-click→Open (Gatekeeper); debug logs/ (screenshot/dump/trace) are
  cwd-relative and fail from a read-only cwd, dev-only flags so not a player
  bug; /version still serves one INSTALLER_URL (nothing consumes it yet — make
  it per-OS when the update check lands).

### D200 — Feedback reports: a PHASE_FEEDBACK form on name_entry's plumbing, flat-column D1 table as the AI export, context captured at open, consent = the submit itself  *(2026-08-12)*
- **Decision:** FEEDBACK buttons on the pause screen (row re-laid 4→5 at
  w=86) and the main menu (QUIT row split 392→188+188) open `PHASE_FEEDBACK
  = 8` — four typed fields (subject/body/tags/from), TAB cycles focus, ENTER
  submits except in BODY where it inserts a newline, ESC routes back to
  where the form opened from (`fb_from_pause`: CMD_POP under the pause
  screen vs CLEAR_TO the title). Run context (wave/score/ship/difficulty/
  prestige) is captured AT OPEN — the numbers the player is looking at — not
  at send. POST /feedback stores flat columns (`ts DEFAULT unixepoch()`
  server-side; client clocks are never trusted) plus auto-attached
  version/platform/player identity/session; `session` joins feedback to the
  same launch's telemetry rows. A failed POST keeps the typed content for
  retry. NOT gated on the ANALYTICS toggle: pressing send is the consent
  (PRIVACY.md line added). Offline: both handlers net-gated, the pause
  button's caption blanks.
- **Why flat columns, not a body blob:** feedback is exactly the table that
  gets exported to an AI triage pipeline — one SELECT yields clean JSONL
  with zero json_extract gymnastics, unlike telemetry where new report
  fields must not need migrations.
- **Rejected:** gating on settings.analytics (a form the player explicitly
  submits is its own consent; coupling them would silently drop feedback
  from opted-out players, the people most likely to have something to say);
  capture-at-send (the form can outlive the wave the complaint is about); a
  tag taxonomy (free text now, normalize at ingestion); a feedback
  browser/dashboard tab, edit/delete, rate limiting, offline queue (YAGNI,
  spec's Out of Scope).
- **Verified:** backend 6 new test.sh cases green (auth, both shapes, empty
  subject, bad platform, in_run without run state); live XTest walkthrough
  on DISPLAY=:1 against local wrangler dev — title submission landed
  in_run=0/NULL run state, pause submission landed in_run=1 wave=1 score=0
  ship=0 difficulty=Normal, server-down showed the red failure line with
  typed content preserved and a clean ESC exit; ctest 8/8; replay canary
  byte-identical to the session baseline. Harness facts for the next
  walkthrough: SDL_GetKeyboardState-polled keys need XTest holds ≥3 frames,
  and clicks must map through the 800x600→window ui_canvas_transform
  (scale 1.1, offset_x 50 at 980x660).

### D201 — The pause freeze is stack-wide, and any screen that must accept input while the sim is frozen belongs ABOVE the phase machine  *(2026-08-13)*
- **Decision:** `menu_paused` tests whether SCREEN_PAUSE is anywhere in the
  screen stack, not just on top, so every screen pushed over pause inherits the
  freeze. Correspondingly, PHASE_FEEDBACK's input block moved out of the phase
  machine's `} else if (sim) {` chain to sit beside the pause-button handling,
  which already documents the rule: the phase machine is gated on `sim`, and
  pausing turns `sim` off.
- **Why (both halves are one bug):** the feedback form was the first screen
  ever pushed over pause. A top-of-stack-only freeze test left three `if (sim)`
  blocks outside the phase machine running while the player typed — particle
  ageing with emit=false (the frozen battlefield visibly drained), trauma decay,
  and `timer.end_frame()` advancing sim time instead of
  `end_frame_no_advance()`. Fixing that alone then revealed the severe half:
  with `sim` correctly false, the feedback block stopped executing, and since
  the generic pause-ESC handler excludes PHASE_FEEDBACK, the player was
  **soft-locked on the form** with no way back. The first bug had been masking
  the second.
- **Rejected:** special-casing PHASE_FEEDBACK in the freeze test (leaves the
  same trap armed for the next screen pushed over pause); giving the form its
  own ESC path through the generic handler (two ESC owners racing on one frame
  is the exact bug D195/D197 already fixed for name entry and the leaderboard).
- **Verified** with `scripts/drive_ui.py` (committed with this change) against
  local wrangler dev. Freeze, by frame count — `end_frame()` increments
  `frame_count_`, `end_frame_no_advance()` does not — holding on each screen for
  6 s and 20 s: pre-fix pause 350 vs form 1583 (delta scaled with hold);
  post-fix 350/354 vs 351/353 (flat). Function: a pause-path report landed with
  in_run=1, wave=1, difficulty Normal, tags and from_name intact while frames
  stayed at 347; ESC returned to the pause screen (Phase 1) and RESUME
  unfroze — frames 347 -> 539. ctest 8/8, canary byte-identical.

- **MERGE 2026-08-15 — `visual-overhaul` renumbering.** The two branches ran in
  parallel for weeks and BOTH allocated from the same counter, so D194-D206 each
  meant two unrelated things (identity/telemetry/leaderboard/dashboard on this
  branch; the v3 render tiers on the other). The shipped distribution numbers
  keep their ids — they are cited from this branch's specs, plans and code, and
  went out in v2.0.0. The incoming v3 block was renumbered instead:

      D194 -> D207   D198 -> D211   D202 -> D215   D206 -> D219
      D195 -> D208   D199 -> D212   D203 -> D216
      D196 -> D209   D200 -> D213   D204 -> D217
      D197 -> D210   D201 -> D214   D205 -> D218

  69 citations updated across code, ENGINE.md, the tracker, the v3 specs and the
  bugs files. The one D195 left in code is `main.cpp`'s `// Task 7 (D195)`, which
  is this branch's own name-entry decision and correct as it stands.

  The trap that caused it: each branch's `decisions.md` writes entries in a
  DIFFERENT format (`### D195 — ...` here, `- **D195 — ...**` there), so a grep
  for one form reports the other branch's log as empty. Next free id is now
  **D220**.


## v3 — the neon polish branch (visual-overhaul)

- **D207 (was D194 on `visual-overhaul`) — Tier 0+1: vsync and render-target bloom.** The branch's goal is the
  Wii Play *Laser Hockey* read: near-black ground, few crisp lines, real halos.
  - *Vsync on* (`SDL_SetRenderVSync(1)`): tear-free presentation. The Timer's
    busy-wait stays as a pacing floor; `--seed` still forces deterministic dt,
    so the canary is untouched. **Rejected:** vsync-off + higher target FPS
    (still tears) and adaptive vsync (`-1`, not universally supported).
  - *Backdrop restraint*: far stars 55@120a (was 140@200a), mid machinery
    5@55a (was 14@130a), near grid 128px@26a (was 64px@40a). The backdrop was
    competing with the enemies for density; the foreground owns the frame now.
  - *Bloom is a render-target chain, not a shader* (`bloom_system.{hpp,cpp}` +
    pure `bloom_math.hpp`): world+UI render into a scene target, which is
    walked down 4 halving linearly-filtered targets (each halving is a free
    box blur) and composited back additively with per-level GameData weights.
    **No bright-pass**: on a near-black field the emissives are the only
    bright content — Tier 2 will move bloom onto a dedicated emissive target
    instead of thresholding. **Self-disabling**: any target-creation failure
    (dummy/offscreen drivers) turns begin()/resolve() into no-ops, so headless
    runs and the screenshot path render the exact pre-bloom pipeline.
    **Rejected:** SDL_GPU shaders now (Tier 4's job, needs a toolchain) and
    per-sprite baked halos as the only glow (they cannot bleed or saturate).

- **D208 — Tier 2: emissive separation is a naming convention, not a component.**
  Every generated atlas/prop now ships a `_glow` sibling PNG whose alpha is the
  source's alpha × per-pixel luminance^1.2 (`emissive_of` in `common.py`) — the
  emissive layer is *derived*, never re-authored, so it can never drift from the
  art and the same sidecar frame rects apply. At render time a second walk
  (`RenderSystem::render_emissive`) draws each entity's sibling — probed via the
  new `ResourceManager::try_load_texture`, which returns nullptr on a miss and
  caches it silently (the magenta missing-texture and per-frame disk probes are
  both wrong here) — plus any additive-Tint visual (particles: their sharp copy
  is already in the scene, so this contributes halo only) into the bloom
  emissive target; the blur chain reads that target alone. Result: the Tier 1
  full-scene wash is gone — the floor is near-black again while emissives bloom.
  **Rejected:** a `GlowSprite` component (invariant 6 makes a new component the
  expensive edit, and the convention needs zero data changes); a luminance
  bright-pass at composite time (Tier 4's shader job, meaningless on a chain
  seeded full-scene); kit-part siblings (they composite over a chassis whose
  glow already blooms). HUD widgets keep no siblings on purpose — menus stay
  crisp. Trap for new art: any sprite that should glow must be born through the
  generators; hand-dropped PNGs bloom only if additive-tinted.
- **D209 — Tier 3: impact feel is dt-shaping, not new systems.** Audit first:
  projectile trails (player_fire_system) and enemy-shot trails
  (enemy_fire_system) already existed, as did kill trauma + seeded shake — so
  Tier 3 shipped only the two genuine gaps.
  - *Hit-stop*: a `hitstop_left` frame counter in main; kills saturate it to
    `feedback.hitstop_frames_kill` (2), a boss death to `hitstop_frames_boss`
    (6) via a `boss.just_died` Blackboard edge. Applied after
    `timer.update_blackboard`: the published `delta_time` is overridden to 0,
    so every system still RUNS (draw/RNG counts unchanged — the determinism
    invariant is untouched) but integrates zero motion; `end_frame` still
    advances, so `--stopframe` cannot hang. Saturating, not additive: a
    multi-kill frame is one beat, not a slideshow. **Rejected:** freezing the
    frame counter (breaks scripted `--keys` indexing, the pause trap) and a
    wall-clock freeze (non-deterministic).
  - *Zoom punch*: the camera block writes `camera.zoom = 1 + zoom_punch *
    trauma²` each simulated frame (same curve as shake, same
    `settings.screen_shake` gate). Legal because CameraControlSystem is not
    instantiated in this game — nothing else writes the key, so no restore
    step. NOTE: the standard canary cannot fire hit-stop at all (a
    mouseless dummy-driver run lands no kills), so it stayed byte-identical to
    the pre-Tier-3 baseline too; a kill-bearing scripted run WOULD legitimately
    shift sim timing vs older builds — that is the feature.
  - *Squash on hit*: **skipped.** `Size` is collision-coupled, so a geometric
    squash needs either a new component (invariant-6 expensive) or draw-path
    plumbing — for feedback the Flash + hit-stop + punch stack already covers.
    Revisit only if a windowed playtest asks for it.
- **D210 — Tier 4: SPIR-V post-processing rides SDL_Renderer, behind an opt-in.**
  `PostFxSystem` attaches one precompiled fragment shader
  (`assets/shaders/postfx.frag.spv`, built OFFLINE by `assets/shaders/make.sh`
  with glslc or standalone glslang — a build never compiles shaders, same
  discipline as the PNG generators) to a full-screen draw via
  `SDL_CreateGPURenderState`. One pass: chromatic aberration, vignette,
  saturation/gain grade, and a radial shockwave triggered by boss deaths and
  arena shifts (`trigger_shock`). The whole composite routes into the postfx
  frame target (bloom's `resolve()` restores to the target captured at
  `begin()` instead of hard-coding the backbuffer — the one bloom change this
  tier needed), then `apply()` draws it back through the shader.
  - **Opt-in, not default** (`--gpu-renderer`): the installed SDL prerelease
    wedges — see `bugs/009`. The classic renderer carries the full Tier 0-3
    look; Tier 4 adds grade/aberration/shockwave only. Flip the default after
    a system SDL update passes the bug-003 re-test.
  - **Uniforms are 8 tightly-packed floats** matching the GLSL block; the
    binding model is SDL's render-state contract (texture set 2/0, uniforms
    set 3/0, vertex color loc 0 + uv loc 1).
  - **Rejected:** SDL_shadercross (HLSL source — GLSL + glslang is one less
    toolchain); per-arena LUT textures (a second sampler binding + a LUT
    generator for what saturation/gain uniforms deliver today — revisit if an
    arena needs a real look, not a grade); rewriting rendering on raw SDL_GPU
    (the render-state API exists precisely so SDL_Renderer code keeps working).
- **D211 — Tier 5: neon lines are immediate-mode geometry, not entities.**
  `line_mesh_math.hpp` (pure, tested: miter joins with width preservation +
  hairpin clamp, strip triangulation, arc-length UVs, circle sampling) feeds
  `RenderSystem::render_glow_lines`: world-space polylines → miter-joined
  triangle-strip ribbons via `SDL_RenderGeometry`, cross-section sampling the
  new 1D `line_falloff.png` additively, with an optional 0.35x-width
  white-lifted core strip — resolution-independent at any zoom, no sprite
  minification ever. The camera transform + world Y-flip are applied inside
  `render_system.cpp`, keeping the one-flip-per-space invariant literally
  one-file true. main.cpp rebuilds the line list every frame from live state
  (nothing to invalidate on an arena shift) and draws it twice: into the scene
  and into the bloom emissive target, so every line halos.
  Consumers: the arena boundary ring (the clamp circle finally has a visible
  rink line), obstacle outlines in the live arena's enemy tint, and a hot
  ribbon over each recycled laser-beam quad.
  **Rejected:** a `LineGlow` component (invariant 6 — and an immediate-mode
  list has no destruction/recycling story to get wrong); replacing the beam
  quads (the ribbon rides on top; removing the quad would touch the damage
  path for a visual); enemy-shot tracers (their particle trails already read
  well — add if a playtest disagrees).

- **D210 addendum (post-SDL-update re-test, same day).** With the system SDL
  rebuilt from origin/main: the mid-run and readback crashes are gone, so
  `--gpu-renderer --screenshot` is now allowed (default runs still capture on
  classic — it stays the verification baseline). Teardown localized one call
  further: `SDL_DestroyGPURenderState` + texture destroy are clean;
  `SDL_ReleaseGPUShader` still wedges under a live renderer, so exactly one
  shader object (~4KB) is leaked to process exit. GPU stays opt-in pending a
  windowed playtest — a product call now, not a stability one. bugs/009
  updated with the full re-test matrix.

### Hoisted from `v3-neon-projectiles-and-display.md` (2026-08-15 merge)

Hoist to `decisions.md` / `progress-tracker.md` on `master` at merge, then empty.

- **D212:** GPU renderer is now the default; `--classic-renderer` is the escape
  hatch. The plan always specified this — Tier 4 shipped it inverted as a
  bugs/009 stability hold, discharged by the SDL update + a windowed playtest.
- **D213:** trail history lives in a render-side `unordered_map` in `main.cpp`,
  NOT an ECS component. Keeping it out of `component_storage` means no gameplay
  system *can* read it, so presentation-only is enforced by construction.
- **D214:** projectiles carry no `Color` component — the neon ribbon is their
  only visual. Colour rides on `ProjectileTag` / `EnemyShot` instead. A new
  `HiddenVisual` engine component was built for this first and discarded: a bare
  tag costs ~30 lines of ComponentStorage instantiation boilerplate, and
  dropping `Color` achieves the same thing with none.
- **Deferred:** Tier 6c (per-arena LUT grade + dash radial blur). 6a + 6b were
  judged sufficient. Still live if the grade reads flat.
- **Rejected:** textured particles as the box-halo fix — measured ~27x slower
  (bugs/004). The mitigation shipped instead is a bloom pullback.
- **Rejected:** `Timer::set_external_pacing` for the menu framerate — the vsync
  double-pacing hypothesis was disproved by A/B (bugs/005). Reverted unshipped.

### Hoisted from `v3-soft-particles-and-explosions.md` (2026-08-15 merge)

- **D215:** additive particles are batched into one `SDL_RenderGeometry` mesh
  UV'd to `glow_disc_64.png`, NOT given a per-entity texture. The texture route
  was measured at ~27x (bugs/004) because `draw_entity` makes six batch-flushing
  SDL state calls per particle; a batch makes six per FRAME.
- **D216:** the explosion shockwave ring and debris shards are `GlowLine`s, not
  new sprites or a new renderer — the Tier 5 line renderer already draws exactly
  this shape.
- **D217:** the tracer is tuning, not new machinery — `taper_widths` gained an
  `exponent` (shots use 2.0) and `GlowLine` a `core_scale` (shots 0.26), plus
  GameData trail length 14x3.0 -> 20x3.6 (~9.8x the 7.0 shot width) with the
  vertex budget raised 4000 -> 6000 to pay for it.
- **D219:** `EnemyDeathSystem` loads `images/v2/effect_explosion.json`. It had
  loaded the class-original placeholder since before v2; see ENGINE.md section 5.
  The blast layers also set `core = false` — a white-lifted core blooms into a
  flat grey ball that fills the ring.
- **D218:** `UIRenderSystem::render` sets `SDL_BLENDMODE_BLEND` itself instead of
  inheriting it. Found by Tier 9: the UI's alpha fills had been relying on
  additive particles setting the renderer-wide draw blend mode each frame, so
  removing particles from the render walk made every alpha-0 fill paint solid
  black. Fixed at the UI's single entry point rather than per fill site — one
  guard where all of them route.
- **NUMBERING HAZARD:** this branch's `decisions.md` stops at D209 and this
  branch's other spec holds D212-D214 unhoisted; `experimental` and
  `distribution` are reported to also claim numbers in this range. Reconcile at
  merge — do not assume D215-D217 are free on `master`.


## D220 — Merge the engine suite, leave the roguelite gameplay on `experimental`  *(2026-08-16)*

**Decision.** `feature/distribution` takes `engine-suite-build` (D138-D151) as a
merge and `545e12f` (The Shroud + The Drift) as a cherry-pick. It takes **none**
of `experimental`'s roguelite gameplay: upgrade tracks, rolled shop cards with
reroll/lock, ability slots on keys 1-4, fusion capstones, the three rule-warp
hulls, ship installs.

**Why.** `gameplay.md` (the owner's spec, `~/Downloads/# Gameplay.md`) supersedes
all of it, and in one place directly contradicts it: the spec says *keep the
original in-game upgrade method and delete the gear/levels tabs*, while
`experimental` demotes `ShipState.upg_counts` to a legacy save field and moves
purchase truth to `track_levels`. Merging it would mean merging code in order to
revert it.

**Cost, measured before deciding** (`git merge-tree`, not guessed):

| Merge | Files | Hunks | Conflicted lines |
| --- | --- | --- | --- |
| all of `experimental` | 13 | 17 | ~430 code + ~1100 doc |
| `engine-suite-build` only | 5 | 7 | 225 |

**The trap that actually drove the call — and the thing to re-read before ANY
future `experimental` merge.** The conflict count is misleading. `shop_system.*`,
`player_components.hpp` and `run_save.cpp` have **zero churn on this branch since
master**, so a merge takes `experimental`'s roguelite versions **cleanly, with no
conflict marker to warn anyone**. The dangerous files are the quiet ones.

**Rejected:** merging `experimental` and reverting the gameplay afterward — the
revert surface is four systems and a save format, and a partially-reverted track
system is worse than either whole one.

**Kept from `experimental` anyway:** the A* blocked-cell fix (`3dc24d5`), which
rode in with the suite.

Suite features all ship inert (`enabled: false`; `forces` inert by shape).
`--suite` flips them on. `gate.sh .canary-baseline.txt` re-proves inertness.


## D221 — Gameplay pack v2.3: owner-interview decision batch  *(2026-08-15)*

**Decision.** The v2.3 gameplay pack (spec:
`agentProjectDocs/specs/gameplay-pack-v2.3.md`, plan:
`plans/gameplay-pack-plan.md`) is built to these owner calls, interviewed
2026-08-15:

1. Drones are **bought with scrap** — the lifetime-score ship unlock retires.
   Buying a drone grants its weapon and its colors account-wide.
2. The **4th drone is the current Purple Gatling** and owns Hailstorm;
   Hailstorm is implemented now but locked until that drone's later release.
   Standard maps to Falcon.
3. **Secondary fire is right mouse** (E/Q/SPACE unchanged).
4. The cosmetic shop sells **extra colors beyond ship-granted** for scrap.
5. Scrap economy ships as a first-pass **tuning table** in GameData.json
   (5/wave, +25/boss, +100 victory; Owl 400, Gryphon 800; colors 100-250).
6. Arena order: **seeded full shuffle**, two rules only — Prism/Prism II
   never first, Singularity pinned to wave 30.
7. Boss: **2-phase enrage at 50% HP** (denser patterns + hunt/reposition
   movement) on all three boss waves, tuned per ordinal.
8. "The 5 bubbles" = the shop stat-sheet pip meters, reused in the hangar
   with alignment fixed.
9. **One release: v2.3.0** — no staged cut.

**Why.** Each was ambiguous in the owner's gameplay.md; options were put to
the owner directly rather than guessed.

**Rejected:** score-milestone ship unlocks (2 also rejects retiring the
gatling or shipping it as a 4th now); staged 2.3/2.4 releases; orchestrator
/subagent execution (phased plan + handoffs chosen — this repo's
verification traps punish unbriefed subagents).

**Note.** The replay canary baseline changes **once**, at the arena-shuffle
tier, by design. Any other tier moving the canary line is a bug.


## D222 — Loadout persistence and weapon identity (gameplay pack tier 1)  *(2026-08-15)*

**Decision.** Three implementation calls under the D221 batch:

1. `MetaSave` gains `scrap`, `owned_ships`, `equipped_ship`, `equipped_weapon`.
   Purchases and the equipped loadout are stored because they record CHOICES
   (the prestige rule); which weapons/colors the player has stays **derived**
   from ship ownership (D81). The equipped loadout DOES reach the sim —
   supersedes D80's "the chosen ship is deliberately not persisted": replays
   are reproducible at a fixed loadout, and canary runs reset `saves/` so they
   always fly the defaults (CLAUDE.md canary rules unchanged).
2. Weapons are first-class `WeaponDef`s with their own projectile colour —
   supersedes **D184** (ship-complement shot colour). The complement rule
   survives only as the fallback when no weapons catalogue is authored.
   Per-weapon battery (`fire_time`/`recharge_time`) rides the same struct.
3. `RunSave` records the run's weapon by name; a resumed run keeps ITS weapon
   even if the hangar equips another meanwhile.

**Why.** The spec makes any owned drone able to install any owned weapon, so
weapon identity cannot live inside ShipDef; the rest follows D81's
store-choices-derive-consequences discipline.

**Rejected:** storing an owned-weapons list (derivable until standalone weapon
purchases exist); persisting per-run scrap increments anywhere but the
exactly-once `bank_run_score` edge (D3).


## D223 — The one deliberate canary re-baseline (gameplay pack tier 4)  *(2026-08-16)*

**Decision.** The replay canary's expected line changed from
`Frames: 3000  Final score: 100  Units: 24  Wave: 1  Phase: 1` to
`Frames: 3000  Final score: 170  Units: 20  Wave: 2  Phase: 1`, exactly once,
in the tier that landed the seeded arena shuffle (D221 call #6). `.canary-
baseline.txt` and CLAUDE.md were rewritten in the same commit; `gate.sh`
re-proves the new line x2. Any OTHER tier of the pack moving this line is a
regression, not a tune.

**Why.** The shuffle changes what seed 42 plays (the run now opens on Core II),
and the same tier's early-drop floor, player-enemy solidity and sustain retune
all sit inside the sim. One tier, one re-baseline, instead of four tiers each
quietly moving the line.

**Determinism preserved:** the shuffle draws from `std::mt19937(seed *
2654435761 + 97)` — a distinct stream constant, the surges pattern — so the
canary is byte-identical across runs and resumes reproduce their run's order.
The rules (no Prism opener, Singularity pinned at wave 30, ladder fixed) are
property-tested across 200 seeds in `test_arena_shuffle.cpp`.

**Rejected:** shuffling in `load_arena_config` (would shuffle the menu/test
config too); re-baselining per tier (hides real regressions between tiers).


## D224 — Sorted entity iteration; the second and final canary re-baseline  *(2026-08-16)*

**Decision.** `ComponentStorage::entities_with_component<T>()` now returns ids
sorted ascending. The canary line moves once more, 170 -> 160 (hash order ->
sorted order), and `.canary-baseline.txt`/CLAUDE.md are rewritten in the same
commit. After this, the canary is invariant to UI authoring.

**Why.** Tier 7's two new screens (~35 boot widgets) moved seed 42's score from
170 to 160 with ZERO sim-code changes. Bisected to the data (tier-7 binary +
tier-6 data = 170; +screens only = 160): boot entity count crossed an
unordered_map rehash threshold, changing iteration order for the sim loops
that draw loot RNG per kill. That made the replay canary sensitive to menu
authoring — a false-alarm generator pointed at whoever edits a screen next.

**Verified:** with the sort, the canary reads 160 with BOTH tier-6 and tier-7
GameData (UI count no longer matters); 409 unit cases + 8/8 ctest green;
canary x2 byte-identical.

**Rejected:** re-baselining without the fix (leaves the trap armed);
per-call-site sorting (misses the next site); ordered std::map storage (bigger
constant on every lookup, not just enumeration).


## D225 — GEAR and LEVELS retired from the shop UI  *(2026-08-16)*

**Decision.** The in-run shop keeps only UPGRADES (the owner's spec, with the
shopMenuItems screenshot as the reference). The GEAR/LEVELS tab widgets are
gone from GameData.json; TAB and `on_shop_page_1/2` clamp to page 0. The
purchase code (`buy_gear`, `upgrade_gear`, items/consumables catalogue,
`gear_levels`/`item_id` save fields) stays compiled and save-compatible —
nothing routes there.

**Why.** The pack's hangar/weapon/cosmetic economy replaces the gear economy;
running both would split the currency's meaning. Removing the CODE as well was
rejected: run-save compatibility keeps the fields anyway, D220 already burned
one merge avoiding a half-reverted economy, and deleting is cheap later.

**Also in this batch (spec's Random Thoughts):** ESC in the shop closes the
shop without raising pause; the drone trail originates at the hull's rear
(tracer read); a LEADERBOARD button on the main menu (the L-key flow,
net-gated); leaderboard-wipe + local-save disclaimers on the field-manual
footer (docs/features.html). Feedback reachability was checked: FEEDBACK is on
both the pause and main menus — the spec note predates Task 7's fix.


## D226 — Escapes in dashboard.js are double-escaped, and the client script is parsed by the gate  *(2026-08-16)*

**Decision.** `backend/src/dashboard.js` is one template literal, so any escape
intended for the browser is written double (`\\n`, not `\n`) — comments in that
file included. `scripts/verify_branch.sh` section 4 now extracts the served
`<script>` and runs `node --check` on it.

**Why.** A single-escaped newline shipped a real line break inside a quoted JS
string, which is a SyntaxError; the browser dropped the whole script and the
dashboard sat on "connecting..." with no data and no error for a day. Every
existing check stayed green because none of them parsed the client script —
`test.sh` only asserts `/dashboard` returns 200 and contains its title.

**Verified:** the new gate was confirmed to FAIL on the broken file and PASS on
the fixed one, not just to pass today (the bugs/003 rule about checks that
cannot fail).

**Rejected:** moving the client script to its own asset file (correct long-term,
and it would make escapes ordinary, but it changes the Worker's bundling and
deploy shape — not worth coupling to a one-line outage fix). Logged in
bugs/011 along with the still-open `cache-control: public` on an authenticated
route.


## D227 — Playtest #1 batch (9 items from the owner's first windowed run)  *(2026-08-16)*

**Decision.** One batch, all nine items from the first real playtest of the
v2.3 pack:

1. **Main menu box** enlarged (y 64->40) so the LEADERBOARD row and the three
   hint lines sit inside it; hint wording evened out because labels scale text
   to fit, so the longest line rendered visibly smaller than its neighbours.
3. **Hangar preview**: the live world drone is parked in an authored preview
   slot while `run_setup` is up. Tier 6 claimed the world drone WAS the
   preview, but it sits at world centre — behind the hangar panel.
4/5. **Cosmetic shop and inventory now CLEAR_TO instead of PUSH.** A pushed
   screen draws over the hangar without hiding it, so both menus rendered on
   top of each other. CLEAR_TO is what PLAY/BACK already use.
6. **Contact damage restored** — see the regression note below.
7. **Charge is visible**: holding right-mouse fills the secondary gauge, and a
   full charge is now a 76px slug (half-size 10->38) that pierces past half
   charge, instead of a marginally fatter dot.
8. **Pause sheet**: pips moved into their own fixed-x column (alignment by
   geometry, the hangar recipe — space padding cannot align a proportional
   font); FIRE RATE and DAMAGE split so each carries its own meter; the GEAR
   row removed, since D225 retired the economy it described. MAX_LINES 17->16.
9. **Fixed-length blaster bolts** for 55 Iron and Hailstorm — *supersedes
   D201/D213 for those two weapons only*. gameplay.md's PAB already said "no
   tracers" for them while asking for a molten-slag tracer on Flak, so Flak and
   Moonshot keep the v3 ribbon. Owner confirmed the per-weapon split.
10. **55 Iron recoloured** red -> warm white (255,236,200): enemy fire is always
   deep red, so the default weapon was reading as incoming fire.

**The regression, and what the fix taught us (item 6).** Tier 4's solidity
separated the drone from enemies at line ~3016, but CollisionSystem runs at
~3091 — so the pair never overlapped when collision looked, `CollidedWith`
never fired, and bumping an enemy did NO damage for the whole pack. Fixed by
moving the separation AFTER collision+damage: move -> collide -> damage -> part.

Then the canary died instantly, which exposed a second bug in the same code:
the separation added a **2px bonus shove on every contact frame**, which is a
repulsion field, not solidity — it walked a standing drone through the swarm.
Measured on the scripted canary: 115 score without solidity, **0** with the
bonus shove, 10 with exact-overlap separation. Now it separates by exactly the
overlap, and the spec's "small bounce" fires only when a DASH ends on top of an
enemy, which is what the spec actually asked for.

**Canary re-baselined a third time** to
`Frames: 3000  Final score: 10  Units: 0  Wave: 1  Phase: 2`. Phase 2 means the
scripted drone now dies in wave 1: it never moves, and standing inside enemies
is no longer free. Determinism still holds (byte-identical x2).

**Open, deliberately not decided here:** the canary now ends in a death run, so
it exercises little past wave 1. Scripting movement keys into it would restore
coverage but changes a documented project convention — owner's call.

**Rejected:** padding the pause sheet's text to align pips (a proportional font
makes that approximate at best); making all four weapons bolts (drops the slag
tracer gameplay.md asks for); keeping the 2px shove as a "feel" knob (it was
the bug).

## D228 — Playtest #2 batch (9 items from the owner's second windowed run)  *(2026-08-16)*

**Decision.** One batch, all nine items from the second playtest of the v2.3
pack (list pasted 2026-08-16; bug 013 filed from item 4):

1. **The scrolling-menu bug (items 1+2) — supersedes D227's park.** The
   "parked" preview drone chased the camera which re-centred on the drone the
   same frame: a feedback loop that scrolled the backdrop forever and pinned
   the ship to screen centre. `park_drone_in_hangar` is DELETED; the CAMERA
   now takes a render-only offset while `run_setup`, `main_menu`,
   `cosmetic_shop` or `inventory` tops the stack, so the live drone draws in
   an authored left-column slot and the world stands still. No sim writes —
   the canary cannot see it.
2. **"(you have N)" removed** from the next-purchasable-ship line — the scrap
   counter already says so (item 3).
3. **Grid inventory** (items 4+5, spec `specs/grid-inventory.md`): the screen
   is a grid of selectable cells — 4 weapon cells + three colour rows of
   (DEFAULT + 6 paints). shop_tab+disabled = equipped (the D88 tab
   convention), card+disabled = unowned. Clicks equip directly and persist;
   `cycle_color_slot` deleted. Bug 013 (BACK under the projectile row) died
   with the old layout, and `test_screen_layout.cpp` now fails the build on
   ANY partial widget overlap on ANY screen — it immediately caught two more
   (run_setup's HANGAR title under the scrap label, the shop tip-name under
   the stat panel), both fixed by re-authoring rects.
4. **Flak slag projectile** (item 6): projectile_size 10->16 and a new `slag`
   flag through WeaponConfig/WeaponStats/ProjectileTag — renders as a short,
   FAT hot-cored chunk (the bolt path at ~1.3x its own radius wide) with the
   long-dormant ember trail emitter finally attached (110/s, chunky). Flak no
   longer shares the ribbon.
5. **Lava-stream flamethrower read** (item 7): globs fattened 7->11px and a
   0.2s additive flame cone (260/s, 14° half-angle, gold->ember fade) spawns
   at the muzzle each stream step. Damage cadence untouched.
6. **Per-ship chassis art** (item 8): `owl_frames` (round two-pod glider,
   wing struts, twin eyes, tail feathers) and `gryphon_frames` (four chunky
   pods, armoured hex hull with plate seams, bright ram prow) in
   make_sprites.py. Paints are now chassis x colour by NAMING CONVENTION —
   `paint_sidecar()` derives `<chassis>_<colour>.json` from the ship's own
   sidecar — and the generator emits the full 3x6 matrix, so
   `CosmeticColorDef::sidecar` (which could only repaint the Falcon) is
   deleted from struct, parser and GameData. test_cosmetics now proves every
   unlocked-chassis x paint atlas exists. The locked Gatling still flies the
   violet Falcon chassis (art TODO stands, spec).
7. **Boot reskin**: the title/hangar preview wore the Falcon atlas regardless
   of the save; `reskin_player()` now runs once at startup.
8. **gameplay.md audit (item 9)**: every requirement verified implemented in
   code — ship-unlock grants its weapon (derived), sustain pickups recharge
   on a timer, waves gate on zero enemies + quota with the anti-softlock
   force-kill, boss holds until reward taken, feedback on the pause row,
   disclaimers/trail/ESC-shop/solidity/leaderboard shipped in tiers 4-8.

**Verified:** zero-warning build; 8/8 suites, 410 unit cases; gate.sh green
TWICE on the UNMOVED D227 baseline (`Frames: 3000 Final score: 10 Units: 0
Wave: 1 Phase: 2`, saves reset per bugs/006); screenshots of main menu,
hangar, inventory, slag fire and the flame cone eyeballed. NOT yet judged by
a human — playtest #3 owns that.

**Rejected:** parking the drone by writing Position with the camera pinned
(still a sim write; the offset does it render-only); one sidecar per paint
colour (cannot express paint x chassis); a WoW-style drag inventory (UIElement
has no texture/drag — selectable cells deliver the "everything is an item"
ask without a new UI engine).

## D229 — Playtest #3 batch (7 items from the owner's third windowed run)  *(2026-08-16)*

**Decision.** One batch, all seven items (list pasted 2026-08-16; bug 014
filed from item 7; owner confirmed replacing the veil on item 4):

1. **Moonshot crescents are crescents now** (item 1): a `crescent` flag
   through WeaponConfig/WeaponStats/ProjectileTag renders the shot as an arc
   bowed along the heading — 7 points, per-point widths thin at the tips and
   fat in the middle — for the primary AND the RMB radial burst. Replaces the
   ribbon for this weapon only.
2. **Flak hits harder and explodes** (item 2): damage 45→55, and a slag
   impact now deals a light AoE (40% of shot damage, 80 px radius, ponytail
   feel-number) with an orange burst so the explosion reads. Hue fixed by NOT
   applying the shared +90/+35/+60 brighten to slag — the wash was turning
   Flak's authored orange white; the hot core supplies the brightness.
3. **The flame IS the flamethrower** (item 3, supersedes the D228 globs): the
   lava stream spawns no projectiles at all — each tick hit-tests a forward
   cone (270 px, ~16°, 4 dmg/tick, knobs in secondary_fire.hpp) and applies
   damage + Burn; the fire particles (faster, longer-lived, ~250 px reach) are
   the entire visual. Nothing left to see "underneath".
4. **Owl special = second dash charge** (item 4, REPLACES the phoenix veil —
   owner's call, supersedes that part of D221/D223): start_run bumps dash_max
   by one for `special: "dash_charge"`. The veil machinery is deleted
   (ship_specials.hpp trigger/re-arm, the solidity pass-through, the armed
   flag); the generic "ship.no_fire" jam countdown stays for future sources.
5. **Ember de-squaring + bigger chunk** (item 5): the size-10 square trail
   particles read as A BOX under the chunk — now 5 px and denser (the sparkle
   without the square); projectile_size 16→18.
6. **Charged slug renders as a chunk** (item 6): spawn_shot grew
   crescent/chunk flags; the 55 Iron charge shot uses the slag render path,
   so the slug is a fat hot-cored chunk scaled by the charge — the beefed-up
   Flak primary the owner described.
7. **Feedback over pause** (item 7, bug 014): the feedback screen was
   authored z 9/10 vs pause's 30/40, and UIRenderSystem sorts by z across all
   active screens, so pause always drew on top. Feedback is now 50/60 —
   strictly above anything it can be pushed over.

**Verified:** zero-warning build; 8/8 suites (one timing-suite flake on the
first pass, clean twice after — the bugs/010 pattern); gate.sh green TWICE on
the UNMOVED baseline, saves reset per bugs/006 and the owner's save restored;
screenshots eyeballed for the crescent, the orange chunk + sparkle trail and
the long flame cone. NOT verifiable headless: the feedback z-fix
(`on_feedback_click` is gated on net::enabled(), off headless — proven by the
sort arithmetic instead) and the Owl's second dash (state dump doesn't print
ShipState; the data test pins the special id). Owner judges both in playtest #4.

**Rejected:** keeping the veil alongside the dash charge (owner said
replace); crescent as a sprite atlas (UIElement/projectiles carry no texture
— the glow-line arc does it in ~20 lines); keeping glob projectiles under the
flame with colliders off (two damage paths to tune instead of one).

## D230 — Playtest #4 batch (6 items from the owner's fourth windowed run)  *(2026-08-16)*

**Decision.** One batch, all six items (list pasted 2026-08-16):

1. **Charge-up reads as charging** (item 1): a world-space charge bar under
   the drone (glow-line track + gold fill, render-only) plus gather motes
   ringing the hull while the RMB is held — the freed player emitter slot
   (see item 4), attached on charge start and removed on release.
2. **The slug was never "not hitting"** (item 2): a probed headless A/B run
   (no-fire 0 score vs slug-only 10) showed it connects — it was illegible
   and died on the first graze. Now 16+34*frac half-size (wider floor) and
   ALWAYS piercing, so a release sweeps through the pack and visibly lands.
3. **Feedback form UX** (item 3): panel centred (it spanned 180-800, flush to
   the right edge); every field is a clickable card box — a button hitbox
   CONTAINING its text label, which the layout gate permits by design; the
   body has its own tall box with click-anywhere focus; SUBMIT button
   replaces ENTER-to-send (ENTER in BODY still inserts a newline); the
   focused card wears the D88 selected look.
4. **One trail, and it dissipates** (item 4): the centre thruster emitter is
   DELETED — it read as a second trail leaking from the hull (and its
   direction math mixed radians with degrees anyway). The rear ribbon now
   sheds one tail point per frame whenever the drone adds no new sample, so
   a stopped drone's trail melts instead of hanging.
5. **Flamethrower on fuel** (item 5, supersedes D229's 3s-burst-per-cooldown):
   hold to breathe while the tank (STREAM_S=3s, refill 6s, knobs in
   secondary_fire.hpp) drains; release refills; the shared HUD gauge shows
   fuel. Damage was PROVEN landing (A/B score 10 vs 0) but invisible — every
   cone tick now flashes its victims.
6. **Moonshot tune** (item 6): damage 14 → 17.5 (+25%), primary crescent
   16 → 22 wide, burst crescents 1.6x primary, secondary_cd 10 → 6.

**Canary re-baselined a FOURTH time** to
`Frames: 3000  Final score: 0  Units: 0  Wave: 1  Phase: 2` — and the move
was PROBED, not assumed: re-adding the thruster emitter restored the old 10
exactly. The emitter's per-frame particle entities perturb entity-id
allocation, so an always-on emitter is a sim change in this engine, not
presentation (bugs/003 Trap 8 note). While confirming, found that **gate.sh
exited 0 on every failure it printed** — DIFFERS/DIVERGED/warnings/red
ctest were all advisory. Fixed: FAIL propagates; verified both exit paths.

**Verified:** zero-warning build; 8/8 suites; canary byte-identical x2 at
the new baseline (gate green, and the gate's red path now really exits 1);
screenshots eyeballed: charge bar + motes, flame plume with no globs under
it, dissipated trail on a stationary drone. NOT verifiable headless: the
feedback form (net-gated) — owner exercises it in playtest #5.

**Rejected:** keeping a cooldown alongside fuel (two clocks, one gauge);
inward-flying gather particles (emitters only push outward — short-lived
growing motes read the same for zero engine work); making fields
multi-widget focus machinery in the engine (the game's by-name rewrite
pattern already does it).

## D231 — Playtest #5 batch (10 items from the owner's fifth windowed run)  *(2026-08-16)*

**Decision.** One batch: 8 items built, 1 recorded, plus bugs 015/016.

1. **Bug 015 — same-family REACTOR SHIFT**: the ladder is 3-wave blocks (the
   6→7 boundary was real), but the shuffle could seat Prism II next to Prism,
   so the shift banner announced an arena the screen already showed. Shuffle
   rule 3: no same-family neighbours (bounded greedy repair; the first draft
   broke the no-Prism-opener rule and the 200-seed test caught it — that test
   now pins all three rules).
2. **Bug 016 — cover now covers**: the tier-3 laser's pierce exemption
   skipped destruction on EVERYTHING, walls included, while player pierce
   shots stop on walls. Enemy shots now die on any OBSTACLE-layer collider
   regardless of pierce; lasers still pierce bodies.
3. **Drift drama** (item 3): authored current +35% (51.3,-18.9) and the
   current REVERSES after two waves in the block. The +35% broke the anti-pin
   invariant (current 54.7 > slowest enemy 48 would wall-pin it and stall the
   wave gate), so the shove is now split: player and loot feel the full
   current, enemies ride one clamped below the slowest speed
   (drift_enemy_scale, pure + tested).
4. **Map modifiers recorded, not built** (item 4): specs/map-modifiers.md
   (Draft) + the owner's open questions. Interview before building.
5. **Low-hull vignette** (item 5): <=10% hull draws a soft flashing red
   border — nested translucent frames fading inward, sine pulse, render-only,
   gameplay phase only. NOT visually verified headless (needs a dying drone
   under a real window) — playtest #6 judges it.
6. **Drone descriptions** (item 6): ShipDef::desc from GameData, shown in the
   hangar under the stat pips. The old hangar_hint slot IS the desc slot now;
   the "grants <weapon>" note moved onto the BUY row.
7. **Flak reach + heat** (item 7): STREAM_RANGE 270→340 (flame particles
   sped up to match), projectile 55→63, breath tick 4.0→4.6 (+15%).
8. **Charge bank** (item 8, supersedes the D229/D230 scaled cooldown): the
   55 Iron secondary runs on a passively-refilling bank (full 2.5s, refill
   8s) — holding pumps the bank into the shot, release fires it, the unspent
   remainder stays banked, and the HUD gauge shows the bank when idle. The
   charge_cooldown helper and its test are deleted.
9. **Juicy slug** (item 9): the charge slug carries the ember tracer the Flak
   chunk wears, and flies 480→720 px/s (+50%).
10. **How-to-play refreshed** (item 10): the in-game screen and
   docs/features.html now say RMB secondaries, dash charges, battery, scrap
   persistence, 3-wave arena rotation, cover blocking both directions; the
   site's dead "upgrades and gear" tab line is gone. The site change is ON
   DISK ONLY — deploying ships whatever is on disk, and this branch has
   uncommitted work, so no deploy was run.

**Verified:** zero-warning build; 407 unit cases, 8/8 suites; gate green
TWICE with the canary UNMOVED on the D230 baseline (and the gate's exit code
is honest now — D230). Screenshots: hangar description renders under the
pips. NOT verified: the vignette, the drift flip feel, and cover-vs-lasers —
playtest #6 owns those.

**Rejected:** capping the drift at +14% to satisfy the invariant (the owner
asked for drama — splitting player/enemy shove keeps both); building map
modifiers now (owner said "remember", not "build"); a screen-space shader
vignette (nested SDL rects read the same at 3am).

## D232 — Playtest #6 batch (16 items from the owner's sixth windowed run)  *(2026-08-17)*

**Decision.** One batch, all sixteen (item 12's cut-off sentence resolved to
"both secondaries + make the Flak chunk a sphere"; item 9's interview chose
options A+C):

1. **Checkpoints restore** (item 1): a boss kill and the 5th-wave shop
   cadence both refill hull AND shield.
2. **Health orbs** (item 2): every kill rolls one — 5% before wave 12, 3%
   after, 25 hull — drawn unconditionally per the R2 RNG discipline.
3. **Flak 63→57** (item 3, −10%).
4. **Armor** (item 4): ShipDef::armor = flat fraction of ALL incoming damage
   ignored, applied before the shield soaks. Gryphon 25%, Falcon 10%,
   Owl 5%. Ninth hangar stat row (rows re-laid at 20 px pitch).
5. **field_focus style** (item 5): the focused feedback card is card-blue a
   shade lighter — the searing shop_tab cyan is gone from the form.
6. **TAGS dropdown** (item 6): six fixed options (incl. "hotdog", owner's
   list) as z-70 overlay buttons — ghost+disabled when closed. The layout
   gate learned that z>=70 marks a floating overlay.
7. **Per-type impact fizzle** (item 7): projectile_fizzle.hpp — crescents
   dissolve as a shimmer ring, slag splashes embers, bolts snap into sparks;
   fires on walls, end-of-range, and enemy-shot deaths (their own colour).
8. **Charge gather** (item 8): bigger, denser, and tinted from ship.shot_* —
   the weapon's own colour.
9. **Drone profile A+C** (item 9): the hangar header reads "<SHIP> PROFILE",
   and every inventory cosmetic row names its owner ("SHIP COLOR — GRYPHON",
   "PROJECTILE COLOR — FLAK CANNON").
10. **Twin Barrel fans the slug** (item 10): ship.extra_shots adds slugs to
    the charge release, same fan step as the primary.
11. **Slug** (item 11): 936 px/s (+30%), still full-range piercing, fizzles
    at end of range, trail 340/s x 0.6 s.
12. **Pizzazz** (item 12): denser flame + a white-hot spark layer on the
    breath; the Flak chunk is now a real molten SPHERE (generated
    slag_glob.png sprite — the glow-rect "block" is gone; the spriteless
    charge slug keeps the fat chunk).
13. **Loadout-gated boss items** (item 13, spec specs/loadout-boss-items.md):
    `requires` on ActiveItemDef + active_requirement_met (pure, tested);
    reward_choices 1→2. Plasma Wake (55 Iron/Moonshot: secondaries trail
    25%-slow, 12 dps plasma patches — stationary BlizzardTag fields with a
    new dps knob); Cryolator (Flak: ice breath, Frostbite stacks −10% each on
    a 0.5 s cadence, 4 = frozen 2 s, shells icy blue for the run); DOZR
    (Gryphon: a dash KILL multiplies remaining dash cooldown by 0.25).
    Frostbite rides the existing Chill component (field adds, no new
    registration).
14. **The ram is armoured** (item 14): mid-dash enemy contact costs a
    ram_dash ship nothing. Hazards still burn.
15. **Cone heat falloff** (item 15): 1.5x on the flame's axis to 0.7x at the
    edge.
16. **Vignette tiers** (item 16): starts at 20% hull, steps at 15/10/5% —
    alpha 32→74 and the pulse quickens per tier.

**Verified:** zero-warning build (the C++20 `requires` keyword collision
renamed to requires_loadout); full suites green after re-pinning two tests
the batch legitimately moved (actives' 30 s rule now applies only to E-fired
ids; the drop property test learned health orbs); canary UNMOVED, gate green
twice; screenshots: the molten sphere and the breath eyeballed. NOT verified
headless: every boss item (needs a boss kill under a real loadout), the
dropdown feel, armor feel, restores — playtest #7 owns those. The in-game
how-to hull line updated for orbs/restores.

**Rejected:** a real Frostbite component (five-file registration for fields
Chill already carries); per-patch damage events on overlap frames without dt
scaling (burst damage, not a field); offering gated items to everyone with a
dead pick (a reward that installs nothing is a lie).

## D233 — Playtest #7 hotfixes (2 items, mid-session report)  *(2026-08-17)*

**Decision.** Two fixes from live play:

1. **The flame rides the hull** (item 1): the breath's main emitter was a new
   STATIONARY host entity every 0.07 s, each radiating for 0.2 s from where
   the drone USED to be — that was the "origin lags behind the player". The
   main flame is now ONE emitter ON the player entity, re-pointed every frame
   (emit accumulator preserved so the density is unchanged), removed the
   frame breathing stops. Only the 0.07 s spark garnish still spawns per
   step — too short-lived to read as lag.
2. **The closed dropdown truly vanishes** (item 2): ghost-styled buttons
   still painted their box frame. Closed fb_tag_ rects are now zeroed —
   nothing drawn, nothing hit-testable; the authored rect is restored when
   the dropdown opens.

**Verified:** zero-warning build, 8/8 suites, canary unmoved (gate green),
screenshot: flame pouring from the hull mid-move. The dropdown re-check is
the owner's (windowed form).

## D234 — Playtest #8 batch (4 items, mid-session report)  *(2026-08-17)*

**Decision.**

1. **Flame deadspot** (item 1): standing ON an enemy could put its centre just
   behind the cone axis, and the angle test alone read that as "not in the
   flame" — 0 damage point-blank. Inside 70 px (+ enemy radius) the cone
   check is waived, and the heat falloff is clamped at its 0.7x floor so a
   behind-axis hit can never go negative.
2. **Dropdown legibility** (item 2): new `menu_option` style (lighter than
   card, bright text) on 34 px rows at a 40 px pitch — the 6 px gaps between
   fills against the dark panel ARE the separating grid.
3. **Reward tooltips** (item 3): ActiveItemDef::desc + a reward_desc caption
   on the boss_reward panel, rewritten every frame to the HOVERED choice
   (first choice before the mouse arrives).
4. **Item slot face** (item 4): six generated neon icons
   (hud_icon_<effect>.png); the held item's icon parks over item_slot_frame
   (the dash-button recipe) and the slot's text yields to it; the key prompt
   moved UNDER the box like SPACE under the booster — "[E] READY/cd" for
   E-fired actives, the item's short tag (WAKE/CRYO/DOZR) for the D232
   passives, a bare "E" when empty. test_pause_screen re-pinned to the
   under-box layout.

**Verified:** zero-warning build, 8/8 suites (one legitimate re-pin), canary
unmoved, gate green; icon contact sheet eyeballed. The deadspot fix, dropdown
and slot face are playtest #9's to judge windowed.

## D235 — Playtest #9 (dev-mode session, 3 items)  *(2026-08-17)*

**Decision.**

1. **The tank refills only while the trigger is released**: holding an empty
   flamethrower used to sputter-refire every few frames as fuel trickled back
   in under the held button (near-zero damage, all noise). Now an empty tank
   under a held trigger does nothing until you let go.
2. **Frostbite is visible**: stacked enemies wear a faint icy additive tint
   that deepens per stack; a FROZEN enemy gets the hard white-blue tint AND a
   tilted hexagonal ice shell drawn around it (glow-line, render-only). Tints
   are cleaned up when the chill ends.
3. **The Cryolator freezes the sphere**: a cold-palette slag_glob_ice.png,
   picked via the run-scoped weapon.glob_ice key (set on pick, reset by
   start_run with the shot colours).

**Verified:** zero-warning build, 8/8 suites, canary unmoved, gate green.
Owner judges the look in the next session.

## D236 — Cryolator freeze speed + breath range (owner tune)  *(2026-08-17)*

Frostbite stack cadence 0.5 s → 0.25 s (a held breath freezes in ~0.75 s of
exposure instead of ~1.5 s); STREAM_RANGE 340 → 425 px (+25%), flame
particles sped/lengthened to keep the visual honest about the reach.
Verified: zero-warning build, 8/8 suites, canary unmoved, gate green.

## D237 — The 1-8 shop instant-buy is retired  *(2026-08-17)*

**Decision.** Owner: "that mechanic no longer exists." It half-existed — the
docs advertised `1-8 = buy the numbered row`, and a real keyboard still
reached `buy_upgrade` instantly, bypassing D189's press-and-HOLD rule that
exists so a stray input cannot spend 200 credits. Rather than leave the docs
lying about live code, the player path is GONE: real digit keys no longer
reach the shop (the dead `digit_prev` edge array went with them), while the
SCRIPTED path (`--keys 3`) stays so headless tests and the canary can still
exercise a purchase.

References purged from every player-facing surface: README controls table
(now Right mouse / B), docs/features.html controls table + the quick keys
card (now "press and HOLD a card"), and the website's controls table. TAB's
row went too — GEAR/LEVELS retired in D225, so TAB has been a no-op since.

**Verified:** zero-warning build, 8/8 suites, canary unmoved, gate green.

**Left alone, needs an owner call:** docs/features.html still carries a whole
GEAR section (§10) plus `shop-gear.png` and the "page two is gear" tutorial
line — all describing the retired D225 economy. Deleting a documented section
and its screenshot is a content decision, not a mechanical one.

## D238 — The in-game mailing-list signup a player can find  *(2026-08-17)*

**Decision.** Owner reported the in-game signup "doesn't work". Diagnosis: the
`/subscribe` endpoint was fine (probed live, 200 + row inserted), and so was
the POST wiring — but the feature was invisible and silent. Three defects,
all fixed:

1. **Nothing was ever reported.** `pending_subscribe` was fired and
   deliberately never polled, so success and failure looked identical —
   nothing happened either way. Both entry points now poll it and report
   ("Subscribed - thanks!" / "That address looks wrong" / a failure line).
2. **Signup was hostage to the rename.** The POST only fired inside the
   register-200 branch, so a taken name (409) silently discarded a valid
   address. It now rides the player's CONFIRMATION, independent of what the
   rename returns.
3. **It was unreachable in practice.** The only surface was an optional field
   on the pilot-name screen — first launch, or N at the title, with nothing
   in the menus mentioning mail exists. New standalone `mailing_list` screen:
   a MAILING LIST button beside LEADERBOARD, M as its keyboard twin (the N/L
   shape), one field, SUBMIT, and a status line. The name_entry field stays
   for first launch — it is still the one moment the game already asks the
   player to type (the spec's §72 reasoning holds).

**Verified:** zero-warning build; 8/8 suites plus a new contract test
(test_mailing_list_screen.cpp) pinning the button, the screen's inactive
boot state, all three click seams and both rewritten labels; canary unmoved,
gate green; windowed screenshot of the live screen (headless can't reach it —
net::enabled() is false there, the same gate the feedback form sits behind).
The drone preview joins the camera-offset list so it parks beside the panel
instead of behind it.

**Left to the owner:** typing a real address and watching it land in the
`subscribers` table — the one step no automated check here can perform.
