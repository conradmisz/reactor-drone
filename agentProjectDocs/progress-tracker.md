# Progress Tracker

Ephemeral state only — lasting decisions go in `decisions.md`.
Keep under ~60 lines; collapse old Completed entries to one line each.

## Current Phase

- **Gameplay pack v2.3 in flight** — spec `specs/gameplay-pack-v2.3.md`, plan
  `plans/gameplay-pack-plan.md`, decisions D221/D222. Tier 0 (spec+baseline)
  and Tier 1 (data model: 4-ship roster with stats, first-class weapons,
  persistent scrap on meta.json, equipped loadout, Gryphon forest atlas) are
  DONE. Tier 2 DONE: per-weapon projectile identity (size + pierce — Moonshot
  wide piercing crescent, Flak big slag), ship specials (Falcon equip_cd 0.75,
  Owl phoenix veil <10% -> 4s invuln+no-fire w/ 25% re-arm, Gryphon ram dash
  +15 shield & positional shove) in ship_specials.hpp + dash_system.
  Tier 3 DONE:
  right-mouse secondaries (55 Iron hold-to-charge w/ scaled cd, Moonshot radial
  crescent burst, Flak 3s lava stream + Burn DoT, Hailstorm traveling blizzard
  + Chill slow) in secondary_fire.{hpp,cpp}; Burn/Chill/BlizzardTag registered
  in the ECS (ENGINE.md updated); hud_sec gauge on the gameplay screen; "RMB"
  scriptable via --keys. 399 unit cases + 8/8 ctest, canary byte-identical on
  baseline; headless RMB probe fires (score diverges as expected). Tier 4 DONE: seeded
  arena shuffle (no Prism opener, Singularity pinned; 200-seed property test),
  player-enemy solidity (dash + Owl veil excepted, bounce-out), early-wave
  drop floor, sustain cap 1->4 at wave 16 + 1.5x pickup size. **Canary
  re-baselined ONCE (D223): the expected line is now Frames: 3000 Final
  score: 170 Units: 20 Wave: 2 Phase: 1** — CLAUDE.md + .canary-baseline.txt
  updated, gate.sh green. 401 unit cases + 8/8 ctest. Tier 5 DONE: 2-phase
  enrage below 50% (cadence x0.6, speed x2, +6 volley shots / +2 patches,
  Flash+HUD cue, knobs in the boss JSON block), positional unstick slide for
  the stuck-behind-structures bug, reward menu deferred until the boss's adds
  are dead, actives trimmed to Heat-Seeking Missiles (reward_choices 1; more
  items TODO in spec). 401 unit + 8/8 ctest, gate.sh on the new baseline.
  Tier 6 DONE: run_setup expanded
  into the hangar (right-side panel; the LIVE world drone reskins on ship
  cycle = the preview; CHANGE SHIP / CHANGE WEAPON / BUY-with-scrap rows;
  8 stat rows with ONE aligned pip column via hangar_stats.hpp; big green
  LAUNCH bottom-left) + run_stats flight report pushed on death/victory
  before the prestige offer (CONTINUE/ESC pops; retry + prestige gated on it).
  Contract-tested in test_hangar_screen.cpp (the intermission-test pattern).
  NOTE: drive_ui XTest CLICKS do not register under the current COSMIC
  desktop (hover works, click lost) — real-mouse click-through is OUTSTANDING
  and folds into the windowed playtest. Next: Tier 7 (cosmetics).

