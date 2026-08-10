#!/usr/bin/env python3
"""Procedural neon sprites for Reactor Drone v2.

All art faces RIGHT (angle 0) and is drawn symmetric about the horizontal axis,
so the renderer's flip heuristic can be disabled (flip_when_left=false) and pure
rotation orients each entity. Every sprite bakes an outer glow halo and writes an
atlas + sidecar in the engine's documented format.

Entities: player drone, 4 enemies (spark / runner / hulk / warden), 2 projectiles
(plasma / bolt), 8-frame explosion, 4-frame impact flash.

Run: python make_sprites.py
"""
from __future__ import annotations

import math

from PIL import Image, ImageChops, ImageDraw, ImageFilter

from common import add_halo, write_sprite, save_png
from palette import CORE, FOUNDRY, BIOLAB, MONO

S = 128  # frame size


def frame():
    return Image.new("RGBA", (S, S), (0, 0, 0, 0))


def neon_poly(img, pts, fill, outline=None, ow=3):
    """Filled polygon with a bright neon outline drawn onto a fresh overlay so the
    core stays crisp under the baked halo."""
    d = ImageDraw.Draw(img)
    d.polygon(pts, fill=(fill[0], fill[1], fill[2], 255))
    if outline:
        d.line(pts + [pts[0]], fill=(outline[0], outline[1], outline[2], 255), width=ow)


def dot(img, xy, r, col, a=255):
    d = ImageDraw.Draw(img)
    d.ellipse([xy[0]-r, xy[1]-r, xy[0]+r, xy[1]+r],
              fill=(col[0], col[1], col[2], a))


def pulse(t, lo=0.55, hi=1.0):
    """0..1 phase -> brightness multiplier (smooth sine)."""
    return lo + (hi - lo) * 0.5 * (1 + math.sin(t * 2 * math.pi))


def scale_col(c, m):
    return (min(255, int(c[0]*m)), min(255, int(c[1]*m)), min(255, int(c[2]*m)))


# ---------------------------------------------------------------------------
# Player drone — arrow hull, cyan, thruster pulse
# ---------------------------------------------------------------------------
def player_frames(n=6):
    pal = CORE
    frames = []
    cx, cy = S/2, S/2
    for i in range(n):
        f = frame()
        b = pulse(i / n)
        # hull: forward-pointing arrow
        hull = [(cx+42, cy), (cx-24, cy-30), (cx-10, cy), (cx-24, cy+30)]
        neon_poly(f, hull, scale_col(pal.primary, 0.7),
                  outline=scale_col(pal.accent, b), ow=3)
        # cockpit glow
        dot(f, (cx+6, cy), 9, scale_col(pal.accent, b))
        dot(f, (cx+6, cy), 4, (255, 255, 255))
        # engine exhaust dots (brighter on pulse)
        for ey in (-13, 13):
            dot(f, (cx-22, cy+ey), 5, scale_col(pal.secondary, b))
        f = add_halo(f, pal.primary, spread=0.14, strength=150)
        frames.append(f)
    return frames


# ---------------------------------------------------------------------------
# Generic enemy builder: a body-shape function + palette + n frames + death
# ---------------------------------------------------------------------------
def enemy_frames(shape_fn, pal, body_col, n=8):
    frames = []
    for i in range(n):
        f = frame()
        b = pulse(i / n)
        shape_fn(f, pal, body_col, b)
        f = add_halo(f, body_col, spread=0.16, strength=150)
        frames.append(f)
    # death: n-frame dissolve (fade + shrink into a flash)
    death = []
    dn = 6
    for i in range(dn):
        f = frame()
        t = i / (dn - 1)
        base = frame()
        shape_fn(base, pal, body_col, 1.0)
        sc = 1.0 - 0.5 * t
        small = base.resize((int(S*sc), int(S*sc)))
        f.paste(small, (int((S-small.size[0])/2), int((S-small.size[1])/2)), small)
        # fade alpha
        alpha = f.split()[3].point(lambda v: int(v * (1 - t)))
        f.putalpha(alpha)
        if t < 0.6:  # early flash
            fl = frame(); dot(fl, (S/2, S/2), int(18 + 30*t), pal.accent, int(180*(1-t)))
            f = Image.alpha_composite(f, fl)
        f = add_halo(f, body_col, spread=0.16, strength=int(150*(1-t)))
        death.append(f)
    return frames, death


