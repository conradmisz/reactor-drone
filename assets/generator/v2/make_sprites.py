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

from PIL import Image, ImageDraw, ImageFilter

from common import add_halo, write_sprite
from palette import CORE, FOUNDRY, BIOLAB

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
    # Enemies: march (loop) + death (oneshot) concatenated
    enemies = [
        ("enemy_spark", spark_shape, CORE, CORE.secondary),
        ("enemy_runner", runner_shape, BIOLAB, BIOLAB.primary),
        ("enemy_hulk", hulk_shape, FOUNDRY, FOUNDRY.primary),
        ("enemy_warden", warden_shape, CORE, CORE.secondary),
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


if __name__ == "__main__":
    main()