- v2.2.0 shipped 2026-08-16; release notes collapsed below.

  Released off `feature/distribution`: engine suite (all flags off), The Shroud,
  The Drift, and `--dev` starting fully kitted. All five gates verified in the
  bugs/008 order — four assets 200 anonymously, Latest moved, Worker `/version`
  2.2.0 with a resolving installer_url, live site links 200. CI green on all
  four jobs. Worker and Pages both redeployed; both repos pushed.

  **Both bugs/008 CI fixes survived their first real run** (`draft: false`,
  `make_latest: true`) — proven now, not just written down. New trap recorded:
  `/version` lagged the deploy ~10 s while still reporting the old version.

  **The in-game updater is finally live for real.** Anyone on 2.1.0 now sees
  UPDATE AVAILABLE, so the Windows silent-install handoff
  (`SDL_CreateProcess(setup, "/SILENT")` -> installer -> relaunch) has a genuine
  2.1.0 -> 2.2.0 jump to exercise. That path has never run on any machine.

  **What merged (D220):**

  `engine-suite-build` merged (D138-D151): force fields, resonance grid,
  timescale/bullet time, adaptive director, surges, bullet patterns,
  destructible arena, palette engine, chip-synth audio, flight report, plus the
  A* blocked-cell fix. **Every feature is OFF** — `enabled: false` in
  GameData.json, `forces` inert by shape — so the sim is byte-identical to
  pre-suite master. `--suite` turns them all on at once; `bash gate.sh
  .canary-baseline.txt` re-proves inertness after any change.

  Cherry-picked `545e12f` from `experimental`: **The Shroud** (wave 10,
  darkness past `light_radius`) and **The Drift** (wave 19, directional
  current). The 30-wave ladder is re-banded to 11 arenas —
  1/4/7/10/13/16/19/22/25/28/30.

  **The rest of `experimental` was deliberately NOT merged** and should not be:
  upgrade tracks, rolled shop cards with reroll/lock, ability slots on 1-4,
  fusion capstones, the three hulls and ship installs. gameplay.md supersedes
  all of it — merging would mean merging code in order to revert it. In
  particular `experimental` demotes `ShipState.upg_counts` to legacy and moves
  purchase truth to `track_levels`, while gameplay.md says to KEEP the original
  upgrade method. Note those files (`shop_system.*`, `player_components.hpp`,
  `run_save.cpp`) auto-merge with ZERO conflicts because this branch never
  touched them — git would take the roguelite version silently.

  Verified at the merge: warning-free build, 8/8 ctest,
  `scripts/verify_branch.sh` **30/30**, and the firing replay canary unchanged
  (`Frames: 3000  Final score: 100  Units: 24  Wave: 1  Phase: 1`).

  **Unplayed:** the owner played the merged build once at `--dev --level 8`
  before release — real coverage of the merge, but neither new arena has been
  seen at its own band (Shroud wave 10, Drift wave 19) and no suite feature has
  ever been enabled outside a scripted run. `--suite` turns them all on.

  **OPEN, highest-value cleanup before the rewrite:**
  `scripts/verify_branch.sh` section 3 still runs the IDLE canary
  (`--keys 5:SPACE`) — the branch gate has reported 30/30 across the whole v2.x
  line while never firing a shot. `gate.sh` was fixed this session;
  verify_branch was left alone because it is the shipping gate. Full write-up
  and the replacement command in `bugs/006`. The gameplay.md rewrite touches
  firing directly, so fix this before it starts.

- **v2.1.0 SHIPPED (2026-08-15) — the v3 visual overhaul is live on
  `feature/distribution`.** `visual-overhaul` merged in (tiers 0-12) and is
  fully contained here; that branch needs nothing further. Release published,
  marked Latest, all four assets verified 200 anonymously; Worker
  RELEASE_VERSION=2.1.0 deployed and its installer_url resolves; website
  redeployed (Pages does NOT auto-deploy on push — `wrangler pages deploy site/`
  in the brainstormlabs repo). Two commits landed AFTER the tag (CI
  auto-publish, the D-number renumber) — docs/CI only, so the tag still matches
  what players run.

  **Merging to master is deliberately NOT wanted — the distribution branch is
  intentional (owner, 2026-08-15). Stop proposing it.** It is 75 commits ahead
  of master and master has nothing unique.

  **Unjudged by a human:** the tiers 10-12 look (tracer, layered explosion,
  glow/bloom pullback, chromatic aberration 0.0028 -> 0.0006) shipped on my
  screenshots alone. The owner played the merged build once but never gave a
  verdict. `DISC_SCALE = 2.5` in `render_particles` is a by-eye constant and is
  the first knob to touch if the glow reads wrong.

  **Never executed:** the Windows silent-install handoff in the new updater. It
  cross-compiles with mingw and the download path is tested, but
  `SDL_CreateProcess(setup, "/SILENT")` -> installer -> relaunch has never run.
  First real exercise is someone on 2.1.0 clicking the button against 2.2.0.
  Note the update button cannot help anyone on 2.0.0 — they lack the code.

  **Owed:** none blocking. `analytics/report.py` still deferred.

