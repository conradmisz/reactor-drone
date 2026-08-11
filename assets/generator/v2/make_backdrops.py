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
import sys
import zlib

from PIL import Image, ImageDraw, ImageFilter

from common import add_halo, save_png
from palette import ARENAS, lerp


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


def tile_blur(img, radius):
    """Gaussian blur that stays seamless: blur a 3x3 tiling and take the centre.

    A plain GaussianBlur samples transparent black past the edge, which leaves a
    visible seam once the texture wraps. Blurring the tiled copy means every edge
    pixel sees its true wrapped neighbours.
    """
    big = Image.new("RGBA", (T * 3, T * 3), (0, 0, 0, 0))
    for ox in range(3):
        for oy in range(3):
            big.paste(img, (ox * T, oy * T))
    return big.filter(ImageFilter.GaussianBlur(radius)).crop((T, T, T * 2, T * 2))


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
    # stars / distant motes — v3 Tier 0: sparser and dimmer. The field is a hint
    # of depth, not a subject; the Laser Hockey read needs a near-empty ground so
    # the neon foreground owns the frame.
    for _ in range(55):
        x, y = rng.randint(0, T), rng.randint(0, T)
        r = rng.choice([1, 1, 1, 2])
        b = rng.uniform(0.4, 1.0)
        col = pal.accent if rng.random() < 0.3 else (200, 200, 220)
        wrap_dot(d, x, y, r, col, int(120 * b))
    return img


