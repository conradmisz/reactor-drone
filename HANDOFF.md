# Handoff — Reactor Drone

You are picking up a project mid-flight. **Everything through iteration 4 is merged,
verified and pushed.** The next work is iteration 5: 13 items from the user's playtest,
already split into lanes below. Your job is to dispatch them.

- Repo: **https://github.com/conradmisz/reactor-drone** (public, default branch `master`)
- Working tree clean, everything committed and pushed.
- **Next free decision id: D105.**

## Read first

1. `CLAUDE.md` — project rules. They override your defaults.
2. `ENGINE.md` — the architecture doc: layer diagram, measured provenance, frame order,
   §5 traps, §6/§6a the hook slots and game systems. **Any engine change updates it in the
   same commit.** This is a hard project rule.
3. `agentProjectDocs/progress-tracker.md`, `decisions.md` (D50-D104 are all allocated).

## Re-establish the baseline before changing anything

```bash
cmake -B CPP/build -S CPP && cmake --build CPP/build -j$(nproc)   # zero warnings
python3 runTestsAll.py                                            # 8/8
rm -f saves/run.json
SDL_VIDEODRIVER=dummy ./CPP/build/game/game --seed 42 --keys 10:SPACE --stopframe 3000
# run TWICE -- byte-identical:
# Shutting down. Frames: 3000  Final score: 0  Credits: 0  Wave: 1  Phase: 2
```

Lua's vendored `tmpnam` link warning is the only allowed warning.

---

## What exists now (do not rebuild any of it)

| Area | State |
|---|---|
| Waves | 50, formula-generated, flatter ramp. `boss: true` at 10/20/30/40/50 |
| Arenas | 9 — Core/Foundry/Bio-lab/Prism x2 passes + **Singularity** (galaxy, `first_wave: 50`) |
| Enemies | seekers, moon shooters (3 tiers), 4 specialty units (spitter/miner/bulwark/splitter) |
| Boss | themed per arena, summons adds, rewards 1 of 3 actives on a 30s cooldown |
| Difficulty | Normal / Hard via `apply_difficulty` (D50), plus `boss_mult` |
| Ships | data (`ShipDef` over `PlayerConfig`); lifetime score in `saves/meta.json` unlocks them; 4000 = Purple Gatling |
| Save | `saves/run.json`, run state only, pause -> SAVE, title -> CONTINUE |
| Shop | real clickable menu, 3 pages, gear levels, detail pane |
| HUD | gauges + 96px radar, hidden outside PLAYING/INTERMISSION |
| VFX | arena destruction across the 5s crossfade ("REACTOR SHIFT") |
| Particles | cap **4000** (D84). Measured peaks: boss+actives 1998, arena destruction 463 |

---

## Iteration 5 — the 13 items, pre-split into lanes

Dispatch these as parallel agents in **git worktrees** (`isolation: "worktree"`). The
protocol that has worked for 11 lanes across two iterations:

- **File ownership is absolute.** A lane edits only its own new files, its own hook block
  in `main.cpp`, and its own `GameData.json` blocks. `component_storage.*`,
  `components.hpp`, `collision_layers.hpp`, `player_components.hpp` and every CMake list
  are **off-limits** — a lane that thinks it needs one **stops and reports**.
- Reserved decision-id ranges per lane, listed below. The **integrator** bumps
  `CLAUDE.md`'s next-free-id line — never a lane, or it is N conflicts.
- Every lane: a spec from `specs/feature-template.md`, tests in `CPP/game/tests/unit/`
  (that dir globs — no CMake edit), its own gate run, and **do not commit** (the
  integrator commits in the worktree and merges).
- `decisions.md` conflicts on every merge because every lane appends. Resolution is
  always "keep both sides".

### Lane L — art overhaul (**D105-D112**) — the biggest lane

Items **#8, #10, #11, #1, #3**. Generator work in `assets/generator/v2/`.

- **#11 Sprite overhaul for enemies** — graduate to more complex drone sprites while
  keeping the neon arcade aesthetic. All of `enemy_*.png`.
- **#10 The Foundry boss "Capital Drone Carrier" is currently a grid of hexagons.** The
  user asked for **extra effort** on this one, explicitly. It is the marquee sprite.
- **#8 Player ship should read as a drone**, not the current arrow/dart.
- **#1 Projectiles should be red** (currently not).
- **#3 Moon shooters must fire from the crescent opening, not the back.** This is a
  *spawn-offset* fix in `enemy_fire_system.cpp`, not art: the shot origin is presumably
  the entity centre or its facing vector; the crescent's mouth is the lit-lobe side.

