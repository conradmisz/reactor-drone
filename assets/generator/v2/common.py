#!/usr/bin/env python3
"""Shared Pillow helpers for the v2 generators (glow, halos, atlas assembly).

Pure Pillow — no numpy, no scipy. Kept small; each generator imports what it
needs. Coordinate convention here is image-space (top-left origin); the game's
Y-flip happens only in the renderer, so art is authored the natural way.
"""
from __future__ import annotations

import json
import math
import os
from typing import Callable

from PIL import Image, ImageChops, ImageDraw, ImageFilter

IMAGES_DIR = os.path.normpath(
    os.path.join(os.path.dirname(__file__), "..", "..", "images", "v2")
)


def ensure_dirs() -> None:
    os.makedirs(IMAGES_DIR, exist_ok=True)


def radial_glow(size: int, color, inner=1.0, outer=0.0, power=2.0) -> Image.Image:
    """A soft radial gradient disc, RGBA. Alpha falls from `inner` at the centre
    to `outer` at the edge following (1-r)**power. Colour is constant; only alpha
    varies, so it composites cleanly under additive blending."""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    px = img.load()
    c = size / 2.0
    for y in range(size):
        for x in range(size):
            d = math.hypot(x + 0.5 - c, y + 0.5 - c) / c
            if d >= 1.0:
                continue
            a = outer + (inner - outer) * ((1.0 - d) ** power)
            px[x, y] = (color[0], color[1], color[2], int(round(255 * a)))
    return img


def soft_disc(size: int, color, radius_frac=0.42, blur=None) -> Image.Image:
    """A filled disc with a blurred glow halo — the workhorse for neon bodies."""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    c = size / 2.0
    r = size * radius_frac
    d.ellipse([c - r, c - r, c + r, c + r], fill=(color[0], color[1], color[2], 255))
    if blur is None:
        blur = size * 0.06
    return img.filter(ImageFilter.GaussianBlur(blur)) if blur else img


def add_halo(sprite: Image.Image, color, spread=0.18, strength=200) -> Image.Image:
    """Bake an outer glow halo around a sprite's opaque silhouette.
    Returns a new image the same size with the halo composited *under* the art."""
    a = sprite.split()[3]
    halo = Image.new("RGBA", sprite.size, (color[0], color[1], color[2], 0))
    halo.putalpha(a.point(lambda v: strength if v > 20 else 0))
    blur = max(1.0, sprite.size[0] * spread)
    halo = halo.filter(ImageFilter.GaussianBlur(blur))
    # v3 Tier 12: cut the Gaussian's tail so the halo reaches TRUE zero inside
    # the frame. At spread 0.18 on a 512 canvas the blur radius is ~92px, so the
    # tail ran all the way into the corners and was then CUT by the frame edge —
    # a soft glow with a hard rectangular boundary, plus a flat alpha ~8 wash
    # over the whole frame. Drawn at gameplay size that reads as a faint BOX
    # around every entity, which is most of what "everything radiates a square"
    # was pointing at (bugs/004 fixed the particles; this is the sprites).
    # Remap rather than threshold, so the halo keeps its gradient.
    floor = 30
    ha = halo.split()[3].point(
        lambda v: 0 if v <= floor else int((v - floor) * 255 / (255 - floor)))
    halo.putalpha(ha)
    out = Image.alpha_composite(halo, sprite)
    return out


def glow_line(draw: ImageDraw.ImageDraw, p0, p1, color, width):
    draw.line([p0, p1], fill=(color[0], color[1], color[2], 255), width=width)


def emissive_of(img: Image.Image, gamma: float = 1.2) -> Image.Image:
    """v3 Tier 2: the sprite's emissive layer, derived not re-authored.

    Alpha is scaled by per-pixel luminance**gamma, so dark hull pixels vanish
    and the neon lines/halos survive. The result is what the engine's emissive
    render pass feeds the bloom chain — geometry identical to the source, so
    the same sidecar frame rects apply."""
    a = img.split()[3]
    lum = img.convert("L")
    lut = [int(round(255 * ((v / 255.0) ** gamma))) for v in range(256)]
    out = img.copy()
    out.putalpha(ImageChops.multiply(a, lum.point(lut)))
    return out


def build_atlas(frames, columns: int) -> Image.Image:
    """Lay square frames into a grid atlas (row-major). Frames must be equal size."""
    fw, fh = frames[0].size
    rows = math.ceil(len(frames) / columns)
    atlas = Image.new("RGBA", (fw * columns, fh * rows), (0, 0, 0, 0))
    for i, fr in enumerate(frames):
        r, c = divmod(i, columns)
        atlas.paste(fr, (c * fw, r * fh))
    return atlas


def write_sprite(name: str, frames, columns: int, animations: dict) -> None:
    """Write <name>.png atlas + <name>.json sidecar in the engine's documented
    format (atlas / frame_width / frame_height / columns / total_frames / animations)."""
    ensure_dirs()
    fw, fh = frames[0].size
    atlas = build_atlas(frames, columns)
    png = os.path.join(IMAGES_DIR, f"{name}.png")
    atlas.save(png)
    # v3 Tier 2: the `_glow` sibling the emissive render pass probes for.
    emissive_of(atlas).save(os.path.join(IMAGES_DIR, f"{name}_glow.png"))
    sidecar = {
        # Relative to assets/images/ — ResourceManager::load_texture prepends that
        # dir, so an "images/" prefix here resolves to assets/images/images/...
        "atlas": f"v2/{name}.png",
        "frame_width": fw,
        "frame_height": fh,
        "columns": columns,
        "total_frames": len(frames),
        "animations": animations,
    }
    with open(os.path.join(IMAGES_DIR, f"{name}.json"), "w") as f:
        json.dump(sidecar, f, indent=2)
    print(f"  wrote {name}: {len(frames)} frames {fw}x{fh} cols={columns} -> {png}")


def save_png(name: str, img: Image.Image, glow: bool = False) -> None:
    """glow=True also writes the `_glow` emissive sibling (v3 Tier 2) — used by
    props/pickups drawn via Images; backdrops and pure-glow discs skip it."""
    ensure_dirs()
    path = os.path.join(IMAGES_DIR, f"{name}.png")
    img.save(path)
    if glow:
        emissive_of(img).save(os.path.join(IMAGES_DIR, f"{name}_glow.png"))
    print(f"  wrote {name}.png {img.size}")
