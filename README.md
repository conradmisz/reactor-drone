# Reactor Drone v2 — Neon Arena Survival

A top-down arena survival shooter for the CS-5850 / hearth-and-hollow (Class-110) engine.
v2 is a total visual overhaul of the original *Reactor Drone*: a neon sci-fi glow art
style, procedural sprite/backdrop/SFX pipeline, a real particle system (trails, thrusters,
explosion bursts), additive glow blending, screen shake + hit feedback, parallax backdrops,
four arenas (Core / Foundry / Bio-lab / Prism) with solid obstacles and contact hazards
progressed through by wave, A\* enemy pathfinding around obstacles, and a data-driven
clickable menu layer ported from the Option-040 UI module.

Every fourth cleared wave the run pauses on a **wave-intermission prompt** rather than
dropping you straight into the shop: a pulsing **SHOP OPEN** button and a **NEXT WAVE**
button, and nothing advances until you pick one. Screens and their colours are authored
as data in `assets/GameData.json` (`screens` / `ui_styles`), not in C++.

**There is no audio yet.** `assets/Audio/` holds eight generated `.wav` files, but nothing
plays them — there is no audio system, no mixer and no SDL audio init anywhere in `CPP/`.
Wiring them up is Phase 7. An earlier revision of this file claimed "a full audio port";
that was wrong.

## Build & run

```
python run.py            # interactive build/test/run menu
```

Requires a C++20 toolchain, CMake, and SDL3 (fetched hermetically). All game assets are
committed; the procedural generators under `assets/generator/v2/` are run offline and never
at build time.

## Controls

A run starts on the title menu: click **NORMAL** or **HARD** (or press SPACE for
Normal). Hard sends more enemies, sooner, faster and tougher, and pays better —
it never makes your own economy harsher.

Arrows / WASD move · mouse aims · hold to fire · **ESC pauses**.

At the wave intermission the drone keeps flying, so you can sweep up the credits the last
kill dropped before deciding: click a button, or press **B** for the shop (the same key that
opens and closes it). In the shop, `1`-`8` buy · TAB flips page · B launches.

## Layout

- `CPP/engine/` — the reusable ECS engine (render, particles, pathfinding, UI/menus; no audio).
- `CPP/game/`   — Reactor Drone gameplay systems and components.
- `assets/`     — committed sprites/backdrops + as-yet-unplayed audio, and the
  `generator/v2/` pipeline.
- `ENGINE.md`   — engine architecture, per-file provenance vs the class baseline, and the
  frame order. Kept current with every engine change.

## Gates

Zero warnings (`-Wall -Wextra -Wpedantic`, vendored deps exempt), 100% ctest
(`^(Engine|ResourceManager)` and `^Game`), hermetic fresh-clone `python run.py` build,
SDL3-only, world-space Y-flip confined to `RenderSystem::draw_entity()` (the UI layer is a
separate coordinate space with its own single flip — see `ENGINE.md` §4).

*Not the graded submission — this is an evolved v2 in its own repo.*