**Pillow is not installed system-wide and must not be.** A private copy lives at
`/tmp/claude-1000/-home-conrad-Documents-GameEngines-reactor-drone-v2/b674fb89-35a4-492e-9256-c3dbe64fd74d/scratchpad/pylibs`
— run generators as `PYTHONPATH=<that> python3 assets/generator/v2/make_sprites.py`. If
that scratchpad is gone, re-create it with
`python3 -m pip install --target=<dir> Pillow` (do NOT `pip install --user`, do NOT venv —
`ensurepip` is missing on this box so `python3 -m venv` cannot bootstrap pip).

`make_backdrops.py` rewrites `bg_core_*`/`bg_foundry_*`/`bg_biolab_*` on a bare full run
(they predate a `hash()`->`crc32` fix); it takes arena-name args to scope it.
`make_sprites.py` is byte-reproducible and safe.

**`Images` names are relative to `assets/images/`** (`ResourceManager::load_texture`
prepends it): `"v2/coin.png"`, NOT `"images/v2/coin.png"`. Sidecar paths ARE relative to
`assets/` and DO carry `images/`. That exact confusion shipped a broken path once.

### Lane M — pause menu, stat overview, item slot (**D113-D119**)

Items **#2, #5, #13, #4**.

- **#2 The pause menu has overlapping text — AGAIN.** Treat this as a regression to
  diagnose, not a nudge. Lane H (D85) added `fit_text_in_rect` in
  `ui_render_math.hpp` and verified the pause screen by reading BMP pixels at frame 101,
  reporting all four strings inside the panel. Lane K then added the `pause_save` widget.
  **Get a screenshot of the current state first** and find out which is true: K's button
  changed the layout after H measured, or `fit_text_in_rect` does not cover this case.
  Guessing here will waste the lane.
- **#5 Character / stat overview in the pause menu**: fire rate, base speed, base shield,
  base HP, the effect of each purchased upgrade, and what gear/items are equipped.
- **#13 Active-item slot, bottom-left**, showing the item taken from a boss, with the
  key to use it printed along the bottom of the square.
- **#4 Minimap equidistant from the top and right edges**, and health packs must show as
  green blips (currently missing/wrong colour).

Minimap blips are pooled `UIElement` widgets on the `gameplay` screen (D58) — that is the
right mechanism, keep it. `MinimapSystem` writes `minimap.x/y/size` over the authored
panel rect, so the JSON rect is a placeholder and the system is the geometry authority.

### Lane N — controls & bombs (**D120-D124**)

Items **#6, #9, #7**.

- **#6 Dash on SPACEBAR with a 10s cooldown** (currently LSHIFT, ~0.15s burst).
  **Careful:** SPACE is the title-screen start key, and `HANDOFF` history records that at
  `PHASE_TITLE` the start fallback is `space_edge` and NOT `advance`, precisely because
  `advance` includes the mouse click. Rebinding dash to SPACE must not make the title
  screen start a run when the player dashes, or vice versa.
- **#9 Bombs (Foundry mines): smaller detonation radius, and destroyable.** Destroyable
  means they need to take damage — check whether they carry `Health` and whether
  `ProjectileHitSystem` can already hit them, before adding anything.
- **#7 More distinct visual impact when buying an upgrade on the ship** — the purchase
  should visibly change the drone.

### Lane O — 30 waves + prestige (**D125-D131**) — design change, not tuning

Item **#14**. *"Reduce wave count to 30, scale difficulty accordingly. Allow option instead
of keeping going to do a 'prestige' run which upgrades the ships base stats but strips
upgrades and you start the run over."*

- Rescale 50 waves -> **30**, keeping total pressure sensible. The current table is
  formula-generated (D53) — regenerate it, do not hand-edit 30 rows.
- **Prestige is persistent progression, so it belongs with `saves/meta.json`** (Lane F's
  `meta_save.*`, D80-D83), alongside lifetime score. Read D80 first: persistence was
  deliberately kept to derived-not-stored state, and unlocks are computed from lifetime
  score rather than saved. Prestige level should follow the same discipline.
- Base-stat buffs must be applied at the **single** `start_run` site where `apply_ship`
  and `apply_difficulty` already run. `apply_difficulty` is **not idempotent** (D50) and
  `main.cpp` re-copies a pristine `const GameConfig base_config` each run. Do not create
  a second application site.
- Bosses currently sit at 10/20/30/40/50 — decide and state where they go in a 30-wave arc.
- **Determinism:** prestige is startup-read persistent state that changes simulation
  values. Lane F kept its read out of the simulation path so the canary held; a prestige
  bonus genuinely changes the sim, so the canary must be run **at a fixed prestige level**
  and that must be documented. Say explicitly how you kept replay reproducible.
