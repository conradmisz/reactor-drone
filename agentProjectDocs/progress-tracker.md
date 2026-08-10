# Progress Tracker

Ephemeral state only — lasting decisions go in `decisions.md`.
Keep under ~60 lines; collapse old Completed entries to one line each.

## Current Phase

- **Iteration 3 Phase 0 (scaffolding) is done and green** — see D51 and
  `ENGINE.md` §6. It adds no behaviour; the canary is byte-identical. The five
  feature lanes it unblocks (plan:
  `~/.claude/plans/create-a-plan-to-polymorphic-gosling.md`) have not started.
- Everything from visual Phase 5 onward — the UI layer, Phase A, Phase B and now
  Phase 0 — is still **uncommitted** in one large tree, no slices chosen yet.

## Current Goal

- Get the uncommitted work committed in sensible slices, then **play it**: the
  Phase B numbers are the user's stated target, not a measured one, and nobody
  has seen Phase A or the new title menu in a real window.

## Completed

- v2 Phases 0-4 (committed, 2026-07-09/10): repo bring-up, procedural neon asset
  pipeline, render upgrades (Tint/additive/flip/layers), particle system, hit
  feedback + screen shake.
- Arena upgrade Phases 1-5 (committed, 2026-07-26/27): aim & movement feel, big
  arena + follow camera, boundary wall, solid sprite terrain, one window-size
  authority.
- Gameplay Phases 1-4 (committed, 2026-07-28): 20-wave arc with timed waves and
  arena-clear gating, currency economy + pickups (XP system deleted), shop with
  6 escalating upgrades, gear page with 4 items + 4 consumables.
- Visual Phase 5 (uncommitted): arena crossfade, luminance enemies tinted per
  arena, the Prism arena + assets, item auras, directional thruster cone, the
  shop particle leak fix.
- UI/menu layer (uncommitted): Option-040 port + tests, `ui_styles`/`screens` in
  `GameData.json`, `pulse_hz`, Blackboard click dispatch, the wave-intermission
  prompt, HUD rebuilt as UI widgets, and the `spawn_world()` fix that was
  destroying the menus.
- Phase A fix-ups (uncommitted): live intermission, ESC → pause screen, hull /
  shield chip gauges, menacing hazards, the 5 s arena transition.
- Gameplay Phase B (uncommitted, 2026-08-09): `main_menu` difficulty select,
  `DifficultyDef` + `apply_difficulty` (D50), Hard mode, and a rebalanced
  "aggressive" Normal wave table. Spec: `specs/difficulty-modes.md`.

- **Lane H — UI & menu overhaul (uncommitted, 2026-08-09):** text fitting in the
  renderer so no label can leave its widget (D85), the arena HUD + radar hidden
  outside a playing phase (D86), the HUD's text rows moved onto the design canvas
  (D87), all six screens re-laid out on one grid with a real type scale and state
  feedback (D88), the shop tooltip promoted to a fixed detail pane (D89), a
  smaller radar-style minimap, and "REACTOR SHIFT" (D90). Spec:
  `specs/ui-and-menu-overhaul.md`.

- **Lane L — art overhaul (iteration 5, 2026-08-10):** the boss's "Capital Drone
  Carrier" sprite and the `Images`-must-not-wear-an-atlas fix (D105), sprites on
  the boss's adds (D106), the player and the four generic enemies rebuilt as
  rotor drones (D107), red player projectiles (D108), and moon shooters that face
  their target and fire from the crescent's mouth (D109). Spec:
  `specs/art-overhaul.md`.

