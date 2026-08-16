# Handoff — the engine feature suite (`engine-suite-build`)

**Purpose of this branch: a merge decision.** Eleven engine-level features from
`specs/engine-feature-suite.md` are implemented here, off `master` @ 779455e.
Nothing is enabled by default, so the branch is safe to sit on indefinitely and
safe to merge as a whole *without* changing how the game plays until a flag is
flipped. This document is what to read before deciding.

Worktree: `../reactor-drone-v2-engine-suite`. Five commits, one per stage.

---

## How to look at it

```bash
cd ../reactor-drone-v2-engine-suite
python run.py -- --suite                       # everything on
python run.py -- --suite --dev --level 8       # ...starting at wave 8, rich money
bash gate.sh .canary-baseline.txt              # the whole gate, incl. the canary
```

`--suite` flips every feature at once. It also *authors* content for the two lanes
that are inert by data rather than by a flag: obstacle HP (240, ~12 shots) so
pillars can be destroyed, a three-row surge table per arena, and
`reactor_bloom` as the boss's bullet pattern. Individually, each feature is a
`GameData.json` flag — the decision does not have to be all-or-nothing.

---

## What shipped, per lane

| # | Lane | Feature | Flag | Stance |
|---|---|---|---|---|
| 1 | P | Temporal Overload (bullet time) | `timescale.enabled` | sim-side |
| 8 | Q | Adaptive Director | `director.enabled` | sim-side |
| RG | R | Resonance Grid | `resonance.enabled` | render-only |
| 10 | S | Flight Report | `flight_report.enabled` | passive |
| 3 | T | Force-Field Layer | *(inert by shape)* | sim-side |
| ~~6~~ | ~~V~~ | ~~Battle-Scar Layer~~ | **CUT (D151)** | — |
| 9 | U | Destructible Arena | `ObstacleDef::hp` | sim-side |
| 5 | W | Palette Engine | `palettes.enabled` | render-only |
| 2 | Y | Bullet-Pattern Language | `EnemyType::pattern` | sim-side |
| 7 | X | Reactor Surge Events | `ArenaDef::surges` | sim-side |
| 4 | Z | Chip-Synth Audio | **SHELVED (D151)** — builds, unwired | isolated |

Design reasoning, the rejected alternatives and every trap are in
**`specs/engine-feature-suite.md` → Merge Notes**, D138-D151 (one entry per lane
plus the playtest batch). They live there rather than in `decisions.md` because
`ai-workflow-rules.md` forbids a feature branch from writing the shared,
append-heavy docs — at merge time they move across. The frame-order table and the
hook contract are in `ENGINE.md` §6b/§6c.

---

## Verification — what actually ran

**Ran, repeatedly, and green:**

- Clean build under `-Wall -Wextra -Wpedantic`; the only warning is Lua's vendored
  `tmpnam`.
- `ctest` 8/8. ~50 new unit cases across ten new test files.
- **Replay canary byte-identical twice AND identical to the pre-suite `master`
  baseline** (`--seed 42 --keys 5:SPACE --stopframe 3000`). That is the proof the
  suite is genuinely inert by default, not merely self-consistent — the baseline
  line is committed as `.canary-baseline.txt`.
- **`--suite` runs are themselves deterministic**: identical summaries across two
  runs at seed 42/3000 frames, and at `--dev --level 8 --seed 7`/4000 frames.
- Headless captures read back and compared: the resonance grid draws over the
  backdrop and under the entities; the palette resolve measurably recolours the
  frame (mean RGB 44/31/21 → 43/23/13 on a wave-6 capture) while leaving the
  enemy-cyan and hazard-orange colour language intact.

**One playtest done (2026-08-12).** It produced D151: the grid was undersized and
cluttered (revised), the scar layer made no sense in a arena that floats in space
(cut), and the audio is shelved. **Everything else is still unplayed** — nobody has
seen the revised grid, watched a pillar come down, read a flight report, met a
surge or a bullet pattern, or triggered bullet time. Every tuning number is
provisional in the strict sense the project uses.

**Known noise:** `Game_Property_Tests` fails about 1 run in 20 on a **pre-existing
`master` flake** — measured at the same rate in both worktrees and filed as
`bugs/010-path-property-test-flake.md`. It is not a suite regression. Re-run before
believing a red ctest line.

---

## What to judge, feature by feature

1. **Bullet time** — does the kill-chain beat read as impact, or as lag? It is the
   one feature that changes the *feel* of every other one.
2. **Resonance grid (revised, D151)** — it now covers the whole arena, is
   invisible at rest, and only a bomb, a collapsing pillar or a boss volley rings
   it. Does a blast read as a shockwave through the lattice, or is it still
   clutter? Knobs: `resonance.spacing` (64), `max_offset` (26), `a` (190, the PEAK
   alpha).
