---
id: 003
title: Verification traps — four ways this repo's checks lie to you
status: resolved
severity: high
resolved: 2026-08-13
---

## Summary

Not a game bug. A ledger of four ways a *verification step* in this repo
reported the wrong answer during the 2026-08-12/13 distribution session. Each
one cost 20-60 minutes and each will recur. Three produced **false passes**,
which is the dangerous direction.

## 1. A reverted header does not change an already-built binary

`net_config.hpp` was reverted from `127.0.0.1:8765` back to the workers.dev URL,
but `CPP/build/game/game` was **not rebuilt**. Four bot clients then played real
runs, banked scores locally, printed no error, and reached nothing. The
dashboard sat at zero while everything looked healthy.

**Rule:** after touching `net_config.hpp`, rebuild *and* verify the artifact:

    strings CPP/build/game/game | grep -c '127.0.0.1'    # must be 0

`scripts/verify_branch.sh` section 7 pins this for the Windows exe.

## 2. `strings ... | grep -q` under `set -o pipefail` inverts its result

`grep -q` exits at the first match and closes the pipe; `strings` dies of
SIGPIPE; `pipefail` reports the *pipeline* as failed. So a check that found what
it wanted reported FAILURE. Worse, the same shape would have reported SUCCESS
for the localhost check on any artifact where `strings` produced no output at
all — a false pass on the exact safety gate from trap 1.

**Rule:** dump `strings` to a file once, then `grep -qF` the file. Never pipe
into `grep -q` in a script with `pipefail`.

## 3. `file(GLOB)` without `CONFIGURE_DEPENDS` silently omits new test files

A new `CPP/game/tests/unit/*.cpp` is not compiled until `cmake -B CPP/build -S
CPP` re-runs. The TDD "verify it fails" step therefore *passed* — because the
test was never built. A broken test can masquerade as a passing one indefinitely.

**Rule:** re-run cmake configure after adding any test file, and make the RED
step fail for the *stated reason*, not just non-zero.

## 4. An unregistered save boots into name entry, not the title

With `registered: false` in `saves/meta.json` and networking live, the game
opens `PHASE_NAME_ENTRY`. A scripted probe that assumes the title screen types
its keystrokes into the name field and silently measures nothing — this made a
sim-freeze probe report frames scaling with hold time when nothing was even
paused.

**Rule:** scripted UI runs write a registered `meta.json` first and restore the
original after. See `scripts/drive_ui.py`.

## Ruled out

- Not a determinism problem: the replay canary was byte-identical to the
  pre-session baseline throughout, including across the `tm.*` counter additions.
- Not flakiness in ctest: the one intermittent failure was
  `EngineTimerPropertyTests` under load while two game processes ran, and it
  passed alone. (Consistent with `bugs/` precedent that timing runs here are
  load-sensitive.)
- `scrot` cannot capture the game window on `:1` — it returns all black. Use the
  game's own `--screenshot N` frames instead. Note those stop firing once the
  sim is frozen, because the frame counter stops.

## Resolution

Traps 1, 2 and 4 are now pinned by `scripts/verify_branch.sh` and documented in
`scripts/drive_ui.py`'s module docstring. Trap 3 has no automated guard — it is
a habit, recorded here.
