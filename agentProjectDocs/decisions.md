# Decisions Log

Append-only. Never rewrite or delete an entry — if a decision is reversed, add
a new one that supersedes it.

**Numbering:** decisions carry stable ids (`D1`…) because code comments,
handoffs and plans cite them. The next free id is **D50**. Continue the
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