- **`feature/distribution` (2026-08-15, slice 2): telemetry, DB status and
  tech center on the dashboard** (`specs/dashboard-telemetry-and-status.md`).
  Outcome split (stacked bar, palette-validated both modes — segment order IS
  the validated adjacency, re-validate if reordered), wave histogram, the
  three-populations trio, per-table row counts + writes-today meter vs the
  100K/day tier, and a route/schema reference kept honest by a
  `verify_branch.sh` check that every worker.js route appears in the page.
  Two-column layout ≥960px. Still riding the one authenticated `/stats`
  batch. Remaining from the idea doc: heatmaps, UI funnel,
  feedback↔telemetry session links; `analytics/report.py` deferred until
  someone misses it. Deploy owed: `npx wrangler deploy` (secret already set
  if slice 1 was deployed).
- **`feature/distribution` (2026-08-15): dashboard inbox + mailing list panels**
  (`specs/dashboard-inbox-and-list.md`, proposed D203). `/dashboard` and
  `/stats` are now **authenticated** — Basic auth against a `DASH_PASS` secret,
  failing closed if the secret is unset — because the page now shows subscriber
  addresses and untrusted player-written feedback bodies. Two new panels
  (Feedback, Mailing list), two new tiles, poll relaxed 15 s -> 30 s.
  `/stats` still never emits a `player_id`.
- **Deploy owed for the above:** `npx wrangler secret put DASH_PASS` then
  `npx wrangler deploy`. Until that runs, production `/dashboard` is the old
  public page. The rest of `ideas/dev-dashboard-prompt.md` (telemetry explorer,
  DB status, tech-center half) is still unbuilt and still wanted.
- **Production is live as of 2026-08-15**: remote D1 migrated (all five
  tables), Worker deployed with telemetry + feedback + mailing list, and the
  junk pilots deleted — `/top` is empty and honest. Website deployed to
  `brainstormlabs.pages.dev` with the signup form. `thebrainstormlabs.com` is
  still on Squarespace nameservers; the plan is Connect (not Transfer — the
  domain is inside its 60-day ICANN lock until ~2026-09-03).
- **Still unplayed in a window.** The name-entry email field has never been
  seen rendered. The dashboard's new panels HAVE been screenshotted headlessly
  (chromium, seeded local D1) — feedback escaping verified against a
  `<script>` subject — but the ops page has never been opened by a human.

- **`feature/distribution` (2026-08-12): live-ops dashboard shipped and
  deployed** as D198 — `GET /dashboard` + `GET /stats` on the existing Worker.
  Verified against seeded local D1 and live production; four headless bot
  clients drove 19 real runs through `bank_run_score` end to end.
- Found while verifying: the built binary carried the old `127.0.0.1:8765`
  `NET_BASE` after the header was reverted, so runs banked locally and silently
  reached nothing. **`strings` the Windows artifact for `127.0.0.1` before
  tagging a release.** Also: 23 local runs vs 19 banked online — the divergence
  the telemetry spec names as the trigger for the deferred retry queue.
- Production D1 still holds ~9 junk pilots from Task 7/8 testing
  (`rtprobe`, `TestPilotXYZ123`, `CurlProbe1`, `AliceRace1*`, `FinalNameR1`,
  `ZZZ_TASK8_*`) plus the four `BOT_*` pilots. Public on `/top`; delete before
  release.
- **UNJUDGED BY A HUMAN: nobody has played any of this in a window.** The
  feedback form, ANALYTICS toggle, leaderboard screen and the 5-button pause row
  have only ever been driven by `scripts/drive_ui.py`. Tests passing is not a
  playtest.