- **Lane M — pause menu, stat overview, item slot (iteration 5, D113-D117):**
  the pause screen re-authored after diagnosing #2 (a widget appended onto a full
  column, not a text-fit failure — D113), a character sheet on it (#5, D117), the
  bottom-left active-item slot (#13, D116), and the minimap moved to equal window
  margins with green health blips (#4, D114/D115). Spec:
  `specs/pause-overview-and-item-slot.md`.

- **Iteration 5 integration (2026-08-10, D132).** Lanes L/M/N/O merged to
  `master` in that order; `decisions.md` and `progress-tracker.md` conflicted on
  every merge (both sides kept, as always). One real cross-lane defect found at
  integration and fixed: the prestige blackboard seam between Lane O (producer)
  and Lane M (consumer) disagreed on key *and* type, so the pause sheet's
  PRESTIGE row could never appear. Fixed in `pause_stats.cpp` and pinned by a
  test in `test_pause_screen.cpp` (D132). Gate re-run on every merge commit, not
  just at the end: build clean, `ctest` 8/8, canary byte-identical twice at
  prestige 0. **Nothing in iteration 5 has been playtested in a window.**

## In Progress

- Nothing being edited right now. The tree is green: `cmake --build` clean,
  `ctest` 8/8 (152 new assertions in `test_difficulty.cpp`), only Lua's vendored
  `tmpnam` warning.

## Next Up

0. **Run the five iteration-3 lanes** (A data spine → then B/C/D/E in parallel,
   merge order A → B → C → E → D). Lane rules, file ownership and reserved
   decision-id ranges are in the plan file; the `// === HOOK: <lane> ===` blocks
   in `main.cpp` are where each one lands.
1. **Commit the working tree in slices** — visual Phase 5 first (it was complete
   before the UI work started), then the UI layer, then Phase A, then Phase B,
   then Phase 0.
2. **Play a full run.** Five phases have now shipped unplayed, and Phase B's
   balance is a stated target with no measurement behind it.
3. **Phase C content** — moon enemy types 1-3, health/shield pickups, the shop
   as a real clickable menu, the halfway boss + its three active items, and an
   Options screen. See `HANDOFF.md`.
4. **Remaining menu screens** — `game_over`, `victory`, `save_slots`; wire
   `ScreenFadeSystem` between `hud_system.render` and `ui_render_system.render`
   (ported, still unused); update `README.md`.
5. **Run-state save/load (Part 5)** — `CPP/game/run_save.{hpp,cpp}`, flat
   `RunState`, atomic write, autosave at the intermission edge, manual save from
   pause, `saves/` in `.gitignore`. Remembering the last difficulty rides on it.
6. **Audio — last, and deliberately rip-out-able.** Keep it to one engine file
   pair, one CMake block, one trigger site list and one JSON block, so a single
   revert removes it if it feels wrong.

## Open Questions

0. **Phase B's numbers are unplayed.** Normal's rebalanced early waves and every
   Hard multiplier are provisional; the least-justified part is waves 5-11,
   rescaled only to stay above the new wave 4. Also unknown: whether Hard's
   ×1.5 count blows the 2000-particle budget in the late fixed waves.
1. **The four balance numbers, still unmeasured** (from `handoff-phase-4.md`
   §8): credits banked at the wave-4 shop; the shape across the 8/12/16 stops;
   total earned waves 1-8 vs 9-16; whether the run ends with unspent credits.
   Every fix is a `GameData.json` edit with no rebuild.
2. Does the Repulsor Field's 35 px/s read as anything? Flagged as the likeliest
   dead number.
3. `README.md` says a C++20 toolchain; `CMakeLists.txt` sets C++17. Fix the
   README (or the standard) once someone decides which is true.
4. Should `ScreenStackSystem::is_modal()` fold into the existing `sim` flag
   rather than becoming a second pause concept?
5. `--dump` and `--trace` are parsed and never consumed; `wave_config.hpp` is
   dead. Delete or implement.

## Session Notes

- **Iteration 6 (2026-08-10): the modular drone (D133/D134).** The player chassis
  is redesigned to wear its upgrades — slimmer hull, pods outboard, hardpoints
  drawn empty — and each shop row has an overlay worn by a follower entity, with
  the Shield Capacitor promoted to a live field ring instead of a static part.
  Spec: `specs/modular-drone-and-upgrade-kit.md`. New: `kit_*.png` x7 +
  `shield_field.png` (21 frames) from the generator, the kit/field half of
  `upgrade_visuals.hpp`, `test_kit_visuals.cpp`, follower creation in
  `spawn_world` and the per-frame update beside the item aura, and a cosmetic
  `player.hit_bearing` publish in `player_damage_system.cpp`.
  Verified: build clean (only Lua's `tmpnam`), `ctest` 8/8, manifest OK (13
  sidecars), canary byte-identical twice on `--seed 42`, and real in-game frames
  captured via `--screenshot` under a TEMPORARY full-kit patch (**reverted** —
  `grep "TEMPORARY VERIFICATION" main.cpp` returns 0). **Not playtested in a
  window.** Design sketches were reviewed and approved before any code.

- **Art (2026-08-10): the sprite generator supersamples.** Pillow's polygon/line
  rasteriser has no antialiasing, so every 128px sprite shipped with stepped
  neon outlines that only the baked halo hid. `make_sprites.py` now draws the
  whole S-family on a 512px canvas through a `ScaledDraw` proxy (authored
  coordinates, pen widths and radii multiplied by `SS=4`) and box-filters down
  in `shrink()` before `add_halo` — the trick `carrier_sprite()` already used
  (D105), applied to the roster. Shape code is unchanged: it still authors in
  128-space. Art already authored at its working size (the carrier, the 96px
  pickups) passes `s=1` and is byte-identical apart from the carrier's
  LANCZOS -> BOX swap. Generator run: 0.35s -> 0.78s. Verified: manifest test
  OK (12 sidecars), `ctest` 8/8, headless canary byte-identical twice on
  `--seed 42`, before/after sheets read at 3x zoom and at true game size under
  arena tint. **Not playtested in a window.**
  Pillow is not installed system-wide here (PEP 668); the wheel was unpacked to
  a scratch dir and used via PYTHONPATH.

- **Docs (2026-08-10): `docs/features.html` refreshed to the shipped game.** It
  was written against the 20-wave/4-arena Phase-4 game and its header still
  called the dash/minimap/boss/sustain/actives blocks "inert scaffolding". Now
  covers the 30-wave arc, 9 arenas over 4 themes, bosses on 10/20/30 + actives,
  moon shooters, arena specialists and mines, dash on SPACE, minimap, sustain
  scrap, Long Barrel / Ricochet Coils, ships + lifetime score, run save /
  CONTINUE, live intermission, the `% 5` shop cadence and prestige. The rail's
  wave ladder is 30 ticks banded off `arenas[].first_wave` with the boss waves
  marked. Verified: HTML nesting balanced, page JS parses (`node --check`),
  BANDS reproduces 1/4/8/12/16/19/23/27/30. **Not opened in a browser.**
  Media slots are still placeholders (`docs/media/` is empty).

- Verified 2026-08-09 (Phase B): `cmake --build` clean, `ctest` 8/8, both
  difficulties replay-identical on `--seed 42`, and an idle drone dies before
  frame 900 on Hard but survives past it on Normal — the proof the multipliers
  reach the sim rather than just the label. Still nobody has played it.
- Headless difficulty select: `--clicks 10:358,349` = NORMAL, `10:622,349` =
  HARD (design-canvas centres through `ui_canvas_transform`); `--keys N:SPACE`
  still starts Normal.
- Verified 2026-08-09: `cmake --build CPP/build -j` clean, `ctest` 8/8 in 45 s.
- Headless: `SDL_VIDEODRIVER=dummy`, and `--fps` does *not* speed a run up — to
  reach a late wave, temporarily change the `% 4` shop trigger and revert it.
- A windowed run absorbs real desktop mouse input and will corrupt a scripted
  run; `offscreen` isolates it but has no mouse, so a zero-score run there is
  expected.
- **Lane M (2026-08-10) — pause/HUD readouts (D113-D117).** Verified: build with
  no warning from our code, `ctest` 8/8, the replay canary byte-identical twice,
  and BMP pixel reads of a paused frame 100 showing the re-laid-out panel with no
  overlap, plus the green health blip at (936,70) inside the radar and the item
  slot bottom-left (that last one under a TEMPORARY `active_id = 1` edit in
  `pause_stats.cpp`, since a headless run cannot kill a boss — **the edit was
  reverted**). **Not playtested.** D118-D119 were reserved and are unused.
- **Lane K (2026-08-09) — run save/quit + loot placement (D100-D103).** Pause ->
  SAVE writes `saves/run.json` (run state only: wave, credits, score, hull,
  shield, gear, items, ship, difficulty, seed); the title screen's
  `menu_continue` widget offers CONTINUE when one exists and resumes through the
  same `start_run` path a fresh run uses. Coins now nudge off obstacles, hazards,
  mines and other loot along a pure golden-angle spiral — no RNG in the search,
  so the draw count per kill is unchanged. Verified: `ctest` 8/8, zero warnings
  from our code, and the replay canary byte-identical across four runs (twice
  with no save file, twice with a populated one). **Not playtested** — the
  headless canary dies in wave 1 with zero kills, so the placement change is
  covered by `test_loot_placement.cpp` rather than by a live run.
- Lane L verification (2026-08-10): build clean (only Lua's `tmpnam`), `ctest`
  8/8, canary byte-identical twice on `--seed 42`. Sprites were checked by
  reading `--screenshot` BMPs, not by a playtest: the carrier, its adds and a
  crescent turned toward the drone were all confirmed in-frame after a
  *temporary* `"boss": true` on wave 1 and `moon_1.first_wave = 1`, both reverted
  with `git checkout -- assets/GameData.json`.


- **Lane N (2026-08-10) — controls & bombs (D120-D123).** Dash moved to SPACE on a
  10 s cooldown, edge-triggered so the title screen's start press cannot also
  spend it (`--keys 10:SPACE 200:SPACE 260:SPACE` shows the run start at 10, the
  dash at 200, and 260 refused by the cooldown). Foundry mines: blast 150 -> 100
  and an armed mine detonates when shot, consuming the shot — no `EnemyTag` and no
  `Collider`, so wave-clear/minimap/loot are untouched. Shop upgrades now drive the
  drone's engine plume (`upgrade_visuals.hpp`) and the shop's preview glow.
  Verified: `ctest` 8/8, zero warnings from our code, canary byte-identical twice.
  **Not playtested**, and the mine changes are covered by `test_lane_n.cpp` only —
  the headless canary never reaches the Foundry.

- **Lane O (2026-08-10) — 30-wave arc + prestige (D125-D131).** The wave table is
  regenerated at 30 rows (1-15 fixed-count, 16-30 timed, bosses 10/20/30, endpoint
  ~10% below the old wave-50 values because the `% 5` shop now opens 6 times, not
  10). Everything wave-indexed rescaled with it: arenas at `first_wave`
  1/4/8/12/16/19/23/27/30, moon shooters at 3/9/18. Prestige is one clamped field
  in `saves/meta.json`, applied as `apply_prestige(config.player, meta.prestige)`
  at the single `start_run` site, offered on the new `prestige_offer` screen when a
  run reaches PHASE_VICTORY. Lane M's stat overview reads `prestige_summary(level)`
  and the Blackboard key `prestige.level`.
  Verified: build clean (only Lua's `tmpnam`), `ctest` 8/8, and the canary
  byte-identical twice at prestige 0 and twice at prestige 3 — **the canary is now
  defined at a fixed prestige level** (D130). The buff was proved to reach the sim
  by a runtime difference, not just a unit test: at `--stopframe 1150` a level-0
  drone is dead (Phase 2) and a level-5 drone is alive (Phase 1). The offer screen
  and its click were driven headlessly by temporarily also raising it on
  PHASE_GAMEOVER (`--clicks 1200:490,394` -> `Prestige: 1` and a fresh run);
  **that relaxation was reverted**. **Not playtested**: nobody has reached wave 30,
  so the ramp, the boss spacing and the prestige percentages are all unmeasured.

- **Field manual redesign (2026-08-10) — docs/features.html (D135).** Full visual
  redesign after an owner interview (direction: refined neon arcade, Orbitron
  display + vendored OFL fonts, real media, JSON-driven content). All game
  content now lives in the `#gamedata` JSON blob inside the file — edit data,
  not markup. Real gameplay media captured headlessly into `docs/media/`
  (7 stills + 2 animated-WebP clips) via `docs/media/capture.sh`; the capture
  runs used a temporary GameData buff that was reverted (`git checkout`), the
  repo's GameData.json is untouched. Verified in headless Chromium (Playwright):
  no console errors, 4 palettes, mobile, desktop, full scroll. **Media caveat:**
  captures show capture-buff HUD numbers (inflated credits/hull); recapture
  after balance settles using the script's header instructions.

- **Arena prop art overhaul (2026-08-10) — D136.** Fifteen bespoke sprites
  replace the two shared shapes: a new directional wall family (segments now
  rotate to the ring tangent via a `Rotation` component in
  `spawn_arena_props`), per-theme obstacles designed for their real layout
  aspect ratios, and shape-first hazard vents. `make_backdrops.py` gained
  `--props-only`. GameData's nine `wall_image` fields point at `wall_*.png`.
  Verified: ctest 8/8, canary byte-identical twice, headless captures of the
  Foundry rampart and a mid-crossfade prop swap. **Not playtested** beyond
  scripted runs; Prism/Bio-lab/Galaxy walls seen only in sprite form, not
  in-arena.
