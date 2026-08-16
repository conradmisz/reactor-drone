# Reactor Drone v2.3.0 — Gameplay Feature Pack (drones, weapons, scrap, hangar)

## Context

Conrad's `~/Downloads/# Gameplay.md` specs the next iteration of Reactor Drone on top of the
current **v2.2.0** release. Target worktree: `~/Documents/GameEngines/reactor-drone-v2-distribution`,
branch `feature/distribution` (HEAD 990d019, up to date with origin). The precondition in the doc —
"merged with visual overhaul" — is **already met**: visual-overhaul (v3 tiers 0-12) is fully merged
and the D196/D197 decision-number collision was resolved (v3 block renumbered D207-D219).
**Next free decision id: D220** (check CLAUDE.md header before starting — it may have moved).

The pack: 3 playable drones with per-ship stats+specials, 4 weapons with primary+secondary fire,
a persistent "scrap" currency, a hangar-style launch menu with cosmetic shop + inventory,
seeded-random arena order, a 2-phase boss rework, and a batch of gameplay/UX fixes.
Everything ships as **one release, v2.3.0**, when done and playtested.

## Execution model (Conrad's choice)

**Phased plan + handoff docs**, per the repo's own convention:
- Author `agentProjectDocs/specs/gameplay-pack-v2.3.md` (from `specs/feature-template.md`) and
  `plans/gameplay-pack-plan.md` in the repo as the first commit — this plan file is the seed for those.
- Numbered, independently-shippable tiers. Each tier: spec-first, decisions logged (D220+ append-only),
  standing gates re-run (zero-warning build, ctest, replay canary ×2, `gate.sh`), one commit (or few),
  and `HANDOFF.md` updated so any tier can start in a fresh session after /clear.
- Per `ai-workflow-rules.md`: every handoff records shipped / not shipped / files touched / tuning
  values / **verification actually run** ("tests passing is not a playtest").

## Decisions locked in interview (log as D-entries in tier 0)

1. Drones are **bought with scrap** (lifetime-score unlock retires). Buying a drone grants its weapon
   and its body+trail(+projectile) colors account-wide.
2. **4th drone = the current Purple Gatling**, owner of Hailstorm — deferred to a later release.
   Implement Hailstorm now, locked/unobtainable. Standard maps to Falcon.
3. **Secondary fire = right mouse** (press/hold; 55 Iron charges on hold). E/Q/SPACE unchanged.
4. Cosmetic shop sells **extra colors beyond ship-granted** (gold/white/etc.) for scrap.
5. Scrap economy: first-pass numbers proposed below, all in GameData.json as a **tuning table**.
6. Arena shuffle: **seeded full shuffle, 2 rules** — Prism/Prism II never first, Singularity fixed at wave 30.
7. Boss: **2-phase enrage at 50% HP** (denser/faster patterns + aggressive hunt/reposition movement),
   applied to all three boss waves, tuned per ordinal. Fixes stuck-behind-structures as part of movement.
8. "5 bubbles" = the 5-pip stat meters (●●●○○) from the shop stats panel, reused in the hangar,
   **alignment fixed** (every row gets pips at one x-column).
9. One release: v2.3.0.

## Key seams (from exploration — reuse, don't rebuild)

| Feature | Seam |
|---|---|
| Per-ship stats | `ShipDef` (`CPP/game/arena_config.hpp` ~L140) + `apply_ship()` (~L619) + parser (`arena_config.cpp` ~L140). Today apply_ship only overlays sprite/clip/weapon — extend with hull/shield/speed/dash/special. Applied at single site in `start_run` (`main.cpp` ~L1234). |
| Weapons | `WeaponConfig`/`WeaponStats` + `player_fire_system.cpp` (166 lines, whole gun). Primary = `mouse.held`. Add `mouse2.held` edge for secondary. Battery/multi-shot/ricochet all blackboard-driven. |
| Scrap | New int(s) on `MetaSave` (`meta_save.hpp/.cpp`, `saves/meta.json`, garbage-tolerant loader) + credit in `bank_run_score()` (`main.cpp` ~L1149). **D3: never persist in-run `ShipState.currency`.** |
| Arena order | Everything resolves through pure `active_arena_index()` (`arena_config.hpp` ~L594). Shuffle a permutation in `start_run` from the run seed. |
| Boss | `boss_system.cpp` (422 lines). One-slot boss item **already exists** (`ShipState.active_id`, keep-or-swap on kill already implemented). `wants_arena_shift()` seam exists unconsumed. |
| UI screens | JSON-authored in `assets/GameData.json → screens` + click router `main.cpp` ~L2920+; label-rewrite idiom (`refresh_*`). Leaderboard screen + fetch already exist (L key) — just needs a main-menu button. |
| Wave-complete | Already requires all enemies dead (`all_complete()` + no `EnemyTag` entities, `main.cpp` ~L2827) with a 30s stall timeout — spec item 8 is mostly done; verify spawner stop + boss-adds case. |
| Player/enemy collision | Today player passes through (contact only queues damage via `player_damage_system`). New: solid-by-default + dash exception + bounce-out — extend `push_out_of_solids` pattern (`main.cpp` ~L820). |

