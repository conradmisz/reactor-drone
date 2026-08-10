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

- **Lane M — pause menu, stat overview, item slot (iteration 5, D113-D117):**
  the pause screen re-authored after diagnosing #2 (a widget appended onto a full
  column, not a text-fit failure — D113), a character sheet on it (#5, D117), the
  bottom-left active-item slot (#13, D116), and the minimap moved to equal window
  margins with green health blips (#4, D114/D115). Spec:
  `specs/pause-overview-and-item-slot.md`.

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
