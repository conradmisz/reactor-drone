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