def mid_layer(pal, rng):
    img = Image.new("RGBA", (T, T), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    # machinery silhouettes: dim rectangles + glowing seams — v3 Tier 0: far
    # fewer and far darker. At 14 rects / alpha 130 this layer competed with the
    # enemies for visual density; it is now a whisper of structure.
    for _ in range(5):
        w, h = rng.randint(40, 130), rng.randint(40, 160)
        x, y = rng.randint(0, T), rng.randint(0, T)
        base = lerp(pal.clear, pal.obstacle, 0.6)
        for ox in (-T, 0, T):
            for oy in (-T, 0, T):
                d.rectangle([x+ox, y+oy, x+ox+w, y+oy+h],
                            fill=(base[0], base[1], base[2], 55))
                # glowing seam line
                d.line([x+ox, y+oy+h//2, x+ox+w, y+oy+h//2],
                       fill=(pal.primary[0], pal.primary[1], pal.primary[2], 45), width=2)
    img = img.filter(ImageFilter.GaussianBlur(1.5))
    return img


def near_layer(pal, rng):
    img = Image.new("RGBA", (T, T), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    # a faint neon grid (divides 512 evenly -> tiles) with occasional bright nodes
    # v3 Tier 0: wider cells, fainter lines, rarer nodes — one crisp line reads
    # better than many faint ones.
    step = 128
    col = pal.primary
    for i in range(0, T, step):
        d.line([i, 0, i, T], fill=(col[0], col[1], col[2], 26), width=1)
        d.line([0, i, T, i], fill=(col[0], col[1], col[2], 26), width=1)
    for gx in range(0, T, step):
        for gy in range(0, T, step):
            if rng.random() < 0.08:
                wrap_dot(d, gx, gy, 3, pal.accent, 90)
    return img


# --- galaxy: the wave-50 arena ----------------------------------------------
# Same three-layer contract as every other theme, but the layers carry different
# imagery: nebula + dense starfield instead of a flat void, an accretion disc
# instead of machinery, and dust motes over a dimmed grid instead of neon pipes.
# Every element is drawn wrap-duplicated or blurred with tile_blur, so all three
# still tile seamlessly.

def galaxy_far(pal, rng):
    img = Image.new("RGBA", (T, T), (255, 255, 255, 0))
    # nebula: fat soft blobs of purple/magenta over the near-black clear
    neb = Image.new("RGBA", (T, T), (0, 0, 0, 0))
    dn = ImageDraw.Draw(neb)
    for _ in range(16):
        x, y = rng.randint(0, T), rng.randint(0, T)
        r = rng.randint(50, 155)
        col = pal.primary if rng.random() < 0.55 else pal.secondary
        wrap_dot(dn, x, y, r, col, rng.randint(45, 95))
    neb = tile_blur(neb, 34)
    base = Image.new("RGBA", (T, T), (pal.clear[0], pal.clear[1], pal.clear[2], 255))
    img = Image.alpha_composite(base, neb)
    # Stars go on their own transparent layer and are composited: ImageDraw
    # *overwrites* RGBA rather than blending, so drawing a low-alpha halo straight
    # onto the opaque base would punch a hole in it instead of glowing.
    halos = Image.new("RGBA", (T, T), (0, 0, 0, 0))
    stars = Image.new("RGBA", (T, T), (0, 0, 0, 0))
    dh, ds = ImageDraw.Draw(halos), ImageDraw.Draw(stars)
    for _ in range(26):                      # bright anchors with a soft halo
        x, y = rng.randint(0, T), rng.randint(0, T)
        wrap_dot(dh, x, y, rng.randint(6, 10), pal.accent, 70)
        wrap_dot(ds, x, y, 2, (255, 255, 255), 255)
    for _ in range(640):                     # the field itself
        b = rng.uniform(0.2, 1.0)
        col = pal.accent if rng.random() < 0.4 else (255, 255, 255)
        wrap_dot(ds, rng.randint(0, T), rng.randint(0, T),
                 rng.choice([1, 1, 1, 1, 1, 1, 2]), col, int(235 * b))
    img = Image.alpha_composite(img, tile_blur(halos, 4))
    return Image.alpha_composite(img, stars)


def galaxy_mid(pal, rng):
    """The black hole: an event horizon that occludes the starfield behind it,
    ringed by a hot accretion disc. Centred so the tile repeat reads as a field
    of distant singularities rather than a cut-off shape."""
    img = Image.new("RGBA", (T, T), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    c = T / 2
    # accretion disc: filled ellipses from a dim magenta rim inward to a white-hot
    # core, painted outer-first so each smaller one overwrites the last.
    for i in range(26):
        t = i / 25.0                      # 0 = outermost, 1 = innermost
        rx = 236 - i * 4.6
        col = lerp(pal.secondary, pal.accent, t ** 1.4)
        d.ellipse([c - rx, c - rx * 0.30, c + rx, c + rx * 0.30],
                  fill=(col[0], col[1], col[2], int(35 + 200 * t)))
    img = tile_blur(img, 7)
    # outer purple bloom off the disc
    bloom = Image.new("RGBA", (T, T), (0, 0, 0, 0))
    ImageDraw.Draw(bloom).ellipse([c - 240, c - 78, c + 240, c + 78],
                                  fill=(pal.primary[0], pal.primary[1],
                                        pal.primary[2], 110))
    img = Image.alpha_composite(tile_blur(bloom, 30), img)
    d = ImageDraw.Draw(img)
    # event horizon — opaque black, kills everything the far layer drew behind it
    d.ellipse([c - 96, c - 96, c + 96, c + 96], fill=(0, 0, 0, 255))
    # photon ring hugging the horizon
    d.ellipse([c - 98, c - 98, c + 98, c + 98],
              outline=(255, 255, 255, 230), width=3)
    d.ellipse([c - 104, c - 104, c + 104, c + 104],
              outline=(pal.primary[0], pal.primary[1], pal.primary[2], 150), width=6)
    return img


def galaxy_near(pal, rng):
    """Foreground dust. The grid is kept (theme family) but dimmed hard so the
    nebula behind it stays the subject; bright drifting motes carry the layer."""
    img = Image.new("RGBA", (T, T), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    step = 128
    col = pal.primary
    for i in range(0, T, step):
        d.line([i, 0, i, T], fill=(col[0], col[1], col[2], 22), width=1)
        d.line([0, i, T, i], fill=(col[0], col[1], col[2], 22), width=1)
    for _ in range(46):
        x, y = rng.randint(0, T), rng.randint(0, T)
        c2 = pal.accent if rng.random() < 0.5 else pal.secondary
        wrap_dot(d, x, y, rng.choice([2, 2, 3, 4]), c2, rng.randint(90, 190))
    return img


LAYERS = {"galaxy": (galaxy_far, galaxy_mid, galaxy_near)}


# ---------------------------------------------------------------------------
# Arena props — walls, obstacles (pillar_*), hazards (vent_*). One bespoke
# shape per theme (D136); the old shared rounded-square/circle pair read as
# the same arena re-tinted five times.
#
# Authored at 4x supersample in a 384px space, BOX-downsampled to 96 (the
# same pattern as make_sprites.py). No RNG anywhere — regeneration is
# byte-stable without seeding.
#
# Constraints the shapes respect (from the engine, see D136):
#  - WALLS: drawn as 97 unrotated-until-D136 segments on a 90-unit ring
#    spacing at 110px, so each segment's left/right ~26px (of 96) is plain
#    full-width banding and all decoration stays inside — the 20-unit overlap
#    then lands on identical pixels and the seam disappears. Segments are
#    rotated to the ring tangent at spawn; art convention: OUTER edge at the
#    top of the image, glowing INNER face at the bottom.
#  - OBSTACLES: the same PNG is stretched to every layout AABB (Foundry uses
#    1:3.6 bars), so each shape must survive strong non-uniform stretch.
#  - HAZARDS: the engine force-tints them additive red (main.cpp Tint), so
#    themes differentiate by silhouette and luminance, not colour.

import math

SS = 4
PS = 96 * SS  # supersampled prop canvas


def _prop_canvas():
    img = Image.new("RGBA", (PS, PS), (0, 0, 0, 0))
    return img, ImageDraw.Draw(img)


def _shrink(img):
    return img.resize((96, 96), Image.BOX)


def _c(col, a=255):
    return (col[0], col[1], col[2], a)


# --- walls -----------------------------------------------------------------

def wall_core(p):
    """Riveted reactor plating with a cyan conduit along the inner face."""
    img, d = _prop_canvas()
    body = lerp(p.clear, p.obstacle, 0.8)
    d.rectangle([0, 40, PS, PS - 56], fill=_c(body))
    d.rectangle([0, 40, PS, 100], fill=_c(lerp(p.clear, p.obstacle, 0.5)))
    d.rectangle([120, 130, PS - 120, PS - 120], outline=_c(p.obstacle), width=6)
    for x in (150, PS // 2, PS - 150):
        d.ellipse([x - 14, 58, x + 14, 86], fill=_c(lerp(p.obstacle, p.primary, 0.35)))
    d.line([0, PS - 76, PS, PS - 76], fill=_c(p.primary), width=10)
    d.line([0, PS - 76, PS, PS - 76], fill=_c(p.accent, 180), width=4)
    return add_halo(_shrink(img), p.primary, spread=0.1, strength=120)


def wall_foundry(p):
    """Slag-crusted furnace brick courses with ember seams and a molten lip."""
    img, d = _prop_canvas()
    d.rectangle([0, 36, PS, PS - 52], fill=_c(lerp(p.clear, p.obstacle, 0.85)))
    rows = [(36, 120), (128, 220), (228, PS - 52)]
    joint = _c(lerp(p.primary, p.clear, 0.55), 220)
    for i, (y0, y1) in enumerate(rows):
        d.rectangle([0, y0, PS, y1], fill=_c(lerp(p.clear, p.obstacle, 0.65)))
        for x in ((170, 250) if i % 2 else (120, 200, 280)):  # joints, edge-safe
            d.line([x, y0, x, y1], fill=joint, width=5)
    d.line([0, 124, PS, 124], fill=_c(p.primary, 200), width=6)
    d.line([0, 224, PS, 224], fill=_c(p.secondary, 160), width=4)
    d.line([0, PS - 64, PS, PS - 64], fill=_c(p.primary), width=12)
    d.line([0, PS - 64, PS, PS - 64], fill=_c(p.accent, 200), width=4)
    return add_halo(_shrink(img), p.primary, spread=0.1, strength=130)


def wall_biolab(p):
    """Containment glass between struts, specimen cultures drifting inside."""
    img, d = _prop_canvas()
    d.rectangle([0, 44, PS, PS - 60], fill=_c(lerp(p.clear, p.obstacle, 0.55)))
    d.rectangle([0, 76, PS, PS - 92], fill=_c(lerp(p.clear, p.primary, 0.22), 235))
    d.rectangle([0, 76, PS, 88], fill=_c(p.obstacle))
    d.rectangle([0, PS - 100, PS, PS - 92], fill=_c(p.obstacle))
    for k, (x, y, r) in enumerate([(130, 168, 22), (196, 150, 11), (250, 182, 16), (150, 210, 8)]):
        d.ellipse([x - r, y - r, x + r, y + r], fill=_c(p.primary, 80 + k * 25))
    d.line([0, PS - 72, PS, PS - 72], fill=_c(p.primary, 230), width=8)
    return add_halo(_shrink(img), p.primary, spread=0.1, strength=120)


def wall_prism(p):
    """Fused crystal battlement — irregular facets rising from a solid base."""
    img, d = _prop_canvas()
    d.rectangle([0, 150, PS, PS - 56], fill=_c(lerp(p.clear, p.obstacle, 0.8)))
    pts = [(0, 150), (110, 70), (160, 128), (216, 52), (268, 132), (PS, 150)]
    d.polygon(pts + [(PS, 170), (0, 170)], fill=_c(lerp(p.clear, p.primary, 0.35)))
    d.line(pts, fill=_c(p.primary), width=6)
    for x, y in (pts[1], pts[3]):
        d.line([x, y, x, 150], fill=_c(p.accent, 140), width=4)
    d.line([0, PS - 68, PS, PS - 68], fill=_c(p.primary, 220), width=8)
    return add_halo(_shrink(img), p.primary, spread=0.14, strength=130)


def wall_galaxy(p):
    """Obsidian event-horizon shards leaning over a hazard-lit rail."""
    img, d = _prop_canvas()
    d.rectangle([0, 170, PS, PS - 60], fill=_c(lerp(p.clear, p.obstacle, 0.7)))
    for k in range(3):
        x = 116 + k * 78
        h = 60 + (k * 37 % 70)
        d.polygon([(x, 170), (x + 30, 170), (x + 26, 170 - h), (x + 8, 170 - h + 14)],
                  fill=_c(lerp(p.clear, p.obstacle, 0.95)),
                  outline=_c(p.hazard, 200), width=3)
    d.line([0, PS - 72, PS, PS - 72], fill=_c(p.hazard, 220), width=8)
    return add_halo(_shrink(img), p.hazard, spread=0.14, strength=130)


# --- obstacles (pillar_*) --------------------------------------------------

def pillar_core(p):
    """Cooling stack: glowing core dome, corner bolts."""
    img, d = _prop_canvas()
    d.rounded_rectangle([24, 24, PS - 24, PS - 24], radius=36,
                        fill=_c(lerp(p.clear, p.obstacle, 0.85)),
                        outline=_c(p.primary), width=8)
    cx = PS / 2
    d.ellipse([cx - 34, cx - 34, cx + 34, cx + 34], fill=_c(p.primary, 200))
    d.ellipse([cx - 16, cx - 16, cx + 16, cx + 16], fill=_c(p.accent, 230))
    for x, y in [(52, 52), (PS - 52, 52), (52, PS - 52), (PS - 52, PS - 52)]:
        d.ellipse([x - 12, y - 12, x + 12, y + 12],
                  fill=_c(lerp(p.obstacle, p.primary, 0.4)))
    return add_halo(_shrink(img), p.primary, spread=0.1, strength=120)


def pillar_foundry(p):
    """Riveted girder with an X-brace — stretches into a beam either way."""
    img, d = _prop_canvas()
    d.rectangle([16, 16, PS - 16, PS - 16], fill=_c(lerp(p.clear, p.obstacle, 0.85)),
                outline=_c(p.primary), width=8)
    brace = _c(lerp(p.obstacle, p.primary, 0.5))
    d.line([16, 16, PS - 16, PS - 16], fill=brace, width=12)
    d.line([PS - 16, 16, 16, PS - 16], fill=brace, width=12)
    d.rectangle([16, 16, PS - 16, 40], fill=_c(lerp(p.clear, p.obstacle, 0.55)))
    d.rectangle([16, PS - 40, PS - 16, PS - 16], fill=_c(lerp(p.clear, p.obstacle, 0.55)))
    for t in (0.18, 0.5, 0.82):
        x = 16 + t * (PS - 32)
        for y in (28, PS - 28):
            d.ellipse([x - 10, y - 10, x + 10, y + 10], fill=_c(p.secondary, 220))
    return add_halo(_shrink(img), p.primary, spread=0.1, strength=120)


def pillar_biolab(p):
    """Specimen tank: domed glass on a machine base, culture bubbling inside."""
    img, d = _prop_canvas()
    d.rounded_rectangle([40, 26, PS - 40, PS - 26], radius=70,
                        fill=_c(lerp(p.clear, p.primary, 0.18), 240),
                        outline=_c(p.obstacle), width=10)
    d.rectangle([28, PS - 70, PS - 28, PS - 26], fill=_c(lerp(p.clear, p.obstacle, 0.9)))
    d.rectangle([28, 26, PS - 28, 64], fill=_c(lerp(p.clear, p.obstacle, 0.9)))
    for k, (x, y, r) in enumerate([(150, 240, 26), (230, 170, 18), (120, 150, 14), (210, 280, 12)]):
        d.ellipse([x - r, y - r, x + r, y + r], fill=_c(p.primary, 120 + k * 20))
    d.line([64, 90, 64, PS - 84], fill=_c(p.accent, 90), width=10)
    return add_halo(_shrink(img), p.primary, spread=0.1, strength=120)


def pillar_prism(p):
    """Crystal cluster: three leaning shards on a base plate."""
    img, d = _prop_canvas()
    shards = [
        [(90, PS - 30), (150, PS - 30), (128, 60), (100, 96)],
        [(170, PS - 30), (250, PS - 30), (300, 110), (210, 70)],
        [(40, PS - 30), (90, PS - 30), (60, 150)],
    ]
    for k, pts in enumerate(shards):
        d.polygon(pts, fill=_c(lerp(p.clear, p.primary, 0.30 + k * 0.12), 245),
                  outline=_c(p.primary), width=6)
    d.line([128, 60, 118, PS - 30], fill=_c(p.accent, 150), width=5)
    d.line([210, 70, 226, PS - 30], fill=_c(p.accent, 120), width=5)
    d.rectangle([30, PS - 44, PS - 30, PS - 24], fill=_c(lerp(p.clear, p.obstacle, 0.9)))
    return add_halo(_shrink(img), p.primary, spread=0.14, strength=120)


def pillar_galaxy(p):
    """Cracked monolith, a glowing fissure running its height."""
    img, d = _prop_canvas()
    d.polygon([(70, PS - 26), (PS - 70, PS - 26), (PS - 96, 40), (96, 56)],
              fill=_c(lerp(p.clear, p.obstacle, 0.95)),
              outline=_c(p.hazard, 220), width=6)
    crack = [(PS/2 - 20, 60), (PS/2 + 10, 140), (PS/2 - 26, 210), (PS/2 + 16, PS - 30)]
    d.line(crack, fill=_c(p.hazard), width=10)
    d.line(crack, fill=_c(p.accent, 170), width=4)
    return add_halo(_shrink(img), p.hazard, spread=0.14, strength=120)


# --- hazards (vent_*) — shape-first; the engine tints them red -------------

def vent_core(p):
    """Cracked coolant breach: jagged star with a hot core."""
    img, d = _prop_canvas()
    cx = cy = PS / 2
    pts = []
    for k in range(12):
        a = math.tau * k / 12
        r = 150 if k % 2 == 0 else 88
        pts.append((cx + math.cos(a) * r, cy + math.sin(a) * r))
    d.polygon(pts, fill=_c(p.hazard, 120), outline=_c(p.hazard), width=8)
    d.ellipse([cx - 44, cy - 44, cx + 44, cy + 44], fill=(255, 255, 255, 220))
    for k in range(4):
        a = math.tau * k / 4 + 0.4
        d.line([cx + math.cos(a) * 40, cy + math.sin(a) * 40,
                cx + math.cos(a) * 178, cy + math.sin(a) * 178],
               fill=(255, 255, 255, 190), width=8)
    return add_halo(_shrink(img), p.hazard, spread=0.18, strength=180)


def vent_foundry(p):
    """Lava grate: bright melt channels behind dark bars."""
    img, d = _prop_canvas()
    d.rounded_rectangle([20, 20, PS - 20, PS - 20], radius=24, fill=_c(p.hazard, 170))
    for k in range(1, 4):
        y = 20 + k * (PS - 40) / 4
        d.line([32, y, PS - 32, y], fill=(255, 255, 255, 230), width=14)
    for k in range(5):
        x = 20 + k * (PS - 40) / 4
        d.line([x, 14, x, PS - 14], fill=_c(lerp(p.clear, p.obstacle, 0.9)), width=18)
    d.rounded_rectangle([20, 20, PS - 20, PS - 20], radius=24,
                        outline=_c(p.hazard), width=10)
    return add_halo(_shrink(img), p.hazard, spread=0.18, strength=180)


def vent_biolab(p):
    """Acid pool: irregular blob with drifting bubbles."""
    img, d = _prop_canvas()
    cx = cy = PS / 2
    rim = [(cx + math.cos(math.tau * k / 16) * (130 + 34 * math.sin(k * 2.3)),
            cy + math.sin(math.tau * k / 16) * (130 + 34 * math.sin(k * 2.3)))
           for k in range(16)]
    d.polygon(rim, fill=_c(p.hazard, 150), outline=_c(p.hazard), width=8)
    inner = [(cx + math.cos(math.tau * k / 16) * (60 + 20 * math.sin(k * 2.3)),
              cy + math.sin(math.tau * k / 16) * (60 + 20 * math.sin(k * 2.3)))
             for k in range(16)]
    d.polygon(inner, fill=(255, 255, 255, 150))
    for x, y, r in [(120, 120, 16), (250, 150, 12), (170, 260, 18), (260, 250, 9)]:
        d.ellipse([x - r, y - r, x + r, y + r], outline=(255, 255, 255, 200), width=6)
    return add_halo(_shrink(img), p.hazard, spread=0.18, strength=180)


def vent_prism(p):
    """Shard spikes: a bed of bright needles on a hot base."""
    img, d = _prop_canvas()
    for k in range(7):
        x = 30 + k * 52
        h = 120 + (k * 53 % 90)
        d.polygon([(x, PS - 40), (x + 44, PS - 40), (x + 22, PS - 40 - h)],
                  fill=_c(p.hazard, 200), outline=(255, 255, 255, 220), width=4)
    d.rectangle([16, PS - 52, PS - 16, PS - 28], fill=_c(p.hazard, 150))
    return add_halo(_shrink(img), p.hazard, spread=0.18, strength=180)


def vent_galaxy(p):
    """Void rift: a torn lens with an event-horizon glint."""
    img, d = _prop_canvas()
    d.polygon([(30, PS/2), (PS/2, PS/2 - 74), (PS - 30, PS/2), (PS/2, PS/2 + 74)],
              fill=(10, 6, 16, 235), outline=_c(p.hazard), width=10)
    d.line([56, PS/2, PS - 56, PS/2], fill=(255, 255, 255, 200), width=6)
    for x, r in [(120, 5), (200, 7), (270, 4)]:
        d.ellipse([x - r, PS/2 - r, x + r, PS/2 + r], fill=(255, 255, 255, 220))
    return add_halo(_shrink(img), p.hazard, spread=0.2, strength=180)


WALL_FNS = {"core": wall_core, "foundry": wall_foundry, "biolab": wall_biolab,
            "prism": wall_prism, "galaxy": wall_galaxy}
PILLAR_FNS = {"core": pillar_core, "foundry": pillar_foundry, "biolab": pillar_biolab,
              "prism": pillar_prism, "galaxy": pillar_galaxy}
VENT_FNS = {"core": vent_core, "foundry": vent_foundry, "biolab": vent_biolab,
            "prism": vent_prism, "galaxy": vent_galaxy}


def main():
    # `--props-only` regenerates walls/pillars/vents without touching the
    # backdrops — needed because the committed core/foundry/biolab bg_* PNGs
    # predate the seed_for() fix (see above) and a full run would rewrite them.
    # Optional arena-name args filter by theme, as before.
    args = sys.argv[1:]
    props_only = "--props-only" in args
    only = set(a for a in args if a != "--props-only")
    print("make_backdrops:")
    for pal in ARENAS:
        if only and pal.name not in only:
            continue
        if not props_only:
            rng = random.Random(seed_for(pal.name))
            far, mid, near = LAYERS.get(pal.name, (far_layer, mid_layer, near_layer))
            save_png(f"bg_{pal.name}_far", far(pal, rng))
            save_png(f"bg_{pal.name}_mid", mid(pal, rng))
            save_png(f"bg_{pal.name}_near", near(pal, rng))
        save_png(f"wall_{pal.name}", WALL_FNS[pal.name](pal))
        save_png(f"pillar_{pal.name}", PILLAR_FNS[pal.name](pal))
        save_png(f"vent_{pal.name}", VENT_FNS[pal.name](pal))


if __name__ == "__main__":
    main()