def spark_shape(f, pal, col, b):  # small fast diamond
    cx, cy = S/2, S/2
    r = 26
    pts = [(cx+r, cy), (cx, cy-r), (cx-r, cy), (cx, cy+r)]
    neon_poly(f, pts, scale_col(col, 0.65), outline=scale_col(pal.accent, b), ow=3)
    dot(f, (cx, cy), 7, scale_col(pal.accent, b))


def runner_shape(f, pal, col, b):  # forward chevron dart
    cx, cy = S/2, S/2
    pts = [(cx+40, cy), (cx-8, cy-26), (cx-18, cy), (cx-8, cy+26)]
    neon_poly(f, pts, scale_col(col, 0.6), outline=scale_col(pal.accent, b), ow=3)
    dot(f, (cx-2, cy), 6, scale_col(pal.accent, b))
    dot(f, (cx+16, cy), 4, (255, 255, 255))


def hulk_shape(f, pal, col, b):  # bulky armored hexagon
    cx, cy = S/2, S/2
    r = 40
    pts = [(cx + r*math.cos(a), cy + r*math.sin(a))
           for a in [math.pi*k/3 for k in range(6)]]
    neon_poly(f, pts, scale_col(col, 0.55), outline=scale_col(pal.accent, b), ow=4)
    # inner plating
    d = ImageDraw.Draw(f)
    r2 = 22
    pts2 = [(cx + r2*math.cos(a), cy + r2*math.sin(a))
            for a in [math.pi*k/3 for k in range(6)]]
    d.polygon(pts2, outline=(pal.secondary[0], pal.secondary[1], pal.secondary[2], 255))
    dot(f, (cx, cy), 8, scale_col(pal.accent, b))


def warden_shape(f, pal, col, b):  # turret octagon + barrel facing right
    cx, cy = S/2, S/2
    r = 34
    pts = [(cx + r*math.cos(a+math.pi/8), cy + r*math.sin(a+math.pi/8))
           for a in [math.pi*k/4 for k in range(8)]]
    neon_poly(f, pts, scale_col(col, 0.5), outline=scale_col(pal.accent, b), ow=3)
    d = ImageDraw.Draw(f)
    # barrel
    d.rectangle([cx+r-4, cy-7, cx+r+22, cy+7],
                fill=(scale_col(col, 0.7)[0], scale_col(col,0.7)[1], scale_col(col,0.7)[2], 255))
    dot(f, (cx+r+22, cy), 6, scale_col(pal.accent, b))
    dot(f, (cx, cy), 10, scale_col(pal.secondary, b))
    dot(f, (cx, cy), 4, (255, 255, 255))


# ---------------------------------------------------------------------------
# Moon shooters (D93) — a WANING CRESCENT, not a full disc. Built as a lit disc
# with a second disc punched out of its leading edge, so the silhouette itself
# carries the read; at gameplay size a filled circle and a crescent are only
# distinguishable by that missing bite. Drawn against MONO like every other
# enemy — arena enemy_tint colour-mods them at runtime.
# ---------------------------------------------------------------------------
def _crescent(f, r, dx, br, fill, outline, ow):
    """Lit disc of radius r minus a disc of radius br offset dx to the RIGHT
    (art faces right, so the horns point the way the moon is travelling).
    Returns the two horn tips for tier decoration."""
    cx, cy = S/2, S/2
    lay = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(lay)
    d.ellipse([cx-r, cy-r, cx+r, cy+r], fill=fill + (255,))
    d.ellipse([cx-r, cy-r, cx+r, cy+r], outline=outline + (255,), width=ow)
    # Inner rim: a ring hugging the OUTSIDE of the bite, so it survives the punch
    # and the terminator gets an edge as bright as the limb.
    g = br + ow * 0.5
    d.ellipse([cx+dx-g, cy-g, cx+dx+g, cy+g], outline=outline + (255,), width=ow)
    # Punch the shadowed lobe, and clip everything to the lit disc so no stray
    # rim escapes the silhouette.
    mask = Image.new("L", (S, S), 0)
    md = ImageDraw.Draw(mask)
    md.ellipse([cx-r, cy-r, cx+r, cy+r], fill=255)
    md.ellipse([cx+dx-br, cy-br, cx+dx+br, cy+br], fill=0)
    lay.putalpha(ImageChops.multiply(lay.split()[3], mask))
    f.alpha_composite(lay)
    # circle-circle intersection: the horn tips
    hx = (r*r - br*br + dx*dx) / (2*dx)
    hy = math.sqrt(max(0.0, r*r - hx*hx))
    return (cx + hx, cy - hy), (cx + hx, cy + hy)


