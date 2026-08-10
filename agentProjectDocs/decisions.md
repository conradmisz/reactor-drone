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