- **Uncommitted on this branch (2026-08-13):** the mailing-list feature —
  `/subscribe` + `/unsubscribe` routes, `subscribers` table, CORS on every JSON
  reply, plus main.cpp/GameData.json changes and an untracked
  `specs/mailing-list.md`. Schema and routes are consistent (checked); the work
  is simply not committed. `wrangler deploy` ships what is on disk, so commit
  before deploying or production runs code that exists in no commit.
- **67 commits unpushed.** No remote has any of this.
- **D201 (2026-08-13):** the pause freeze is now stack-wide and PHASE_FEEDBACK
  handles input above the phase machine. Found two chained bugs: a screen
  pushed over pause silently un-froze the sim, and fixing that exposed a
  soft-lock (the form went inert, ESC included). `scripts/drive_ui.py` is the
  committed XTest harness for UI verification `--keys` cannot reach.
- **Feedback reports shipped** (2026-08-12, D200): in-game form (pause +
  main menu) -> POST /feedback -> flat-column D1 table with server ts and
  auto context (version/platform/identity/run state/session). Live-verified
  both entry points + failure path. Prod deploy rides the SAME pending
  migration as telemetry.
- **Telemetry tasks 1-5 built** (2026-08-12): `/telemetry` route + `runs`
  table (committed, NOT yet migrated/deployed to prod — needs
  `wrangler d1 execute --remote --file schema.sql` then `wrangler deploy`),
  telemetry.{hpp,cpp} + tests, tm.* counters, consent (ANALYTICS toggle +
  disclosure + PRIVACY.md, pulled ahead of collection), main.cpp collection +
  POST. E2E-verified against local wrangler dev; canary byte-identical to
  pre-telemetry baseline throughout. Remaining: Task 6 `analytics/report.py`,
  Task 7 doc sync.
- **Cross-platform distribution** (2026-08-12, D199): RD_PORTABLE flag +
  Linux tarball + mac .app jobs in release.yml. Linux verified end-to-end
  locally (alien dir, scratch HOME, bundled libs, windowed gameplay); mac
  authored but only CI-tested — **first tag push is the real mac test, check
  the build-mac jobs**. Windows path unchanged.