def moon_shape(tier):
    """tier 1 slow shot / 2 tracking / 3 laser — one escalating family."""
    r, dx, br, ow = {1: (32, 26, 30, 3), 2: (37, 28, 33, 4), 3: (42, 27, 36, 5)}[tier]

    def shape(f, pal, col, b):
        cx, cy = S/2, S/2
        top, bot = _crescent(f, r, dx, br, scale_col(col, 0.55),
                             scale_col(pal.accent, b), ow)
        # Tier 2+: barbed horns. Tier 3: a charging focus in the crescent's mouth.
        if tier >= 2:
            for (hx, hy), sgn in ((top, -1), (bot, 1)):
                pts = [(hx, hy), (hx + 16, hy + sgn*4), (hx - 4, hy + sgn*12)]
                neon_poly(f, pts, scale_col(col, 0.7),
                          outline=scale_col(pal.accent, b), ow=2)
        dot(f, (cx - r*0.45, cy), 6 + tier, scale_col(pal.accent, b))
        if tier >= 3:
            dot(f, (cx + dx - br*0.35, cy), 9, scale_col(pal.accent, b), 210)
            dot(f, (cx + dx - br*0.35, cy), 4, (255, 255, 255))
    return shape


# ---------------------------------------------------------------------------
# Pickups and hazards — single-frame, full-colour (no runtime tint on these) and
# 96px like the pillar/vent family. Each is silhouette-first: at pickup size the
# player sees the shape, the colour only confirms it.
# ---------------------------------------------------------------------------
P = 96


def _pickup_frame():
    return Image.new("RGBA", (P, P), (0, 0, 0, 0))


def health_pickup():
    """Repair cell: a medical cross in a housing ring. Green (110,235,130) — the
    exact colour the flat-Color placeholder used, so nothing else has to move."""
    body, edge = (60, 170, 90), (170, 255, 195)
    f = _pickup_frame()
    d = ImageDraw.Draw(f)
    c = P/2
    d.ellipse([c-38, c-38, c+38, c+38], outline=(60, 140, 90, 255), width=4)
    for box in ([c-10, c-30, c+10, c+30], [c-30, c-10, c+30, c+10]):
        d.rounded_rectangle(box, radius=5, fill=body + (255,))
    for box in ([c-10, c-30, c+10, c+30], [c-30, c-10, c+30, c+10]):
        d.rounded_rectangle(box, radius=5, outline=edge + (255,), width=3)
    d.rounded_rectangle([c-4, c-22, c+4, c+22], radius=3, fill=(235, 255, 240, 255))
    return add_halo(f, (110, 235, 130), spread=0.16, strength=170)


def shield_pickup():
    """Barrier capacitor: a crest with a charge chevron and two terminal nubs.
    Ice blue (120,200,255) — the placeholder's colour."""
    body, edge = (55, 115, 180), (190, 230, 255)
    f = _pickup_frame()
    d = ImageDraw.Draw(f)
    c = P/2
    crest = [(c, c-34), (c+28, c-18), (c+28, c+4), (c, c+34), (c-28, c+4), (c-28, c-18)]
    d.polygon(crest, fill=body + (255,))
    d.line(crest + [crest[0]], fill=edge + (255,), width=4)
    d.line([(c-15, c-4), (c, c+10), (c+15, c-4)], fill=(235, 248, 255, 255), width=5)
    for sx in (-1, 1):
        d.rectangle([c + sx*28 - 4, c-8, c + sx*28 + 4, c+2], fill=edge + (255,))
    return add_halo(f, (120, 200, 255), spread=0.16, strength=170)


