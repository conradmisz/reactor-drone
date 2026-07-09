# Reactor Drone v2 — Neon Arena Survival

A top-down arena survival shooter for the CS-5850 / hearth-and-hollow (Class-110) engine.
v2 is a total visual overhaul of the original *Reactor Drone*: a neon sci-fi glow art
style, procedural sprite/backdrop/SFX pipeline, a real particle system (trails, thrusters,
explosion bursts), additive glow blending, screen shake + hit feedback, parallax backdrops,
three arenas (Core / Foundry / Bio-lab) with solid obstacles and contact hazards progressed
through by wave, A\* enemy pathfinding around obstacles, and a full audio port.

## Build & run

```
python run.py            # interactive build/test/run menu
```

Requires a C++20 toolchain, CMake, and SDL3 (fetched hermetically). All game assets are
committed; the procedural generators under `assets/generator/v2/` are run offline and never
at build time.

## Controls

Arrows / WASD move · mouse aims · hold to fire · ESC quits.

## Layout

- `CPP/engine/` — the reusable ECS engine (render, particles, pathfinding, audio).
- `CPP/game/`   — Reactor Drone gameplay systems and components.
- `assets/`     — committed sprites/backdrops/audio + the `generator/v2/` pipeline.

## Gates

Zero warnings (`-Wall -Wextra -Wpedantic`, vendored deps exempt), 100% ctest
(`^(Engine|ResourceManager)` and `^Game`), hermetic fresh-clone `python run.py` build,
SDL3-only, Y-flip confined to `RenderSystem::draw_entity()`.

*Not the graded submission — this is an evolved v2 in its own repo.*
