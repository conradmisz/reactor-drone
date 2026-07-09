#!/usr/bin/env python3
"""Procedurally generate the laser-beam texture (instructor-only asset tool).

The beam is drawn as a rotated, elongated textured quad whose UVs are the full
0,0 -> 1,1. The texture therefore carries all its DETAIL across the beam's WIDTH
(the image's vertical axis) and is UNIFORM along the beam's LENGTH (the image's
horizontal axis), so stretching the quad along its length never distorts the look
and the cross-section glow stays crisp at any beam distance.

Profile across the width: a white-hot core fading through laser-red to a fully
transparent edge (alpha falloff), so the beam reads as a glowing line over the
sprites beneath it. Because the length axis is uniform, the beam can be "pulsed"
purely by animating the quad's thickness (Size.height) or a Color tint at runtime
— no extra texture frames needed.

Output: assets/images/laser_beam.png (RGBA).
Run from anywhere: python make_laser_beam.py
"""

from __future__ import annotations

import math
from pathlib import Path

from PIL import Image

# Image axes: width = along the beam LENGTH (uniform), height = across the beam
# WIDTH (the detailed glow profile). A small length dimension is fine since it is
# uniform and the quad stretches it; a tall height gives a smooth cross-section.
LENGTH_PX = 16
WIDTH_PX = 64

# Core color (white-hot) blended out to the laser color at the edges.
CORE_RGB = (255, 255, 255)
EDGE_RGB = (255, 40, 40)

# Falloff tightness: larger = tighter/brighter core, faster fade to transparent.
ALPHA_FALLOFF = 2.2   # controls the alpha (glow) envelope
CORE_FALLOFF = 3.2    # controls how quickly white core -> red


def _lerp(a: int, b: int, t: float) -> int:
    return int(round(a + (b - a) * t))


def build_beam() -> Image.Image:
    img = Image.new("RGBA", (LENGTH_PX, WIDTH_PX), (0, 0, 0, 0))
    px = img.load()
    center = (WIDTH_PX - 1) / 2.0
    for y in range(WIDTH_PX):
        d = abs(y - center) / center          # 0 at center, 1 at the edge
        glow = math.exp(-((d * ALPHA_FALLOFF) ** 2))   # alpha envelope
        core = math.exp(-((d * CORE_FALLOFF) ** 2))    # whiteness toward center
        r = _lerp(EDGE_RGB[0], CORE_RGB[0], core)
        g = _lerp(EDGE_RGB[1], CORE_RGB[1], core)
        b = _lerp(EDGE_RGB[2], CORE_RGB[2], core)
        a = int(round(255 * glow))
        for x in range(LENGTH_PX):
            px[x, y] = (r, g, b, a)
    return img


def main() -> int:
    out = Path(__file__).resolve().parent.parent / "images" / "laser_beam.png"
    out.parent.mkdir(parents=True, exist_ok=True)
    build_beam().save(out)
    print(f"wrote {out} ({LENGTH_PX}x{WIDTH_PX} RGBA)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