def coin_sprite():
    """Currency: a struck circular coin. Hard bright rim + inner ring + a credit
    glyph, so it never reads as a projectile (those are soft glow blobs with no
    edge). Gold (255,210,90) — the placeholder's colour."""
    f = _pickup_frame()
    d = ImageDraw.Draw(f)
    c = P/2
    d.ellipse([c-34, c-34, c+34, c+34], fill=(190, 140, 35, 255))
    d.ellipse([c-34, c-34, c+34, c+34], outline=(255, 235, 150, 255), width=5)
    d.ellipse([c-21, c-21, c+21, c+21], outline=(255, 200, 70, 255), width=3)
    # credit glyph: one bar. Anything finer turns to mush at pickup size.
    d.line([(c, c-15), (c, c+15)], fill=(255, 245, 200, 255), width=5)
    # struck-metal specular on the upper-left rim
    d.arc([c-30, c-30, c+30, c+30], 195, 255, fill=(255, 255, 235, 255), width=4)
    return add_halo(f, (255, 210, 90), spread=0.14, strength=150)


def mine_sprite():
    """Foundry mine: the bomb-emoji read — round dark body, cap, curved fuse, lit
    spark. The body is dark so the hot orange rim and the spark carry it against
    a near-black arena."""
    f = _pickup_frame()
    d = ImageDraw.Draw(f)
    c = P/2
    cy = c + 8
    d.ellipse([c-30, cy-30, c+30, cy+30], fill=(38, 26, 30, 255))
    d.ellipse([c-30, cy-30, c+30, cy+30], outline=(255, 140, 50, 255), width=5)
    d.ellipse([c-16, cy-18, c-4, cy-8], fill=(255, 205, 150, 220))   # specular
    d.rectangle([c-8, cy-38, c+8, cy-27], fill=(255, 150, 60, 255))  # cap
    fuse = [(c, cy-38), (c+9, cy-46), (c+20, cy-44), (c+24, cy-34)]
    d.line(fuse, fill=(255, 220, 150, 255), width=4, joint="curve")
    d.ellipse([c+18, cy-40, c+30, cy-28], fill=(255, 245, 200, 255))  # spark
    for k in range(6):   # spark rays
        a = math.pi*2*k/6
        d.line([(c+24, cy-34),
                (c+24 + math.cos(a)*13, cy-34 + math.sin(a)*13)],
               fill=(255, 190, 90, 255), width=2)
    return add_halo(f, (255, 140, 50), spread=0.16, strength=180)


# ---------------------------------------------------------------------------
# Projectiles
# ---------------------------------------------------------------------------
def plasma_frames(n=4):
    pal = CORE
    frames = []
    for i in range(n):
        f = frame()
        b = pulse(i / n)
        dot(f, (S/2, S/2), int(16*b), pal.primary)
        dot(f, (S/2, S/2), 8, pal.accent)
        dot(f, (S/2, S/2), 4, (255, 255, 255))
        f = add_halo(f, pal.primary, spread=0.22, strength=180)
        frames.append(f)
    return frames


def bolt_frames(n=4):
    pal = FOUNDRY
    frames = []
    for i in range(n):
        f = frame()
        b = pulse(i / n)
        d = ImageDraw.Draw(f)
        cx, cy = S/2, S/2
        # elongated dart facing right
        pts = [(cx+30, cy), (cx-18, cy-9), (cx-10, cy), (cx-18, cy+9)]
        neon_poly(f, pts, scale_col(pal.primary, 0.8), outline=scale_col(pal.accent, b), ow=2)
        dot(f, (cx+12, cy), 4, (255, 255, 255))
        f = add_halo(f, pal.primary, spread=0.2, strength=170)
        frames.append(f)
    return frames


# ---------------------------------------------------------------------------
# Effects
# ---------------------------------------------------------------------------
def explosion_frames(n=8):
    pal = FOUNDRY
    frames = []
    cx, cy = S/2, S/2
    for i in range(n):
        f = frame()
        t = i / (n - 1)
        r = 10 + t * 54
        a = int(255 * (1 - t) ** 1.3)
        # expanding ring
        d = ImageDraw.Draw(f)
        col = pal.accent if t < 0.4 else pal.primary
        d.ellipse([cx-r, cy-r, cx+r, cy+r], outline=(col[0], col[1], col[2], a),
                  width=max(2, int(10*(1-t))))
        # hot core early
        if t < 0.6:
            dot(f, (cx, cy), int(20*(1-t)+6), pal.accent, int(220*(1-t)))
        # sparks
        for k in range(8):
            ang = math.pi*2*k/8 + i*0.3
            rr = r*0.9
            dot(f, (cx+math.cos(ang)*rr, cy+math.sin(ang)*rr), max(1,int(5*(1-t))),
                pal.secondary, a)
        f = f.filter(ImageFilter.GaussianBlur(1.2))
        frames.append(f)
    return frames