3. **Adaptive Director** — is it perceptible at all? It is *meant* not to be. If it
   is noticeable, `min_mult`/`max_mult` want narrowing.
4. **Flight report** — legible? Does it fit its 300-unit square on the game-over
   panel without fighting the buttons?
6. **Palette** — the duotone resolve is a tone map, not a true index remap (see
   D147 for why SDL3 leaves no better option). Check it does not break the rule
   that red always means "this hurts you" (D136). `LIGHT_GAIN` in
   `palette_system.cpp` is the brightness knob.
7. **Destructible cover** — is 240 HP right, and does losing cover read as a
   consequence or an annoyance?
8. **Surges** — is 1.5–1.8 s of telegraph enough warning? The sweeping arc is the
   one most likely to feel unfair.
9. **Bullet patterns** — is `reactor_bloom` dodgeable at the boss's size? A ring of
   14 at 190 px/s is a guess.
9. **Audio** — shelved (D151). Nothing to judge until it is re-wired.

---

## Deliberately not done

- **`items::repulse_enemies` was NOT folded into the force layer**, though the spec
  asked for it. That helper pushes positions after the arena clamp; the force layer
  writes velocities before movement. Converting it would retune a shipped item's
  feel without a playtest, and mixing that into a merge decision about eleven new
  features makes the decision harder, not easier. It is a one-lane follow-up.
- **No cracked-obstacle sprite variants.** Damage stages are a darkening multiply;
  the generator work is flagged with a `ponytail:` comment at the site.
- **The SFX note table is code, not data** — 1–3 note blips, where a JSON table
  would be more surface area than the sounds. The comment names where it moves.
- **One playtest, and no balance pass.** Every number is still provisional.
- **The battle-scar layer was cut and the audio shelved** after that playtest
  (D151). The audio code still builds; the scar layer is gone from the tree.
- The eight committed WAVs under `assets/Audio/` are still unused by anything; the
  chip synth replaces them rather than playing them.

---

## Where this stands, and how to pick it up

Last session ended 2026-08-13, after the first playtest and the D151 batch it
produced. Nothing is in flight; the branch is committed and the gate is green.

**The one thing owed: re-play `--suite`.** Four of the five D151 items shipped
unplayed, and the grid revision in particular is a visual change only a window can
judge. In order of what is least verified:

1. **The revised resonance grid** — it now covers the arena, is invisible at rest,
   and only a bomb, a collapsing pillar or a boss volley rings it. Does a blast
   read as a shockwave, or is it still clutter? (`--dev --level 8` reaches mines
   and pillars fast; the boss is wave 10/20/30.)
2. **Temporal Overload** — never triggered in the first playtest. `--dev --level
   12` is dense enough to chain three kills almost by accident.
3. **Flight report** — reachable by simply dying.
4. **Surges** (waves 2/4/6), **bullet patterns** (the boss), **destructible
   cover** (240 HP, ~12 shots).

```bash
cd ../reactor-drone-v2-engine-suite
python run.py -- --suite --dev --level 8
bash gate.sh .canary-baseline.txt        # build + ctest + canary vs pre-suite master
```

**Environment notes from that session, so they are not rediscovered:**

- A Remote Control session named `engine-suite` was left running in a terminal on
  this worktree (`claude --remote-control "engine-suite"`), so the branch can be
  driven from the Claude phone app. It may or may not still be alive.
- A cloud session was created in error and never used —
  `session_01VnpiFdZS2xk37X1rqH1bLU`. There is no CLI to delete it; stop it from
  claude.ai/code if it is still listed.
- `--cloud` requires a TTY, so it cannot be started from inside a Claude Code Bash
  tool without a pty; `--remote-control` has no such restriction.

## If the answer is "merge"

1. Play `--suite` first and decide flag by flag; merging with everything off is
   also a legitimate outcome (it lands eleven dormant features and the hook
   scaffolding, and costs nothing at runtime).
2. **Move the Merge Notes across**: the decisions into `decisions.md`, the state
   into `progress-tracker.md`, a line per kept feature into `project-overview.md`,
   then delete the section. The branch deliberately leaves all four of those files
   byte-identical to `master`.
3. Re-run the provenance sweep in `ENGINE.md` §2 — the suite adds three engine
   file pairs and the counts in that section are now stale by design.
4. `bugs/003` should be fixed on `master`, not here.
5. The suite reserved decision ids D138–D180 and spent D138–D151; D152–D180 are
   burned, so the next free id after a merge is still **D194**.

## If the answer is "not yet"

The branch is inert by construction — it can sit. The only maintenance cost is that
`main.cpp`'s hook blocks and `arena_config.hpp` will drift against `master`, so a
long wait means a real rebase rather than a fast-forward.
