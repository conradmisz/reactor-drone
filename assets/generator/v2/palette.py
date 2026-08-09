#!/usr/bin/env python3
"""Synthwave palettes for Reactor Drone v2 — single source of truth.

Every v2 generator imports from here so the three arenas stay colour-coherent.
Each palette is a small dataclass of RGB tuples. Colours are chosen bright and
saturated so they read well under additive glow blending over a near-black clear.

Run directly to print the palettes as a sanity check.
"""
from __future__ import annotations

from dataclasses import dataclass


RGB = tuple  # (r, g, b), each 0-255


@dataclass(frozen=True)
class Palette:
    name: str
    clear: RGB      # solid clear colour behind the parallax layers (near-black)
    primary: RGB    # main neon (hull glow, player accents)
    secondary: RGB  # complementary neon (enemies, hazards)
    accent: RGB     # highlights, muzzle flashes, projectiles
    hazard: RGB     # contact-hazard tint (vents/lava)
    obstacle: RGB   # pillar/wall body tint
    # v2 Phase 5a: the colour enemies are *tinted* in this arena. Enemy sprites
    # are generated from MONO (below) and coloured at runtime by ArenaDef's
    # enemy_tint, so this is the single source of truth for that JSON value.
    # Chosen complementary to `primary` — an enemy must never be the arena's own
    # neon, which is exactly the bug this phase fixes.
    enemy: RGB = (255, 255, 255)


CORE = Palette(
    name="core",
    clear=(10, 10, 20),
    primary=(60, 230, 255),     # cyan
    secondary=(255, 70, 200),   # magenta
    accent=(180, 245, 255),     # pale cyan-white
    hazard=(255, 90, 160),
    obstacle=(70, 90, 140),
    enemy=(255, 70, 200),       # magenta against the cyan core
)

FOUNDRY = Palette(
    name="foundry",
    clear=(20, 10, 6),
    primary=(255, 150, 40),     # orange
    secondary=(255, 210, 70),   # amber
    accent=(255, 230, 160),     # pale amber
    hazard=(255, 120, 30),
    obstacle=(120, 80, 50),
    enemy=(70, 210, 255),       # cyan against the orange foundry
)

BIOLAB = Palette(
    name="biolab",
    clear=(6, 20, 12),
    primary=(80, 255, 140),     # green
    secondary=(180, 120, 255),  # violet
    accent=(200, 255, 210),     # pale green-white
    hazard=(140, 255, 90),
    obstacle=(60, 110, 90),
    enemy=(190, 120, 255),      # violet against the green bio-lab
)

# v2 Phase 5b — the fourth arena (waves 16-20). Deliberately achromatic:
# its enemies hue-cycle (tie-dye), so a saturated backdrop would fight a sixth of
# every cycle. A violet-white void keeps the rainbow swarm legible throughout.
PRISM = Palette(
    name="prism",
    clear=(14, 10, 22),
    primary=(200, 190, 255),    # pale violet-white
    secondary=(255, 255, 255),
    accent=(240, 240, 255),
    hazard=(255, 80, 120),
    obstacle=(80, 78, 110),
    enemy=(255, 255, 255),      # unused: tick_enemy_tint overwrites it every frame
)

# v2 Phase 5a — not an arena. Enemy sprites are generated against this so the art
# is pure luminance; multiplying it by a saturated hue at runtime (SDL colour-mod)
# reproduces the old look at full richness and makes every other hue work too.
# The relative-value structure lives in make_sprites' scale_col()/pulse() factors,
# so nothing about the shapes has to change.
MONO = Palette(
    name="mono",
    clear=(0, 0, 0),
    primary=(255, 255, 255),
    secondary=(210, 210, 210),
    accent=(255, 255, 255),
    hazard=(255, 255, 255),
    obstacle=(255, 255, 255),
)

ARENAS = [CORE, FOUNDRY, BIOLAB, PRISM]
BY_NAME = {p.name: p for p in ARENAS}


def lerp(a: RGB, b: RGB, t: float) -> RGB:
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(3))


if __name__ == "__main__":
    for p in ARENAS:
        print(f"{p.name:8} clear={p.clear} primary={p.primary} "
              f"secondary={p.secondary} accent={p.accent} enemy={p.enemy}")
