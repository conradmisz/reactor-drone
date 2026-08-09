#!/usr/bin/env python3
"""Tileable parallax backdrops + obstacle/hazard sprites, per arena.

Three 512x512 seamlessly-tiling layers per arena (far void+stars, mid machinery
silhouettes, near grid/pipes) plus a pillar (obstacle) and vent (hazard) sprite in
each arena's palette. Seeded RNG so regeneration is deterministic.

Layers wrap by texture size at runtime (BackdropSystem), so every element that
crosses an edge is drawn again on the opposite side to stay seamless.

Run: python make_backdrops.py
"""
from __future__ import annotations

import random
import zlib

from PIL import Image, ImageDraw, ImageFilter

from common import add_halo, save_png
from palette import CORE, FOUNDRY, BIOLAB, PRISM, lerp


def seed_for(name: str) -> int:
    """Stable per-arena RNG seed.

    This used to be hash(name), which Python randomises per process (PYTHONHASHSEED)
    — so "seeded RNG so regeneration is deterministic" was not actually true and a
    re-run silently produced different backdrops. crc32 is stable across processes
    and versions. The committed PNGs predate this fix, so the first regeneration of
    core/foundry/biolab will change them once; every run after that is identical.
    """
    return zlib.crc32(name.encode()) & 0xffffffff

T = 512  # tile size


def wrap_dot(d, x, y, r, col, a):
    """Draw a dot, duplicating across the tile edges so it tiles seamlessly."""
    for ox in (-T, 0, T):
        for oy in (-T, 0, T):
            if -r <= x+ox <= T+r and -r <= y+oy <= T+r:
                d.ellipse([x+ox-r, y+oy-r, x+ox+r, y+oy+r],
                          fill=(col[0], col[1], col[2], a))


def far_layer(pal, rng):
    img = Image.new("RGBA", (T, T), (0, 0, 0, 0))
    # subtle vertical gradient of the clear colour
    top = lerp(pal.clear, pal.primary, 0.06)
    for y in range(T):
        t = y / T
        c = lerp(pal.clear, top, t)
        for x in range(0, T):
            img.putpixel((x, y), (c[0], c[1], c[2], 255))
    d = ImageDraw.Draw(img)
    # stars / distant motes
    for _ in range(140):
        x, y = rng.randint(0, T), rng.randint(0, T)
        r = rng.choice([1, 1, 1, 2])
        b = rng.uniform(0.4, 1.0)
        col = pal.accent if rng.random() < 0.3 else (200, 200, 220)
        wrap_dot(d, x, y, r, col, int(200 * b))
    return img


def mid_layer(pal, rng):
    img = Image.new("RGBA", (T, T), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    # machinery silhouettes: dim rectangles + glowing seams, sparse
    for _ in range(14):
        w, h = rng.randint(40, 130), rng.randint(40, 160)
        x, y = rng.randint(0, T), rng.randint(0, T)
        base = lerp(pal.clear, pal.obstacle, 0.6)
        for ox in (-T, 0, T):
            for oy in (-T, 0, T):
                d.rectangle([x+ox, y+oy, x+ox+w, y+oy+h],
                            fill=(base[0], base[1], base[2], 130))
                # glowing seam line
                d.line([x+ox, y+oy+h//2, x+ox+w, y+oy+h//2],
                       fill=(pal.primary[0], pal.primary[1], pal.primary[2], 90), width=2)
    img = img.filter(ImageFilter.GaussianBlur(1.5))
    return img


def near_layer(pal, rng):
    img = Image.new("RGBA", (T, T), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    # a faint neon grid (divides 512 evenly -> tiles) with occasional bright nodes
    step = 64
    col = pal.primary
    for i in range(0, T, step):
        d.line([i, 0, i, T], fill=(col[0], col[1], col[2], 40), width=1)
        d.line([0, i, T, i], fill=(col[0], col[1], col[2], 40), width=1)
    for gx in range(0, T, step):
        for gy in range(0, T, step):
            if rng.random() < 0.12:
                wrap_dot(d, gx, gy, 3, pal.accent, 120)
    return img


def pillar(pal):
    """Solid obstacle sprite — a glowing pillar/block."""
    s = 96
    img = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    body = lerp(pal.clear, pal.obstacle, 0.85)
    d.rounded_rectangle([10, 6, s-10, s-6], radius=10,
                        fill=(body[0], body[1], body[2], 255),
                        outline=(pal.primary[0], pal.primary[1], pal.primary[2], 255), width=3)
    # inner light strips
    for yy in (28, 48, 68):
        d.line([20, yy, s-20, yy], fill=(pal.accent[0], pal.accent[1], pal.accent[2], 160), width=2)
    return add_halo(img, pal.primary, spread=0.1, strength=120)


def vent(pal):
    """Contact-hazard sprite — a glowing vent/grate."""
    s = 96
    img = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    c = s/2
    d.ellipse([8, 8, s-8, s-8], outline=(pal.hazard[0], pal.hazard[1], pal.hazard[2], 255), width=4)
    # hot core + radiating slits
    d.ellipse([c-18, c-18, c+18, c+18], fill=(pal.hazard[0], pal.hazard[1], pal.hazard[2], 200))
    for k in range(8):
        import math
        a = math.pi*2*k/8
        d.line([c, c, c+math.cos(a)*34, c+math.sin(a)*34],
               fill=(255, 255, 255, 180), width=2)
    return add_halo(img, pal.hazard, spread=0.18, strength=180)


def main():
    print("make_backdrops:")
    for pal in (CORE, FOUNDRY, BIOLAB, PRISM):
        rng = random.Random(seed_for(pal.name))
        save_png(f"bg_{pal.name}_far", far_layer(pal, rng))
        save_png(f"bg_{pal.name}_mid", mid_layer(pal, rng))
        save_png(f"bg_{pal.name}_near", near_layer(pal, rng))
        save_png(f"pillar_{pal.name}", pillar(pal))
        save_png(f"vent_{pal.name}", vent(pal))


if __name__ == "__main__":
    main()
