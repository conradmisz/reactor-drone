# Class-090 Entity Atlas Generator

This directory contains the **procedural entity model and texture-atlas generator** for
Class-090's tower-defense game. It loads CC0 3D models, renders them from a fixed
three-quarter camera into texture **atlases** (a grid of animation/rotation frames) plus a
**sidecar JSON** describing the grid, the animation clips, and — for towers — the on-screen
angle of each rotation facing. The engine then draws those committed atlases through its
ordinary `SpriteSheet` / `Animation` components.

## Do I need to run this?

**No — not to build or play the game.** The committed atlas PNGs + sidecar JSON are the
source of truth; the game reads those directly and never touches the 3D models, Python, or
this tool at runtime. The build does not invoke it and it is not wired into CMake/CTest.

But you are **welcome to explore and re-run it.** It is a small, self-contained example of
how the sprite sheets you consume in the game are actually produced — turning 3D models
into the 2D frame grids the engine animates and rotates. Re-running it only matters if you
want to change the rendered art (different model, pose, camera, colors, or number of
rotation facings).

## How it works

The pipeline is five small stages, one model at a time:

1. **Extract** (`extract_models.py`) — pulls the needed `.glb` meshes out of the committed
   `kenney_tower-defense-kit.zip` into `models/` using Python's `zipfile` (no network, no
   shell). It also extracts the kit's shared `Textures/colormap.png` so the meshes keep
   their colors.
2. **Load + color** (`render/loader.py`) — `trimesh` loads a mesh (combining multi-part
   scenes into one) and computes its bounding box. The Kenney meshes are UV-mapped onto a
   single shared palette image (`colormap.png`); the loader **bakes that palette into
   per-vertex colors** so the offscreen renderer can draw color without uploading a GL
   texture. (This is why the models render in full color rather than white.)
3. **Pose** (`render/poser.py`) — computes a per-frame transform: a vertical bob, a spin
   about the vertical axis, a recoil, a uniform scale, and a sink. Towers
   (`entities/towers.py`) are **modular**: a shared round base stays put while only the
   per-tower weapon is posed.
4. **Render** (`render/scene.py`) — `pyrender` builds an offscreen scene (a fixed
   three-quarter camera + lights, sized to the model) and renders each posed frame to an
   RGBA image with a transparent background.
5. **Pack + describe** (`atlas.py`, `manifest.py`) — `Pillow` packs the frames into a grid
   atlas PNG (frame 0 top-left), and a sidecar JSON is written next to it with the grid
   layout and animation clips. For a tower's **directional** clip (a full 360° spin), the
   generator also bakes a `facing_angles_deg` table — the on-screen angle each frame's
   weapon points — by projecting through the same render camera, so the game can pick the
   frame that aims at a target.

A separate tiny tool, `make_laser_beam.py`, procedurally draws the `laser_beam.png` glow
texture (it is not from the kit) — detailed across the beam's width and uniform along its
length so it stretches cleanly.

### What gets generated

`reference`, four enemies (`enemy_runner` / `fast` / `armored` / `boss`, with march + death
clips), four towers (`tower_cannon` / `laser` / `mortar` / `phaser`, each a 32-facing
directional spin), and two primitives (`projectile_cannonball`, `effect_explosion`). Each
produces a `<name>.png` + `<name>.json` in `../images/`, plus a provenance entry in
`../asset_manifest.json`.

## Asset credits — thank you, Kenney! 🙏

The 3D models come from the wonderful **[Kenney Tower Defense Kit (2.1)](https://kenney.nl/assets/tower-defense-kit)**,
created and shared by **Kenney** at **[kenney.nl](https://kenney.nl)**.

The kit is released under **[Creative Commons Zero (CC0 1.0)](https://creativecommons.org/publicdomain/zero/1.0/)** —
free for personal, **educational**, and commercial use, with attribution appreciated but
not required. We credit Kenney here because the work is genuinely great and worth your
time.

It's a friendly, low-poly tower-defense set: turrets, cannons, catapults, ballistae, tower
bases, UFO enemies, projectiles, tiles, and scenery — all sharing one tidy `colormap.png`
palette. If you want to swap in different art or build your own tower-defense look, browse
Kenney's huge library of free game assets at **[kenney.nl/assets](https://kenney.nl/assets)**
(and consider supporting his work on [Patreon](https://patreon.com/kenney)). The kit is
committed here as `kenney_tower-defense-kit.zip` — unzip it and poke around.

## Setup (only if you want to run it)

```bash
python3 -m venv .venv
source .venv/bin/activate            # Windows: .venv\Scripts\activate
pip install trimesh pyrender pillow numpy pytest
```

`pyrender` renders offscreen and picks a backend automatically (Pyglet/EGL/OSMesa). On a
headless machine you may need EGL or OSMesa — see the
[pyrender offscreen docs](https://pyrender.readthedocs.io/en/latest/examples/offscreen.html).
If no backend is available the golden-image test skips, so the rest of the suite still runs.

## CLI usage

All tuning lives in `generator-parameters.json`; the CLI only selects what to render.

```bash
python3 extract_models.py            # pull the CC0 meshes + colormap out of the kit zip
python3 generate_atlas.py --all      # regenerate every entity's atlas + sidecar + manifest
python3 generate_atlas.py --entity tower_cannon   # just one entity
python3 generate_atlas.py --entity reference --debug   # overlay frame-index labels
python3 make_laser_beam.py           # regenerate the procedural laser-beam texture
```

Running `generate_atlas.py` with neither `--all` nor `--entity` prints usage and exits
non-zero. On success it prints each output atlas + sidecar path.

## Tests

```bash
pytest tests/
```

The generator has its own `pytest` suite (independent of the Catch2 engine/game tests),
including golden-image comparisons that skip cleanly when no offscreen backend is present.

## Python-only

Every script here is Python — no `.sh`/`.bat` anywhere under `2026/Class-090`, and even
unzipping the kit is done through Python's `zipfile`.
