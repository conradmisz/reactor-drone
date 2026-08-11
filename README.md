# Reactor Drone

A top-down neon arena survival shooter in C++17 and SDL3, built on a hand-rolled ECS
engine. You fly a maintenance drone inside a reactor, mouse-aiming against ring-spawned
waves, banking credits from loot and spending them between waves on permanent upgrades.

Everything the player sees — waves, arenas, enemies, the shop catalogue, menus, colours —
is authored as data in `assets/GameData.json`. Tuning the game needs no rebuild.

**[▶ Watch the walkthrough](https://www.youtube.com/watch?v=1y5MgQmIUT0)**

```
python3 run.py            # interactive build / test / run menu
```

Requires CMake and a C++17 toolchain. SDL3, SDL3_image, SDL3_ttf, nlohmann/json and Lua are
fetched hermetically at configure time. All assets are committed; the procedural generators
under `assets/generator/v2/` run offline and never at build time.

## The run

A 50-wave arc across nine arenas. Four themes — **Core**, **Foundry**, **Bio-lab**,
**Prism** — cycled twice with different obstacle layouts and tougher specialty units on the
second pass, then **Singularity**, a black-hole map the final boss transforms the arena into
mid-fight.

- **Bosses every 10 waves**, themed to the arena they spawn on: the Foundry boss drops
  mines, the Bio-lab boss spits poison. Killing one offers a choice of three active
  abilities on a 30-second cooldown — homing missiles, a four-beam laser that sweeps 360°,
  or a forcefield that auto-fires below 20% hull, healing you and holding enemies out.
- **Specialty units** per theme: poison spitters, mine droppers, armoured bulwarks, and
  Prism splitters that break into two smaller drones when killed.
- **Moon shooters** arrive around wave 3 and escalate — slow shots, then tracking shots,
  then piercing lasers.
- **Two difficulties.** Hard sends more enemies, sooner, faster and tougher, and pays
  better. It never makes your own economy harsher.

## Progression

Credits are spent between waves — an upgrade panel every wave, the full shop every fifth.
Eight upgrade lines (hull, shields, thrusters, fire rate, damage, extra shots, projectile
range, ricochet) stack with an escalating price curve, plus equippable gear and consumables.

Score also accumulates **across runs**. Lifetime score unlocks new ships: at 4,000 points
the Purple Gatling opens up, trading damage per shot for triple the fire rate. Ships are
data — a sprite plus a weapon block — so adding one is a JSON edit.

A run can be saved from the pause menu and resumed later. The save holds run *state* —
wave, credits, hull, gear — not a snapshot of the world, so it stays small and stays valid
across builds.

## Controls

| | |
|---|---|
| Arrows / WASD | Move |
| Mouse | Aim (hold to fire) |
| Space | Thruster dash — damages what you pass through (10 s cooldown) |
| ESC | Pause |
| 1-8 | Buy / equip in the shop |
| TAB / B | Change shop page / launch |

## Engineering

Four gates are kept green permanently:

- **Zero warnings** under `-Wall -Wextra -Wpedantic`. The single allowed exception is Lua's
  vendored `tmpnam` link warning.
- **All tests pass** — 8 ctest suites, unit and property-based, ~24,000 assertions.
- **Hermetic build** from a fresh clone.
- **Deterministic seeded replay.** Two headless runs of the same seed and difficulty print a
  byte-identical summary. Every system that draws from the RNG draws on every path, in a
  fixed order, so a branch can never desynchronise the stream:

```
SDL_VIDEODRIVER=dummy ./CPP/build/game/game --seed 42 --keys 10:SPACE --stopframe 3000
```

The game runs headless with scripted input (`--keys`, `--clicks`, `--screenshot`,
`--stopframe`), which is how features are verified without a window.

## Layout

```
CPP/engine/     ECS core, rendering, particles, UI/menu layer
CPP/game/       Gameplay systems — waves, shop, bosses, items, save
assets/         GameData.json, generated sprites and backdrops
assets/generator/v2/    Offline Pillow scripts that produce the art
```

`ENGINE.md` is the architecture document: layer diagram, measured provenance against the
class baseline, the exact frame order, and the traps that cost real debugging time. It is
updated in the same commit as any engine change.

## Status

Playable start to finish. Balance is provisional — the wave curve is formula-generated and
tuned by eye, not by measurement. There is **no audio**: `assets/Audio/` holds generated
`.wav` files but nothing plays them yet.

Built on the CS-5850 / hearth-and-hollow class engine; `ENGINE.md` §2 records exactly which
files are original to the course baseline and which are new.
