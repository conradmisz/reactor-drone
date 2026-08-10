# Iteration 5 — result

All 13 playtest items plus the 30-wave/prestige design change are **merged into
`master` and gated**. Nothing is pushed. Next free decision id is now **D133**.

The scheduled run in `.handoff-run.sh` never executed — `handoff-run.log` contains
only `Permission denied` (the script was never `chmod +x`'d). It did zero work; this
run started from the clean master baseline.

## What shipped

| Item | Lane | Decision |
|---|---|---|
| #10 Foundry boss "Capital Drone Carrier" | L | D105 |
| — boss adds wear real sprites (found while screenshotting) | L | D106 |
| #11 enemy sprites, #8 player reads as a drone | L | D107 |
| #1 projectiles are red | L | D108 |
| #3 moon shooters fire from the crescent mouth *and* face their target | L | D109 |
| #2 pause menu overlap | M | D113 |
| #4 minimap margins + green health blips | M | D114/D115 |
| #13 active-item slot, bottom-left | M | D116 |
| #5 character/stat overview | M | D117 |
| #6 dash on SPACE, 10 s cooldown | N | D120 |
| #9 mine blast 150→100, destroyable | N | D121/D122 |
| #7 upgrades visibly change the drone | N | D123 |
| #14 30-wave arc + prestige | O | D125-D131 |
| prestige seam fix (integration) | — | D132 |

Burned unused: D110-D112, D118-D119, D124.

## Root causes worth knowing (these were not the bugs they looked like)

- **#10 was not only art.** The boss wore `Images{"v2/enemy_hulk.png"}` and an
  `Images` wearer draws the *whole* texture — `enemy_hulk.png` is a 14-frame atlas,
  so the boss rendered as a literal contact sheet of hexagons.
- **#2 was not a text-fit failure.** `fit_text_in_rect` did its job; Lane K appended
  `pause_save` (y 204..250) onto a column already holding the ESC caption (218..242).
  A latent second bug surfaced with it: `z_order` sorts globally across active
  screens and `gameplay` is always active, so the hull gauge drew *over* the panel.
  The fix that stops a third report is the new no-two-rects-intersect test.
- **#6 was never possible.** The dash hook and the title-start branch are mutually
  exclusive per frame. The one real hole — holding SPACE from the title into the
  first playing frame — is closed by consuming the same one-frame `space_edge`.
- **#3 was two bugs.** Wrong spawn origin *and* enemies carried no `Rotation` at all,
  so the sprite pointed right forever; fixing only the offset would have been correct
  in one direction out of four.
- **The one real cross-lane defect (D132):** Lane O wrote the prestige level as a
  `double` under `"prestige.level"`; Lane M read an `int` from `"meta.prestige_level"`.
  Wrong key *and* wrong type, so the PRESTIGE row silently never appeared. Both lanes'
  gates were green — file ownership meant they never compiled against each other.
  Fixed on the consumer side and pinned by a test.

## Verification that actually ran (on the final merged master)

```
cmake -B CPP/build -S CPP && cmake --build CPP/build -j$(nproc)
  -> 0 warnings, 0 errors (Lua's vendored `tmpnam` is the only warning in the log)

python3 runTestsAll.py
  -> 100% tests passed, 0 tests failed out of 8

rm -f saves/run.json saves/meta.json
SDL_VIDEODRIVER=dummy ./CPP/build/game/game --seed 42 --keys 10:SPACE --stopframe 3000
  Prestige: 0
  Shutting down. Frames: 3000  Final score: 0  Credits: 0  Wave: 1  Phase: 2
  Prestige: 0
  Shutting down. Frames: 3000  Final score: 0  Credits: 0  Wave: 1  Phase: 2   (identical)

printf '{"lifetime_score": 0, "prestige": 3}' > saves/meta.json    # canary at a FIXED level
  Prestige: 3
  Shutting down. Frames: 3000  Final score: 0  Credits: 0  Wave: 1  Phase: 2
  Prestige: 3
  Shutting down. Frames: 3000  Final score: 0  Credits: 0  Wave: 1  Phase: 2   (identical)
```

The canary summary is unchanged from the pre-iteration baseline. `start_run` now
prints `Prestige: N` on its own line, so the comparison target grew rather than moved.
The same gate was re-run on **every** merge commit, not only at the end.

Pixel proof of the D132 fix (BMP reads at a paused frame 100, `dummy` driver): at
prestige 3 a new full-width line appears in the pause panel (670 ink px in x 300..514
where prestige 0 has none) and every stat row below shifts down by exactly 24px with a
byte-identical ink profile. The row renders; I did not OCR its text.

## Not verified — read this before trusting anything above

- **Nothing in iteration 5 has been playtested in a window.** Every claim here is
  headless plus pixel reads. Tests passing is not a playtest.
- **Balance is unmeasured.** The 30-wave ramp, the ~10% endpoint reduction, the boss
  spacing and the three prestige percentages are formula-generated or eyeballed.
- **Nobody has reached wave 30**, so the real victory → prestige flow was proved on a
  temporarily relaxed trigger at game-over, then reverted. The `prestige_offer` panel
  was never screenshotted.
- **Mines and the upgrade plume were never seen in a live run** — the canary dies in
  wave 1 in Core and mines are Foundry-only. Unit tests only.
- Red projectiles were never caught *in flight* in a screenshot (headless has no mouse
  aim, so the drone rarely fires). Death-frame animation and rotor spin at 60fps are
  unchecked in motion, as is how the carrier reads in the Foundry's orange palette.
- The stat sheet's column alignment is space-padding against a proportional font —
  approximate by construction. `minimap.x` is tuned to the pinned 980x660 window.

## Still open (unchanged from HANDOFF, not assigned to any lane)

- `Game_Property_Tests` has a rare pre-existing flake on a negative click coordinate.
- The tiled black hole: `bg_galaxy_mid.png` still reads as a *field* of singularities.
- Waves 16-29 are still waves 1-14 rotated 90° — shrunk, not authored. Still PROVISIONAL.
- No audio at all.

The four lane branches and their worktrees are left in place under
`.claude/worktrees/` if you want to inspect any lane's own history.
