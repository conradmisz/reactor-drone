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


def boom(img, p0, p1, col, w):
    """A structural arm between the chassis and a rotor pod."""
    d = ImageDraw.Draw(img)
    d.line([p0, p1], fill=(col[0], col[1], col[2], 255), width=w)


def rotor(img, xy, r, body, accent, b, t, blades=2, w=3):
    """A rotor pod: pod ring + a translucent swept disc + blades at phase `t`.

    The blades rotate with the frame phase, which is the whole reason these read
    as drones rather than as polygons with circles glued on. Drawn on an overlay
    so the translucent disc does not wash out the pod rim under the baked halo.
    """
    x, y = xy
    lay = Image.new("RGBA", img.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(lay)
    d.ellipse([x-r, y-r, x+r, y+r], fill=(body[0], body[1], body[2], 70))
    d.ellipse([x-r, y-r, x+r, y+r], outline=(accent[0], accent[1], accent[2], 255), width=w)
    deg = 360.0 * t
    for k in range(blades):
        a0 = deg + 360.0 * k / blades
        d.arc([x-r+w, y-r+w, x+r-w, y+r-w], a0, a0 + 54,
              fill=(accent[0], accent[1], accent[2], 210), width=w)
    d.ellipse([x-4, y-4, x+4, y+4], fill=(body[0], body[1], body[2], 255))
    img.alpha_composite(lay)


def quad_pods(f, pal, col, b, t, pods, r, hub, bw):
    """Four (or two) rotor pods on booms from `hub`, drawn back-to-front."""
    for px, py in pods:
        boom(f, hub, (px, py), scale_col(col, 0.45), bw)
    for px, py in pods:
        rotor(f, (px, py), r, scale_col(col, 0.8), scale_col(pal.accent, b), b, t)


# ---------------------------------------------------------------------------
# Player drone (#8, D107) — a quad-rotor drone, not an arrow. Four spinning
# rotor pods on booms around a wedge chassis, sensor eye forward, twin thrusters
# aft. Symmetric about the horizontal axis so pure rotation orients it.
# ---------------------------------------------------------------------------
def player_frames(n=6):
    pal = CORE
    frames = []
    cx, cy = S/2, S/2
    for i in range(n):
        f = frame()
        b = pulse(i / n)
        t = i / n
        pods = [(cx+27, cy-29), (cx+27, cy+29), (cx-21, cy-29), (cx-21, cy+29)]
        quad_pods(f, pal, pal.primary, b, t, pods, 17, (cx+2, cy), 7)
        # chassis: a forward wedge with a squared-off tail
        hull = [(cx+42, cy), (cx+16, cy-19), (cx-24, cy-15),
                (cx-24, cy+15), (cx+16, cy+19)]
        neon_poly(f, hull, scale_col(pal.primary, 0.7),
                  outline=scale_col(pal.accent, b), ow=3)
        # spine plating
        d = ImageDraw.Draw(f)
        d.line([(cx-16, cy-8), (cx+18, cy-8)], fill=scale_col(pal.secondary, 0.8) + (255,), width=2)
        d.line([(cx-16, cy+8), (cx+18, cy+8)], fill=scale_col(pal.secondary, 0.8) + (255,), width=2)
        # sensor eye
        dot(f, (cx+16, cy), 9, scale_col(pal.accent, b))
        dot(f, (cx+16, cy), 4, (255, 255, 255))
        # thrusters
        for ey in (-9, 9):
            dot(f, (cx-24, cy+ey), 5, scale_col(pal.secondary, b))
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
        shape_fn(f, pal, body_col, b, i / n)
        f = add_halo(f, body_col, spread=0.16, strength=150)
        frames.append(f)
    # death: n-frame dissolve (fade + shrink into a flash)
    death = []
    dn = 6
    for i in range(dn):
        f = frame()
        t = i / (dn - 1)
        base = frame()
        shape_fn(base, pal, body_col, 1.0, t)
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


def spark_shape(f, pal, col, b, t):
    """Scout quad: a tiny four-rotor drone. Smallest silhouette in the game, so
    the body is barely more than a hub — the four rotors carry the read."""
    cx, cy = S/2, S/2
    pods = [(cx+21, cy-21), (cx+21, cy+21), (cx-21, cy-21), (cx-21, cy+21)]
    quad_pods(f, pal, col, b, t, pods, 12, (cx, cy), 5)
    r = 15
    neon_poly(f, [(cx+r, cy), (cx, cy-r), (cx-r, cy), (cx, cy+r)],
              scale_col(col, 0.65), outline=scale_col(pal.accent, b), ow=3)
    dot(f, (cx, cy), 6, scale_col(pal.accent, b))
    dot(f, (cx+4, cy), 3, (255, 255, 255))


def runner_shape(f, pal, col, b, t):
    """Interceptor: a dart chassis with two swept-back rotor booms. Reads fast
    because the rotors trail the nose instead of bracketing it."""
    cx, cy = S/2, S/2
    pods = [(cx-10, cy-27), (cx-10, cy+27)]
    quad_pods(f, pal, col, b, t, pods, 14, (cx+6, cy), 6)
    neon_poly(f, [(cx+42, cy), (cx+4, cy-15), (cx-22, cy-9),
                  (cx-22, cy+9), (cx+4, cy+15)],
              scale_col(col, 0.6), outline=scale_col(pal.accent, b), ow=3)
    d = ImageDraw.Draw(f)
    d.line([(cx-16, cy), (cx+22, cy)], fill=scale_col(pal.secondary, 0.9) + (255,), width=2)
    dot(f, (cx+2, cy), 7, scale_col(pal.accent, b))
    dot(f, (cx+20, cy), 4, (255, 255, 255))


def hulk_shape(f, pal, col, b, t):
    """Heavy lifter: an armoured hex core slung under four big rotor pods, with
    a cargo clamp forward. The one enemy whose booms are thicker than its arms."""
    cx, cy = S/2, S/2
    pods = [(cx+26, cy-30), (cx+26, cy+30), (cx-26, cy-30), (cx-26, cy+30)]
    quad_pods(f, pal, col, b, t, pods, 18, (cx, cy), 9)
    r = 30
    pts = [(cx + r*math.cos(a), cy + r*math.sin(a))
           for a in [math.pi*k/3 for k in range(6)]]
    neon_poly(f, pts, scale_col(col, 0.55), outline=scale_col(pal.accent, b), ow=4)
    d = ImageDraw.Draw(f)
    r2 = 17
    d.polygon([(cx + r2*math.cos(a), cy + r2*math.sin(a))
               for a in [math.pi*k/3 for k in range(6)]],
              outline=scale_col(pal.secondary, 1.0) + (255,))
    # cargo clamp: two jaws off the nose
    for sgn in (-1, 1):
        d.line([(cx+26, cy+sgn*9), (cx+40, cy+sgn*14), (cx+44, cy+sgn*5)],
               fill=scale_col(col, 0.85) + (255,), width=4, joint="curve")
    dot(f, (cx, cy), 8, scale_col(pal.accent, b))


def warden_shape(f, pal, col, b, t):
    """Gunship: octagonal turret core, forward barrel, one rotor pod above and
    below plus two small stabiliser fans aft. Bulkier than the interceptor and
    obviously armed."""
    cx, cy = S/2, S/2
    pods = [(cx-2, cy-32), (cx-2, cy+32)]
    quad_pods(f, pal, col, b, t, pods, 15, (cx-2, cy), 7)
    for sgn in (-1, 1):
        rotor(f, (cx-28, cy+sgn*17), 9, scale_col(col, 0.8), scale_col(pal.accent, b),
              b, 1.0 - t, blades=3, w=2)
    r = 26
    pts = [(cx + r*math.cos(a+math.pi/8), cy + r*math.sin(a+math.pi/8))
           for a in [math.pi*k/4 for k in range(8)]]
    neon_poly(f, pts, scale_col(col, 0.5), outline=scale_col(pal.accent, b), ow=3)
    d = ImageDraw.Draw(f)
    d.rectangle([cx+r-6, cy-8, cx+r+20, cy+8], fill=scale_col(col, 0.7) + (255,))
    d.rectangle([cx+r-6, cy-8, cx+r+20, cy+8],
                outline=scale_col(pal.accent, b) + (255,), width=2)
    dot(f, (cx+r+20, cy), 6, scale_col(pal.accent, b))
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

    def shape(f, pal, col, b, _t):
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
# The boss — "Capital Drone Carrier" (#10, D105). The boss used to wear an
# `Images{"v2/enemy_hulk.png"}`, i.e. the whole 4x4 ATLAS, which is why it read
# as a grid of hexagons. It is a single full-frame image now: one 256px carrier,
# drawn at 512 and downsampled so the plating survives, MONO like every other
# enemy so BossSystem's per-arena tint themes it (Foundry orange, Core magenta,
# and so on). Single frame on purpose — the boss is a static Images wearer, not
# a SpriteSheet, and giving it an atlas would be the exact bug again.
# ---------------------------------------------------------------------------
CW = 512      # carrier working canvas (downsampled to CO)
CO = 256


def carrier_sprite():
    pal = MONO
    col = pal.primary
    f = Image.new("RGBA", (CW, CW), (0, 0, 0, 0))
    cx, cy = CW/2, CW/2
    hull = scale_col(col, 0.26)
    deck = scale_col(col, 0.10)
    plate = scale_col(col, 0.58)
    rim = scale_col(col, 1.0)
    d = ImageDraw.Draw(f)

    # --- rotor nacelles on outrigger pylons (drawn first: they sit behind) ---
    for sx in (-1, 1):
        for sy in (-1, 1):
            px, py = cx + sx*104, cy + sy*152
            d.line([(cx + sx*60, cy + sy*62), (px, py)],
                   fill=hull + (255,), width=26)
            d.line([(cx + sx*60, cy + sy*62), (px, py)],
                   fill=plate + (255,), width=6)
    for sx in (-1, 1):
        for sy in (-1, 1):
            px, py = cx + sx*104, cy + sy*152
            rotor(f, (px, py), 54, scale_col(col, 0.55), rim, 1.0, 0.12, blades=4, w=7)
            d = ImageDraw.Draw(f)
            d.ellipse([px-58, py-58, px+58, py+58], outline=plate + (255,), width=4)

    # --- hull: a long armoured spearhead ---
    body = [(cx+236, cy), (cx+186, cy-46), (cx+120, cy-72), (cx-146, cy-92),
            (cx-206, cy-56), (cx-206, cy+56), (cx-146, cy+92), (cx+120, cy+72),
            (cx+186, cy+46)]
    d.polygon(body, fill=hull + (255,))
    d.polygon([(cx+188, cy), (cx+110, cy-46), (cx-140, cy-62), (cx-186, cy-36),
               (cx-186, cy+36), (cx-140, cy+62), (cx+110, cy+46)],
              fill=scale_col(col, 0.15) + (255,))
    d.line(body + [body[0]], fill=rim + (255,), width=6)

    # --- flight decks: a recessed launch trench along each flank, with bays ---
    for sy in (-1, 1):
        y0, y1 = cy + sy*88, cy + sy*58
        d.rectangle([cx-140, min(y0, y1), cx+112, max(y0, y1)], fill=deck + (255,))
        d.rectangle([cx-140, min(y0, y1), cx+112, max(y0, y1)],
                    outline=plate + (255,), width=3)
        for k in range(6):          # launch bays, lit
            bx = cx - 126 + k*42
            d.rectangle([bx, min(y0, y1)+7, bx+26, max(y0, y1)-7],
                        fill=scale_col(col, 0.85) + (255,))
        for k in range(3):          # docked micro-drones on the deck lip
            mx = cx - 108 + k*84
            my = cy + sy*46
            d.polygon([(mx+13, my), (mx-7, my-9), (mx-2, my), (mx-7, my+9)],
                      fill=plate + (255,))

    # --- spine, ribs, bridge, radar ---
    d.line([(cx-190, cy), (cx+200, cy)], fill=scale_col(col, 0.75) + (255,), width=5)
    for k in range(7):
        rx = cx - 150 + k*44
        d.line([(rx, cy-30), (rx, cy+30)], fill=plate + (255,), width=3)
    for r in (46, 30, 14):          # radar dish amidships
        d.ellipse([cx-56-r, cy-r, cx-56+r, cy+r], outline=plate + (255,), width=3)
    d.line([(cx-56, cy-46), (cx-56, cy+46)], fill=rim + (255,), width=3)
    bridge = [(cx+118, cy), (cx+86, cy-34), (cx+34, cy-30),
              (cx+34, cy+30), (cx+86, cy+34)]
    d.polygon(bridge, fill=scale_col(col, 0.58) + (255,))
    d.line(bridge + [bridge[0]], fill=rim + (255,), width=4)
    dot(f, (cx+72, cy), 20, rim)
    dot(f, (cx+72, cy), 10, (255, 255, 255))

    # --- prow: main bay aperture, split by an armoured beak ---
    d = ImageDraw.Draw(f)
    for sy in (-1, 1):          # armoured beak, split by the main bay mouth
        d.polygon([(cx+236, cy + sy*6), (cx+150, cy + sy*40), (cx+150, cy + sy*14)],
                  fill=scale_col(col, 0.9) + (255,))
        d.line([(cx+186, cy + sy*46), (cx+236, cy + sy*4)], fill=rim + (255,), width=5)
        d.line([(cx+150, cy + sy*13), (cx+226, cy + sy*3)], fill=rim + (255,), width=3)
    d.rectangle([cx+150, cy-12, cx+222, cy+12], fill=deck + (255,))
    d.rectangle([cx+150, cy-12, cx+222, cy+12], outline=rim + (255,), width=3)

    # --- engine bank aft: three nozzles + exhaust bloom ---
    for k, ey in enumerate((-40, 0, 40)):
        d.rectangle([cx-232, cy+ey-17, cx-198, cy+ey+17], fill=plate + (255,))
        d.rectangle([cx-232, cy+ey-17, cx-198, cy+ey+17], outline=rim + (255,), width=3)
        dot(f, (cx-230, cy+ey), 12, rim, 230)
        dot(f, (cx-236, cy+ey), 6, (255, 255, 255))

    # --- masts ---
    for sy in (-1, 1):
        d.line([(cx+20, cy + sy*72), (cx+4, cy + sy*118)], fill=plate + (255,), width=5)
        dot(f, (cx+4, cy + sy*118), 7, rim)

    f = f.resize((CO, CO), Image.LANCZOS)
    return add_halo(f, col, spread=0.11, strength=140)


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
    # D105: the boss's carrier. Single frame, worn as Images by BossSystem.
    save_png("enemy_boss_carrier", carrier_sprite())


if __name__ == "__main__":
    main()
