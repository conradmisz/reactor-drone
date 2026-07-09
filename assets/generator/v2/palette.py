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


CORE = Palette(
    name="core",
    clear=(10, 10, 20),
    primary=(60, 230, 255),     # cyan
    secondary=(255, 70, 200),   # magenta
    accent=(180, 245, 255),     # pale cyan-white
    hazard=(255, 90, 160),
    obstacle=(70, 90, 140),
)

FOUNDRY = Palette(
    name="foundry",
    clear=(20, 10, 6),
    primary=(255, 150, 40),     # orange
    secondary=(255, 210, 70),   # amber
    accent=(255, 230, 160),     # pale amber
    hazard=(255, 120, 30),
    obstacle=(120, 80, 50),
)

BIOLAB = Palette(
    name="biolab",
    clear=(6, 20, 12),
    primary=(80, 255, 140),     # green
    secondary=(180, 120, 255),  # violet
    accent=(200, 255, 210),     # pale green-white
    hazard=(140, 255, 90),
    obstacle=(60, 110, 90),
)

ARENAS = [CORE, FOUNDRY, BIOLAB]
BY_NAME = {p.name: p for p in ARENAS}


def lerp(a: RGB, b: RGB, t: float) -> RGB:
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(3))


if __name__ == "__main__":
    for p in ARENAS:
        print(f"{p.name:8} clear={p.clear} primary={p.primary} "
              f"secondary={p.secondary} accent={p.accent}")
