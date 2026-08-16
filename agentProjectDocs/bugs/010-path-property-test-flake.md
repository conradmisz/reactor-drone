---
id: 010
title: Game_Property_Tests fails ~1 run in 20 — find_path returns a path whose first cell is not walkable
status: open
severity: medium
area: build
opened: 2026-08-12
resolved:
---

## Symptom

`ctest` reports `Game_Property_Tests` failed, with two assertions in
`test_enemy_path_properties.cpp` ("find_path steps only through walkable cells on
random obstacle grids"):

```
test_enemy_path_properties.cpp:63: FAILED:
  CHECK( pg.is_walkable(path[i].col, path[i].row) )   -> false
test_enemy_path_properties.cpp:64: FAILED:
  CHECK( grid->get_tile(path[i].col, path[i].row) == TileType::PATH )  -> 0 == 1
```

Re-running the same target usually passes. This makes an otherwise-green gate
report 7/8 at random, which is exactly the kind of noise that gets a real failure
waved through later.

## Reproduce

Intermittent, roughly **5% of runs** (2 failures in 40, measured twice — see Ruled
Out). The test seeds its grid from `GENERATE(take(10, random(1, 100000)))`, so each
process gets ten fresh grids and the failure depends on the draw.

```bash
cd CPP/build/game/tests
for i in $(seq 1 40); do ./game_property_tests "[enemy_path7]" >/tmp/pt.log 2>&1 \
  || { echo "FAILED iter $i"; grep -a "Randomness seeded" /tmp/pt.log; }; done
```

Failing Catch2 seeds observed so far: `2534921327`, `3089104229` (engine-suite
worktree), `1391957818`, `1263390197` (master worktree). Catch2's `--rng-seed`
should make one of these reproducible on demand — **not yet tried**.

## Environment

- Both worktrees: `master` @ 779455e and `engine-suite-build`. **Pre-existing on
  `master`** — it is not an engine-suite regression (see Ruled Out).
- Linux 6.18, GCC, Catch2 v3.5.2, Debug-ish default CMake build.
- The test builds a 12x12 grid at 40 px cells with 8 random 1-cell obstacle boxes
  and clearance 0, then asks `find_path` for 5 random start/goal pairs.

## Ruled Out

**Append-only. Never delete a line from this section.**

- **Tested:** ran `[enemy_path7]` 12x, then 40x, in the `engine-suite-build`
  worktree. **Observed:** 12/12 pass, then 2 failures in 40 (iterations 5 and 39).
  **Eliminates:** "it always fails" and "it never fails" — it is intermittent at
  roughly 5%.
- **Tested:** ran the same target 40x against the **master** worktree's build,
  which has none of the engine-suite code. **Observed:** 2 failures in 40
  (iterations 3 and 16). **Eliminates:** the engine suite as the cause — in
  particular the D146 change that points `enemy_seek.set_arena` at
  `CrumbleSystem::live_obstacles()` instead of the config rows. Same rate, no suite
  code present.
- **Tested:** ran the full `game_property_tests` binary directly after a ctest
  failure. **Observed:** all 32 cases passed (87542 assertions). **Eliminates:**
  "the ctest invocation differs from a direct run" — it is the same binary and the
  same flake, just a different draw.

## Suspects

1. **`find_path` returns a path starting on an unwalkable cell when the START cell
   is itself blocked.** The test picks start/goal uniformly at random and 8 of the
   144 cells are obstacles, so ~5.5% of single picks land inside one — which
   matches the observed failure rate closely enough to be the first thing to test.
   *Test:* construct a grid with a known blocked cell, call `find_path` with
   `start` = that cell, and check whether the returned path is empty (correct) or
   begins on the blocked cell (the bug).
2. **The same for the GOAL cell**, with `find_path` pathing to an unreachable
   blocked cell and returning a best-effort path that ends inside it. *Test:* as
   above with `goal` blocked, and note that the test's own `path.back() == goal`
   assertion did NOT fail, which argues against this one.
3. **Overlapping obstacle boxes make a cell's walkability depend on rasterisation
   order** in `enemy_path::build_obstacle_grid`. *Test:* rasterise two boxes that
   share a cell in both orders and compare the grids.

If suspect 1 holds, the fix is a question of contract, not of code volume: either
`find_path` refuses a blocked start (return empty), or the test is wrong to ask.
Whichever is chosen, the OTHER one gets a comment saying so — an unstated contract
here is what let this sit unnoticed on `master`.

## Resolution

Empty — open. Deliberately not fixed inside the engine-suite branch: it is a
pre-existing `master` flake and fixing engine pathfinding from a feature branch
would put an unrelated change in the merge decision.