- **`visual-overhaul` (2026-08-11): v3 neon-polish branch started** — plan in
  `plans/v3-neon-polish-plan.md` (5 tiers). Tier 0 (vsync + backdrop restraint)
  Tier 1 (render-target bloom, D207) and Tier 2 (emissive separation, D208,
  38 generated `_glow` siblings) are committed. Verified per tier: clean build,
  ctest 8/8, canary byte-identical twice, headless screenshots read back
  (Tier 2's confirms the Tier 1 full-scene wash is gone). Tier 3 (hit-stop + camera zoom punch, D209 — trails already existed) is
  committed; its canary byte-identical (mouseless canary lands no kills, so hit-stop
  never fires in it; kill-bearing runs shift by design). Tier 4 (SPIR-V postfx via SDL_CreateGPURenderState, D210) is committed —
  OPT-IN via --gpu-renderer because the installed SDL prerelease wedges under
  it (bugs/009: mid-run segfault with bloom, readback segfault, teardown hang;
  all verified gone under SDL origin/main built in a scratch worktree).
  Tier 5 (immediate-mode neon line ribbons via SDL_RenderGeometry, D211:
  arena rink ring, obstacle outlines, laser-beam ribbons) is committed. ALL
  FIVE TIERS DONE. **Unplayed in a window** — a windowed playtest of the whole
  branch is the next real gate; then the pre-merge branch (user's plan).

- **`feature/gameplay-polish` (2026-08-11): the THIRD playtest batch is
  implemented** as D193 (one decisions.md entry, numbered by feedback item) —
  loot lifetime back to 14 s, +2 on every unit, a wave-15 credit vacuum and
  wave-15 BIG UNITs, the 48×48 ability row with an always-visible `ITEM` slot and
  a SPACE-captioned dash button under a 16-frame circular cooldown dial, SPACE no
  longer firing, a longer dash, the shop hold-bar made visible and its stat
  previews moved into the right-hand pane, bigger two-salvo missiles with a real
  rocket sprite, the square-cropped bubble shield fixed in the sprite generator,
  and a `--dev` god mode.
- Verified: clean build (only Lua's `tmpnam`), `ctest` 8/8 after repinning the
  drop value in `test_arena_properties.cpp` (3 → 5, the flat +2 landing), the
  replay canary byte-identical twice on `--seed 42` with the dev path present,
  and `--dev --level 12` starting with 999999 units.
- **Unplayed in a window.** Specifically unverified: whether the booster icon and
  the circular dial read at 48 px, whether the 16 px `DRONE STATS` rows shrink
  their values too far, and everything the earlier batches still owe.

- **`feature/gameplay-polish` (2026-08-10): the SECOND playtest batch is
  implemented** as D192 (one decisions.md entry) — shop hold-bar redesign,
  violet ship atlas, boss renamed to "drone", bombs damaging enemies, mushroom
  blast sprite, the boss's missed poison-image site, gas-not-flower poison,
  boss health bar, the primary-fire battery, a dash charge per boss, the
  currency renamed to UNITS with a digital chit sprite, and loot lasting 26 s.
- Verified: clean build (only Lua's `tmpnam`), `ctest` 8/8 (4 new cases —
  blast-hits-enemies, dash-charge refill, battery lockout, the new HUD widget
  contract), replay canary byte-identical twice on `--seed 42`, and headless
  captures read back: the three-bar HUD stack with no text overlap, and the
  boss bar + green poison clouds under a TEMPORARY wave-1 `boss: true` +
  Core→`bio_spitter` patch (**reverted**; `grep -c '"boss": true'` = 3).
- The earlier batch (D181-D191) and its two scope-only specs
  (`specs/mechanics-page.md`, `specs/tutorial-stage.md`) are still unplayed.

## Current Goal

- **Playtest both batches in a window.** Specifically unverified in a real
  window: the shop hold-to-buy wash (a scripted `--clicks` is instantaneous —
  down and up in one frame — so a sustained hold cannot be driven headlessly),
  the mine blast landing on enemies, the violet ship, and whether 12 s of fire
  / 3 s of recharge is the right battery tuning.

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

- **Main-menu suite (2026-08-10) — spec main-menu-suite, D137, 3 commits.**
  The title is now a hub (CONTINUE-newest / PLAY / SAVE SLOTS / RECORDS /
  HOW TO PLAY / SETTINGS / QUIT); PLAY opens run_setup (difficulty tabs +
  ship + LAUNCH); three save slots with load/delete and legacy migration;
  settings (screen shake, minimap) in saves/settings.json applied live;
  records from new meta.json fields (best_wave, runs_played); pause MENU and
  end-screen ESC return to the hub over a frozen world. All screens are
  GameData-authored on the D88 grid; every gate green per phase (build,
  ctest 8/8, canary x2 = pre-feature baseline). **Not playtested** — all
  verification headless (--clicks E2E + screenshots read back). Open edge:
  ESC-from-game-over shares the pause-MENU handler but was not click-driven;
  worth one real death-screen check next playtest.

- **Ability row, third pass (D193 revision, items 2/11).** Both HUD boxes
  64×64 → 48×48 (dash moved x 88 → 72); the empty boss slot says `ITEM` on one
  line instead of `EMPTY` / `BOSS`; and the dash button's face is now two
  generated sprites instead of a `▲` glyph and a horizontal bar — `hud_boost.png`
  (a real booster) plus `hud_dash_sweep.png`, a 16-frame clock wipe that greys
  out the whole box and sweeps clockwise back to clear. They are screen-space
  sprite entities, not widgets: placed in `main.cpp` right after `camera.update`
  (which owns `ScreenPosition`) from `hud_dash_frame`'s live rect, so they
  inherit the authored geometry and the HUD phase gate. New `hud_slot_frame`
  style = rim, no fill, so the UI does not veil them. Removed: three widgets,
  `DASH_CD_FULL_W`, and the `dash.cooldown` Blackboard key.
  Verified: `-fsyntax-only -Wall -Wextra -Wpedantic` clean on every touched TU
  and the generator re-run; `dash_sweep_frame` pinned in `test_pause_screen.cpp`
  (range + monotone). **NOT built, NOT tested, NOT playtested** — the full
  ctest/canary/window pass is still owed.