- Coordinates with Lane M (#5 stat overview must show prestige bonuses). Lane O owns the
  meta/state; Lane M owns the screen. Neither edits the other's file.

---

## Traps that have each cost real time

1. **`ComponentStorage` uses explicit template instantiation.** A new component type needs
   a storage member, two `get_storage<>` specialisations and six instantiation lines, or
   you get a wall of undefined references that reads like a build-system fault. This is
   why new component types are banned inside lanes.
2. **`CPP/game/*.cpp` and `CPP/game/tests/` both glob**, so a new game source needs no
   CMake edit — but the glob is evaluated at **configure** time, so an existing
   `CPP/build` fails to link until you re-run `cmake -B CPP/build -S CPP`. Engine `.cpp`s
   ARE listed explicitly, in `CPP/game/CMakeLists.txt` and twice in
   `CPP/engine/tests/CMakeLists.txt`.
3. **A screen-space entity still needs a `Position`.** `render_system.cpp:110` iterates
   `entities_with_component<Position>()` and only *prefers* `ScreenPosition` for entities
   already in that list — no `Position` means it never draws. True for HUD **text** only,
   which `HUDSystem` renders through a separate path. Two working fixes: keep `Position`
   and rewrite it from the inverted camera transform (D63), or make it a `UIElement`
   widget (D58). This cost two lanes time.
4. **Editing `GameData.json` with a Python round-trip reformats the whole file.** Splice
   by line range, re-dump only the block you replace (`json.dumps(obj, indent=2)` plus a
   2-space prefix matches house style), then re-parse to prove validity. A bad slice once
   silently wrote `"ustain"` as a top-level key; the re-parse is what caught it.
5. **Determinism is a project invariant.** RNG draws happen on every path, in a fixed
   order. `EnemyDeathSystem::drop_loot` is the reference implementation. A rejection loop
   that draws a variable number of times breaks replay silently — Lane K's loot placement
   solved it by making the search **draw nothing** (a fixed pure golden-angle spiral over
   16 candidates), and pinned it with a test that two identically-seeded worlds, one
   blanketed in hazards, produce byte-identical positions on the *next* kill.
6. **`--fps N` does NOT speed up a headless run** — the Timer still paces to 60fps, so
   60000 frames is ~17 minutes. To reach a late state, temporarily relax the trigger,
   verify, then **revert it and say so**. Do not brute-force frame counts.
7. **`SDL_VIDEODRIVER=dummy`, never `offscreen`** — offscreen supplies no mouse, so the
   drone never aims and a zero score is expected rather than a bug.
8. **Pause freezes the frame counter**, so scripted frame-indexed keys stop advancing.
   But a `--clicks` scheduled *at* the frozen frame **re-fires** — that is how Lane K
   drove SAVE headlessly. Useful, non-obvious.
9. **`--screenshot N` writes a BMP** to the log dir. Reading those pixels back is the only
   way to prove something renders; a passing unit test does not. Lanes B and H both did
   this. There is no Pillow in a worktree, so H wrote a small BMP reader.
10. **`spawn_world()` destroys every entity** and explicitly skips `UIScreen`/`UIElement`
    carriers. Any teardown must do the same. It is also why the minimap blip pool survives
    across runs.
11. **`apply_difficulty` is not idempotent** (D50). Anything difficulty-scalable scales
    there, from the pristine `base_config`, never in a second place.
12. **`remove_component<ContactDamage>` is not instantiated.** Lane E worked around it (no
    collider means no `CollidedWith`, so the damage is inert). Anything wanting to strip
    it must stop and report instead.

---

## Known open items, not yet assigned

- **`Game_Property_Tests` has a rare pre-existing flake**: `test_cli_parser_properties.cpp`
  generated `256:200,-855` and failed on a negative click coordinate. Passed 20 consecutive
  re-runs afterwards. Real, unrelated to any lane, unfixed.
- **The tiled black hole.** `bg_galaxy_mid.png` is a single centred black hole per 512px
  tile, so at runtime the Singularity arena reads as a *field* of singularities rather than
  one dramatic object. Fixing it means a non-tiled sprite entity. The user has not ruled on
  it.
- **Waves 26-50 are waves 1-25's layouts rotated 90°** (mechanical, labelled provisional in
  D53). If the arc drops to 30 waves this may resolve itself — or need real authoring.
- **Balance is unmeasured.** Every number is formula-generated or eyeballed. The user has
  now played to wave 12 once; that is the entire body of evidence.
- **No audio.** `assets/Audio/` holds generated `.wav` files; nothing plays them. There is
  no audio system, no mixer, no SDL audio init.

## Working agreement the user has settled into

- They dispatch parallel lanes and expect a **merge report per lane**, with the *actual*
  gate output — not a claim that it passed.
- They value being told what was **not** verified. Every lane states plainly what it could
  not see (nothing is playtested in a window until they play it).
- Tests passing is not a playtest, and the project rule says to state which verification
  actually ran.
