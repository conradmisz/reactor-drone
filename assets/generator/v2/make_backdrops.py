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
    # Optional arena-name args regenerate just those themes. Needed because the
    # core/foundry/biolab PNGs predate the seed_for() fix (see above) and a full
    # run would rewrite them; `make_backdrops.py galaxy` touches only galaxy.
    only = set(sys.argv[1:])
    print("make_backdrops:")
    for pal in ARENAS:
        if only and pal.name not in only:
            continue
        rng = random.Random(seed_for(pal.name))
        far, mid, near = LAYERS.get(pal.name, (far_layer, mid_layer, near_layer))
        save_png(f"bg_{pal.name}_far", far(pal, rng))
        save_png(f"bg_{pal.name}_mid", mid(pal, rng))
        save_png(f"bg_{pal.name}_near", near(pal, rng))
        save_png(f"pillar_{pal.name}", pillar(pal))
        save_png(f"vent_{pal.name}", vent(pal))


if __name__ == "__main__":
    main()
