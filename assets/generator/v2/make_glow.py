#!/usr/bin/env python3
"""Additive-blend glow textures: soft radial discs + a muzzle-flash star.

These are authored white so the engine's per-entity Tint can recolour them at
runtime, and they're meant to be drawn with SDL_BLENDMODE_ADD (a Tint{additive}).
Outputs committed to assets/images/v2/.

Run: python make_glow.py
"""
from __future__ import annotations

import math

from PIL import Image, ImageDraw, ImageFilter

from common import radial_glow, save_png


def muzzle_star(size=128, points=4, spike=0.46, core=0.12) -> Image.Image:
    """A 4-point sparkle: a bright core plus long thin spikes, blurred."""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    c = size / 2.0
    # spikes
    for k in range(points):
        ang = math.pi * k / points
        dx, dy = math.cos(ang), math.sin(ang)
        L = size * spike
        w = size * 0.03
        # a thin quad along the spike axis
        d.line([(c - dx * L, c - dy * L), (c + dx * L, c + dy * L)],
               fill=(255, 255, 255, 255), width=max(1, int(w)))
    img = img.filter(ImageFilter.GaussianBlur(size * 0.012))
    # bright core disc
    core_img = radial_glow(size, (255, 255, 255), inner=1.0, outer=0.0, power=2.4)
    core_small = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    r = int(size * (core + 0.18))
    cc = core_img.resize((r, r))
    core_small.paste(cc, (int(c - r / 2), int(c - r / 2)), cc)
    return Image.alpha_composite(img, core_small)


def main():
    print("make_glow:")
    # Soft glow discs at two sizes — the additive halo workhorses.
    save_png("glow_disc_64", radial_glow(64, (255, 255, 255), power=2.2))
    save_png("glow_disc_128", radial_glow(128, (255, 255, 255), power=2.4))
    # A tighter, hotter core for projectile heads.
    save_png("glow_core_32", radial_glow(32, (255, 255, 255), power=3.2))
    # Muzzle-flash star (one-shot on fire).
    save_png("muzzle_star", muzzle_star(128))


if __name__ == "__main__":
    main()