## Tiers

### Tier 0 — Hygiene + spec
- Commit the 5 dirty doc files sitting in the worktree (ENGINE.md, decisions.md, bugs 006/008, progress-tracker).
- Write `specs/gameplay-pack-v2.3.md` + `plans/gameplay-pack-plan.md`; log interview decisions as D220+.
- Verify baseline: `scripts/verify_branch.sh` sections 1-4 green before any code.

### Tier 1 — Data model + persistence (scrap, ships, weapons)
- Extend `ShipDef`: hull, shield, speed, dash_distance, special-attribute id, scrap price, sprite. Extend
  `apply_ship()` to overlay them. Roster in GameData.json: Falcon (blue, balanced, 55 Iron, −25% equipment
  cooldown), Owl (purple, balanced, Moonshot, <10%HP → 4s invincible/no-fire/phase-through), Gryphon
  (forest green, high hull/slow/short dash/starting shield, Flak, dash recharges shield + knockback + damage),
  Gatling/4th (locked, Hailstorm) — each with a unique sprite (via `assets/generator/v2/make_sprites.py`).
- New `WeaponDef` block: damage, fire_rate, recharge_rate (battery), range (projectile_lifetime), default
  projectile color, secondary id + cooldown. 4 weapons per the spec.
- `MetaSave`: `scrap`, `owned_ships[]`, `owned_weapons[]`, `owned_cosmetics[]`, `equipped_{ship,weapon,ship_color,trail_color,proj_color}`. Old saves load clean (loader is per-field tolerant; new fields default: Falcon owned+equipped).
- Scrap award in `bank_run_score()`: per-wave-cleared amount + boss bonus + wave-30 victory bonus.
- **First-pass economy (tuning table in GameData.json):** 5 scrap/wave, +25/boss, +100 victory bonus
  (full victory ≈ 325; typical partial run ~100-150). Owl 400, Gryphon 800. Cosmetic colors 100-250.
- Tests: extend `test_meta_save`, add weapon/ship parse tests. Beware trap 3 (GLOB, re-run cmake for RED).

### Tier 2 — Weapon primaries + ship specials
- 55 Iron (red, skinny laser, balanced), Moonshot (green wide crescent, multi-hit, low rof/range,
  low-medium damage), Flak (yellow/orange slag + tracer, slow/high damage, bigger projectiles),
  Hailstorm (ice blue, high rof/low damage). Crescent multi-hit needs a piercing projectile variant
  in `projectile_hit_system`.
- Ship specials via the existing blackboard idiom (`"ship.active_cd_mult"` already exists for Falcon's;
  Owl's invuln rides the i-frame path; Gryphon's rides `tick_dash`).
- Per-weapon recharge_rate feeds the existing battery publish (`"battery.*"`).

### Tier 3 — Secondary fire
- Right-mouse edge in `player_fire_system` + per-weapon secondary with shared 10s cd slot on `ShipState`.
- 55 Iron: hold-to-charge big projectile, damage ∝ hold, cd scales with hold (max 10s).
- Moonshot: radial crescent burst. Flak: 3s cone stream + 3s burn DoT after last exposure (new small
  status component). Hailstorm: traveling expanding blizzard ring, slows enemies (reuse slow if any, else timed speed_mult on enemy).
- HUD: secondary cooldown indicator (copy the `hud_dash_*` dial idiom).

### Tier 4 — Run structure: arena shuffle + wave/collision fixes
- Seeded arena permutation in `start_run` (from run seed RNG, NOT random_device — determinism/replays):
  Prism/Prism II never index 0, Singularity pinned to wave 30.
- **Canary re-baseline (deliberate, one decision entry):** the shuffle changes what seed 42 plays, so the
  expected canary line and `.canary-baseline.txt` change once here. Follow bugs/003: clear `saves/`,
  dump `strings` to file then `grep -qF`, rebuild before measuring.
- Player no longer passes through enemies (solid + small bounce), dash exception + bounce-away on
  dash-end-overlap; Owl's phase-through overrides. Expect feel iteration.
- Later waves: 4 health/shield pickups per map with recharge (extend `sustain_spawn_system` config per-wave).
- Bigger health pickups; slightly higher early-wave credit drop rate (`EconomyConfig`).
- Verify enemy spawn cutoff + all-dead gate handles boss-summoned adds.

### Tier 5 — Boss rework
- 2-phase enrage at 50%: cadence multiplier, denser signature patterns, extra radial layer.
- Hunt/reposition movement with structure avoidance (fixes stuck-behind-structures — that's a pathing
  target-selection fix, not a physics one).
