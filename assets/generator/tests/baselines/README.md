# Golden-image baselines

This directory holds the checked-in golden-image baseline(s) consumed by
`tests/test_golden_image.py` (Requirement 11.8).

- `reference.png` — the stitched atlas render of the **reference** entity, produced by
  the full generator pipeline (`params` → `loader` → `scene` → `poser` → `atlas`).
- `enemy_runner.png`, `enemy_fast.png`, `enemy_armored.png`, `enemy_boss.png` — the
  stitched atlas renders of the four **Gen-2 enemy** entities (Runner, Fast, Armored,
  Boss), each produced by the same pipeline. Every enemy atlas contains a looping `march`
  clip followed by a non-looping `death` clip packed contiguously (Requirements 9.7, 9.8).

## How the test uses these

`tests/test_golden_image.py` renders the reference entity and each enemy entity through
the real `pyrender` backend and compares the result against the matching baseline PNG
using the **mean absolute per-pixel difference** (0–255 scale, all RGBA channels). The
comparison passes when that mean difference stays under the test's
`MEAN_ABS_DIFF_TOLERANCE`, which absorbs minor cross-driver anti-aliasing noise while
still catching a real regression (wrong pose, wrong framing, lost transparency, or a
changed frame count/grid).

The tests are **backend-gated**: on a machine with no offscreen GL backend (or with
`pyrender` not installed) they `pytest.skip(...)` instead of failing, so the suite still
passes without a display.

## Regenerating the baselines (instructor, backend-capable machine)

You must run this on a machine that **has** a working `pyrender` offscreen backend
(`pip install pyrender` plus a GL backend — see the generator `README.md`).

Option A — let the tests generate candidates (first time / missing baseline):

```bash
cd 2026/Class-090/assets/generator
# with a baseline absent, that entity's test renders it and skips, telling you the path
python -m pytest tests/test_golden_image.py
```

Option B — deliberately refresh after an intended visual change:

```bash
cd 2026/Class-090/assets/generator
python tests/test_golden_image.py                 # refreshes reference + all four enemies
python tests/test_golden_image.py enemy_boss      # or refresh a single entity by name
```

In both cases, **eyeball the produced PNG** to confirm it looks correct, then commit it.
A baseline should only change when a visual change is intended.