def impact_frames(n=4):
    pal = CORE
    frames = []
    cx, cy = S/2, S/2
    for i in range(n):
        f = frame()
        t = i / (n - 1)
        r = 6 + t * 22
        a = int(255 * (1 - t))
        for k in range(6):
            ang = math.pi*2*k/6
            dot(f, (cx+math.cos(ang)*r, cy+math.sin(ang)*r), max(1,int(4*(1-t)+1)),
                pal.accent, a)
        dot(f, (cx, cy), int(8*(1-t)+2), (255,255,255), a)
        f = f.filter(ImageFilter.GaussianBlur(0.8))
        frames.append(f)
    return frames


def main():
    print("make_sprites:")
    # Player
    write_sprite("player_drone", player_frames(6), 3,
                 {"march": {"start_frame": 0, "frame_count": 6,
                            "frame_duration": 0.09, "looping": True}})
    # Enemies: march (loop) + death (oneshot) concatenated.
    # v2 Phase 5a: drawn against MONO, i.e. pure luminance. These used to bake an
    # arena's own primary (runner=BIOLAB green, hulk=FOUNDRY orange), which made
    # those enemies invisible in the arena they belonged to. The colour now comes
    # from ArenaDef::enemy_tint at spawn, via SDL colour-mod, so one sprite set
    # serves every arena — and the tie-dye hue cycle in Prism.
    enemies = [
        ("enemy_spark", spark_shape, MONO, MONO.primary),
        ("enemy_runner", runner_shape, MONO, MONO.primary),
        ("enemy_hulk", hulk_shape, MONO, MONO.primary),
        ("enemy_warden", warden_shape, MONO, MONO.primary),
        # D93: the three moon shooters. They shared enemy_warden's octagon until
        # now, which is why nothing about them read as a moon.
        ("enemy_moon_1", moon_shape(1), MONO, MONO.primary),
        ("enemy_moon_2", moon_shape(2), MONO, MONO.primary),
        ("enemy_moon_3", moon_shape(3), MONO, MONO.primary),
    ]
    for name, shape, pal, col in enemies:
        march, death = enemy_frames(shape, pal, col, 8)
        allf = march + death
        write_sprite(name, allf, 4, {
            "march": {"start_frame": 0, "frame_count": len(march),
                      "frame_duration": 0.1, "looping": True},
            "death": {"start_frame": len(march), "frame_count": len(death),
                      "frame_duration": 0.08, "looping": False},
        })
    # Projectiles
    write_sprite("projectile_plasma", plasma_frames(4), 4,
                 {"pulse": {"start_frame": 0, "frame_count": 4,
                            "frame_duration": 0.06, "looping": True}})
    write_sprite("projectile_bolt", bolt_frames(4), 4,
                 {"pulse": {"start_frame": 0, "frame_count": 4,
                            "frame_duration": 0.06, "looping": True}})
    # Effects
    write_sprite("effect_explosion", explosion_frames(8), 4,
                 {"expand": {"start_frame": 0, "frame_count": 8,
                             "frame_duration": 0.06, "looping": False}})
    write_sprite("effect_impact", impact_frames(4), 4,
                 {"expand": {"start_frame": 0, "frame_count": 4,
                             "frame_duration": 0.05, "looping": False}})
    # Single-frame pickups/hazards (D94-D96). No sidecar: these are worn by an
    # `Images` component, which takes a bare PNG path relative to assets/images/.
    save_png("pickup_health", health_pickup())
    save_png("pickup_shield", shield_pickup())
    save_png("pickup_coin", coin_sprite())
    save_png("hazard_mine", mine_sprite())


if __name__ == "__main__":
    main()