- Boss-down flow: reward menu only after boss **and all its summoned adds** are dead.
- Boss items: trim catalog to Heat-Seeking Missiles only (remove Laser Cannon, Repulsion Device from
  `actives`; keep the `active_items` plumbing). **TODO logged in spec: more boss items later.**
- Per-ordinal tuning knobs in the `boss` JSON block.

### Tier 6 — Hangar (launch menu rework) + run stats screen
- Expand `run_setup` into the hangar: drone preview (reuse shop's live preview idiom), stats panel with
  **aligned 5-pip meters** (extract/reuse `pips()` from `shop_system.cpp` ~L328), CHANGE SHIP /
  CHANGE WEAPON selectors, difficulty tabs kept, **big green LAUNCH bottom-left**, buttons into
  cosmetic shop + inventory (tier 7 screens).
- End-of-run statistics screen (new screen, both defeat+victory, before prestige offer): waves reached,
  kills, score, **scrap earned this run** — data already in `telemetry::RunReport` + `pause_stats`.
- Purchasing drones with scrap happens here (hangar), not in the cosmetic shop.

### Tier 7 — Cosmetics: slots, shop, inventory
- Cosmetic slots: ship color, trail color (per drone), projectile color (per weapon). Auto-grant colors
  on drone purchase; colors usable cross-ship/cross-weapon. Wire into the existing color publishes
  (`ShipDef.color`, `"ship.shot_r/g/b"` D184, trail color).
- Cosmetic shop screen (from hangar): extra colors for scrap.
- Inventory screen: tabs → Weapons | Cosmetics (Cosmetics sub-split: ship stuff / projectile colors).
- Equip state persists on `MetaSave`.

### Tier 8 — Cleanup batch
- Shop: remove GEAR and LEVELS tabs (hide tabs + pages; **keep** `gear_levels`/item fields for save
  compat, keep Q-consumables? — no: Gear page sold items/consumables. Decision at tier time: either keep
  the page's items reachable elsewhere or retire items+consumables with the tabs. Default: retire the
  UI, leave components inert, log decision).
- ESC in shop closes shop without opening pause menu.
- Trail originates from drone rear + more tracer-like (offset the sample point in the render-pass
  `trail_history` sampling, `main.cpp` ~L3677; presentation-only).
- Leaderboard button/tab on main menu → existing leaderboard screen.
- Website disclaimers on the Worker dashboard/landing (`backend/src/worker.js`): periodic leaderboard
  wipes; deleting game files loses progress (persistent cloud saves only if requested).
- Verify feedback menu reachability from pause + main menu (explorer says both exist; doc claims
  inaccessible — check in a real window, fix if actually broken).

### Tier 9 — Balance, playtest, release
- Playtest passes (real window, `.agents/skills/playtest` triage skill for feedback batches): weapon
  feel, boss difficulty, scrap pacing, collision bounce feel. Tune the tables.
- Full `scripts/verify_branch.sh` (all 7 sections). GAME_VERSION + Worker RELEASE_VERSION → 2.3.0,
  tag v2.3.0 (watch CI incl. build-mac).

## Failure points / traps (read before every tier)

- `agentProjectDocs/bugs/003-verification-traps.md` — stale binary after header change, pipefail+grep -q
  inversion, GLOB without CONFIGURE_DEPENDS (new test files silently not compiled), stale `saves/`
  booting into name entry. Also bugs/006 (canary reads persistent saves — clear `saves/` first),
  008 (release/merge traps), 010 (property-test ~1/20 flake — re-run before believing red).
- Canary changes ONCE (tier 4, arena shuffle) — any other tier that shifts the canary line is a bug.
- Determinism: all new RNG (shuffle) derives from the run seed; `net::enabled()` already false under
  `--stopframe`.
- Save compat: meta.json/run save loaders are field-tolerant, but run saves snapshot gun stats —
  weapon swap must round-trip through `run_save`/`run_save_apply` (add fields + test).
- Playtest debt: distribution features have historically only been driven by `drive_ui.py`. Every tier
  with player-facing surface gets a real windowed playtest note in its handoff, not just green gates.

## Out of scope (logged as TODOs in the spec)

Prestige iteration, wave count changes, more boss items, the 4th drone release, per-ship tailored
in-run shop upgrades, persistent cloud saves, leaderboard wipe automation.

## Verification

Per tier: `python run.py` (zero warnings), `python runTestsAll.py`, replay canary ×2 against the
current baseline (`--seed 42 --keys $(seq -f '%g:SPACE' 10 4 2990) --stopframe 3000`, `saves/` cleared),
`./gate.sh` where applicable, plus the tier's own checks (e.g. tier 3: headless run holding right-mouse
via blackboard script-key; tier 6/7: `scripts/drive_ui.py` click-through + `--screenshot` frames; tier 4:
two different seeds produce different arena orders, same seed twice produces identical orders).
End-to-end at tier 9: `verify_branch.sh` all sections + real windowed playtests on Normal and Hard.
