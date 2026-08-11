#!/usr/bin/env python3
"""Procedural neon sprites for Reactor Drone v2.

All art faces RIGHT (angle 0) and is drawn symmetric about the horizontal axis,
so the renderer's flip heuristic can be disabled (flip_when_left=false) and pure
rotation orients each entity. Every sprite bakes an outer glow halo and writes an
atlas + sidecar in the engine's documented format.

Entities: player drone, 4 enemies (spark / runner / hulk / warden), 2 projectiles
(plasma / bolt), 8-frame explosion, 4-frame impact flash.

Everything in the S-family (player, enemies, projectiles, effects) is drawn on a
canvas SS times the output edge and box-filtered down — see shrink(). Shape code
below keeps authoring in 128-space; ScaledDraw does the multiplying.

Run: python make_sprites.py
"""
from __future__ import annotations

import math

from PIL import Image, ImageChops, ImageDraw, ImageFilter

from common import add_halo, write_sprite, save_png
from palette import CORE, FOUNDRY, BIOLAB, MONO

S = 128   # OUTPUT frame size — the size art is authored at, and written at
SS = 4    # supersample factor
W = S * SS  # working canvas edge


def frame():
    return Image.new("RGBA", (W, W), (0, 0, 0, 0))


def _scale_xy(xy, s):
    """Scale either coordinate form Pillow accepts: a flat [x0,y0,x1,y1] box or
    a list of (x, y) points."""
    if isinstance(xy, (list, tuple)) and xy and isinstance(xy[0], (int, float)):
        return [v * s for v in xy]
    return [(x * s, y * s) for x, y in xy]


class ScaledDraw:
    """An ImageDraw proxy that multiplies authored coordinates, pen widths and
    corner radii by `s`.

    Pillow's polygon/line rasteriser has no antialiasing, so drawing a 128px
    sprite directly gave visibly stepped neon outlines that only the baked halo
    hid. Everything is drawn at SSx and box-filtered down instead — the same
    trick carrier_sprite() has always used (D105), applied to the whole roster.

    This proxy exists so that stays a one-line change per draw site: every shape
    function keeps its coordinates in the 128-space the art was authored in.
    """

    _SCALED_KW = ("width", "radius")

    def __init__(self, d, s):
        self._d, self._s = d, s

    def __getattr__(self, name):
        fn = getattr(self._d, name)

        def call(xy, *args, **kw):
            for k in self._SCALED_KW:
                if kw.get(k) is not None:
                    kw[k] = max(1, int(round(kw[k] * self._s)))
            return fn(_scale_xy(xy, self._s), *args, **kw)
        return call


def draw(img, s=SS):
    """ImageDraw.Draw for art authored in 128-space. `s=1` for art already
    authored at its working size (the carrier, the pickups)."""
    return ScaledDraw(ImageDraw.Draw(img), s)


def shrink(img, size=S):
    """SSx working canvas -> output frame. BOX is an exact area average over each
    SSxSS block, which is textbook supersampling for an integer factor and cannot
    ring the way a windowed filter does on hard neon edges.

    ponytail: straight (non-premultiplied) resize. The transparent black outside
    the silhouette can darken the outermost edge pixels; add_halo runs after this
    and paints over them. If a dark fringe ever shows, premultiply alpha here.
    """
    return img.resize((size, size), Image.BOX)


def neon_poly(img, pts, fill, outline=None, ow=3, s=SS):
    """Filled polygon with a bright neon outline drawn onto a fresh overlay so the
    core stays crisp under the baked halo."""
    d = draw(img, s)
    d.polygon(pts, fill=(fill[0], fill[1], fill[2], 255))
    if outline:
        d.line(pts + [pts[0]], fill=(outline[0], outline[1], outline[2], 255), width=ow)


def dot(img, xy, r, col, a=255, s=SS):
    d = draw(img, s)
    d.ellipse([xy[0]-r, xy[1]-r, xy[0]+r, xy[1]+r],
              fill=(col[0], col[1], col[2], a))


def pulse(t, lo=0.55, hi=1.0):
    """0..1 phase -> brightness multiplier (smooth sine)."""
    return lo + (hi - lo) * 0.5 * (1 + math.sin(t * 2 * math.pi))


def scale_col(c, m):
    return (min(255, int(c[0]*m)), min(255, int(c[1]*m)), min(255, int(c[2]*m)))


def boom(img, p0, p1, col, w, s=SS):
    """A structural arm between the chassis and a rotor pod."""
    d = draw(img, s)
    d.line([p0, p1], fill=(col[0], col[1], col[2], 255), width=w)


def rotor(img, xy, r, body, accent, b, t, blades=2, w=3, s=SS):
    """A rotor pod: pod ring + a translucent swept disc + blades at phase `t`.

    The blades rotate with the frame phase, which is the whole reason these read
    as drones rather than as polygons with circles glued on. Drawn on an overlay
    so the translucent disc does not wash out the pod rim under the baked halo.
    """
    x, y = xy
    lay = Image.new("RGBA", img.size, (0, 0, 0, 0))
    d = draw(lay, s)
    d.ellipse([x-r, y-r, x+r, y+r], fill=(body[0], body[1], body[2], 70))
    d.ellipse([x-r, y-r, x+r, y+r], outline=(accent[0], accent[1], accent[2], 255), width=w)
    deg = 360.0 * t
    for k in range(blades):
        a0 = deg + 360.0 * k / blades
        d.arc([x-r+w, y-r+w, x+r-w, y+r-w], a0, a0 + 54,
              fill=(accent[0], accent[1], accent[2], 210), width=w)
    d.ellipse([x-4, y-4, x+4, y+4], fill=(body[0], body[1], body[2], 255))
    img.alpha_composite(lay)


def quad_pods(f, pal, col, b, t, pods, r, hub, bw, s=SS, accent=None):
    """Four (or two) rotor pods on booms from `hub`, drawn back-to-front."""
    accent = accent or pal.accent
    for px, py in pods:
        boom(f, hub, (px, py), scale_col(col, 0.45), bw, s=s)
    for px, py in pods:
        rotor(f, (px, py), r, scale_col(col, 0.8), scale_col(accent, b), b, t, s=s)


# ---------------------------------------------------------------------------
# Player drone (#8, D107) — a quad-rotor drone, not an arrow. Four spinning
# rotor pods on booms around a wedge chassis, sensor eye forward, twin thrusters
# aft. Symmetric about the horizontal axis so pure rotation orients it.
# ---------------------------------------------------------------------------
def player_frames(n=6, hull=None, accent=None, trim=None):
    """The modular chassis (D133). Slimmer and longer than the drone it replaces,
    with the rotor pods pushed further outboard on longer booms — the hull reads
    as a frame with space in it, and that space is where the upgrade kit lands.

    The two flank rails and the tail socket are drawn EMPTY here on purpose: the
    stock drone advertises its own hardpoints, so a purchase seats into a mount
    that was always there instead of appearing on bare hull.

    hull/accent/trim default to the CORE palette (the Standard drone). A ship
    whose catalogue colour is not cyan gets its OWN atlas from here rather than
    a runtime Tint: the art bakes cyan, and multiplying cyan by violet is blue,
    which is exactly why the Purple Gatling read as blue.
    """
    pal = CORE
    hull = hull or pal.primary
    accent = accent or pal.accent
    trim = trim or pal.secondary
    frames = []
    cx, cy = S/2, S/2
    for i in range(n):
        f = frame()
        b = pulse(i / n)
        t = i / n
        pods = [(cx+30, cy-33), (cx+30, cy+33), (cx-30, cy-33), (cx-30, cy+33)]
        quad_pods(f, pal, hull, b, t, pods, 15, (cx+2, cy), 6, accent=accent)
        chassis = [(cx+48, cy), (cx+20, cy-13), (cx-34, cy-11),
                   (cx-34, cy+11), (cx+20, cy+13)]
        neon_poly(f, chassis, scale_col(hull, 0.7),
                  outline=scale_col(accent, b), ow=3)
        d = draw(f)
        d.line([(cx-26, cy-6), (cx+22, cy-6)],
               fill=scale_col(trim, 0.8) + (255,), width=2)
        d.line([(cx-26, cy+6), (cx+22, cy+6)],
               fill=scale_col(trim, 0.8) + (255,), width=2)
        # empty hardpoints: two flank rails + the tail socket
        for sy in (-1, 1):
            d.line([(cx-13, cy + sy*15), (cx+13, cy + sy*17)],
                   fill=KIT_STEEL + (110,), width=2)
        d.rectangle([cx-46, cy-8, cx-36, cy+8], outline=KIT_STEEL + (110,), width=2)
        # sensor eye + aft thrusters
        dot(f, (cx+20, cy), 8, scale_col(accent, b))
        dot(f, (cx+20, cy), 4, (255, 255, 255))
        for ey in (-6, 6):
            dot(f, (cx-34, cy+ey), 4, scale_col(trim, b))
        # A tighter halo than the drone this replaces (0.14/150): the slimmer
        # hull leaves more empty frame, and at that spread the four pods' halos
        # merged into a haze covering the whole tile — which reads on a dark
        # floor as a lit square rotating with the ship.
        f = add_halo(shrink(f), hull, spread=0.085, strength=120)
        frames.append(f)
    return frames


# ---------------------------------------------------------------------------
# The upgrade kit (D133) — one overlay per shop upgrade row, authored in the
# SAME 128-space as the chassis so it composites 1:1 at any size. Every part is
# mirrored about the horizontal axis (art faces right and rotates, never flips)
# and owns one longitudinal station, so all seven can be worn at once:
#
#   muzzle   +40..+62   coils / long barrel
#   collar   +32..+42   heavy collar (wraps whatever barrel is fitted)
#   nose-out  y +/-15   twin barrels
#   flank    -14..+15   hull plating (the authored rails)
#   spine    -26..-14   heavy drums
#   tail     -48..-34   overclock heat-sink (the authored socket)
#   tail-out  y +/-17   aux nozzles
#
# Shield Capacitor is NOT here: shields have live state, so the shield is the
# animated field ring in shield_frames().
# ---------------------------------------------------------------------------
KIT_HOT = (255, 226, 150)    # bolt-on accent: warm gold, never reads as hull
KIT_STEEL = (150, 168, 186)  # bolt-on body: neutral against the cyan hull


def kit_plating():
    """HULL PLATING — armour slabs seated on the flank rails."""
    f = frame()
    cx, cy = S/2, S/2
    for sy in (-1, 1):
        pts = [(cx-14, cy + sy*15), (cx+12, cy + sy*20),
               (cx+15, cy + sy*14), (cx-14, cy + sy*11)]
        neon_poly(f, pts, scale_col(KIT_STEEL, 0.55), outline=KIT_HOT, ow=2)
        for k in range(3):
            dot(f, (cx-8 + k*9, cy + sy*15), 2, KIT_HOT)
    return f


def kit_thruster():
    """AUX THRUSTER — nozzles in the tail corners, aft of everything else."""
    f = frame()
    cx, cy = S/2, S/2
    d = draw(f)
    for sy in (-1, 1):
        box = [cx-45, cy + sy*17 - 4, cx-33, cy + sy*17 + 4]
        d.rectangle(box, fill=scale_col(KIT_STEEL, 0.75) + (255,))
        d.rectangle(box, outline=KIT_HOT + (255,), width=2)
        dot(f, (cx-46, cy + sy*17), 5, CORE.accent)
        dot(f, (cx-49, cy + sy*17), 2, (255, 255, 255))
    return f


def kit_heatsink():
    """OVERCLOCK — the finned heat-sink dropped into the tail socket."""
    f = frame()
    cx, cy = S/2, S/2
    d = draw(f)
    d.rectangle([cx-48, cy-9, cx-34, cy+9],
                fill=scale_col(KIT_STEEL, 0.5) + (255,), outline=KIT_HOT + (255,), width=2)
    for k in range(5):
        y = cy - 7 + k*3.5
        d.line([(cx-47, y), (cx-35, y)], fill=KIT_HOT + (255,), width=2)
    for sy in (-1, 1):
        d.line([(cx-46, cy + sy*11), (cx-36, cy + sy*11)], fill=CORE.accent + (255,), width=3)
    return f


def kit_drums():
    """HEAVY ROUNDS — magazine drums on the spine + a collar at the barrel base.
    The collar is what lets this compose with Long Barrel and the coils."""
    f = frame()
    cx, cy = S/2, S/2
    d = draw(f)
    for sy in (-1, 1):
        d.ellipse([cx-26, cy + sy*8 - 6, cx-14, cy + sy*8 + 6],
                  fill=scale_col(KIT_STEEL, 0.6) + (255,), outline=KIT_HOT + (255,), width=2)
        dot(f, (cx-20, cy + sy*8), 2, KIT_HOT)
    d.rectangle([cx+32, cy-6, cx+42, cy+6],
                fill=scale_col(KIT_STEEL, 0.7) + (255,), outline=KIT_HOT + (255,), width=2)
    return f


def kit_twin():
    """TWIN BARREL — a second pair of barrels outboard, over the front booms."""
    f = frame()
    cx, cy = S/2, S/2
    d = draw(f)
    for sy in (-1, 1):
        box = [cx+14, cy + sy*15 - 3, cx+52, cy + sy*15 + 3]
        d.rectangle(box, fill=scale_col(CORE.primary, 0.75) + (255,))
        d.rectangle(box, outline=KIT_HOT + (255,), width=2)
        dot(f, (cx+53, cy + sy*15), 4, CORE.accent)
    return f


def kit_longbarrel():
    """LONG BARREL — the centre barrel, extended well past the nose."""
    f = frame()
    cx, cy = S/2, S/2
    d = draw(f)
    d.rectangle([cx+40, cy-4, cx+62, cy+4], fill=scale_col(CORE.primary, 0.8) + (255,))
    d.rectangle([cx+40, cy-4, cx+62, cy+4], outline=KIT_HOT + (255,), width=2)
    for sy in (-1, 1):
        d.line([(cx+44, cy + sy*4), (cx+36, cy + sy*9)], fill=KIT_HOT + (255,), width=2)
    dot(f, (cx+62, cy), 4, CORE.accent)
    return f


def kit_coils():
    """RICOCHET COILS — induction rings wrapping the muzzle, one per level."""
    f = frame()
    cx, cy = S/2, S/2
    d = draw(f)
    d.rectangle([cx+44, cy-3, cx+58, cy+3], fill=scale_col(KIT_STEEL, 0.5) + (255,))
    for k, x in enumerate((cx+44, cx+51, cx+58)):
        r = 9 - k
        d.ellipse([x-3, cy-r, x+3, cy+r], outline=KIT_HOT + (255,), width=2)
    dot(f, (cx+60, cy), 3, CORE.accent)
    return f


# Index-aligned with GameData.json's shop.upgrades rows. Index 1 (Shield
# Capacitor) has no static part — it is the field ring — so it is a hole here
# and kit_visuals.hpp maps around it.
KIT_PARTS = [
    (0, "kit_plating", kit_plating),
    (2, "kit_thruster", kit_thruster),
    (3, "kit_heatsink", kit_heatsink),
    (4, "kit_drums", kit_drums),
    (5, "kit_twin", kit_twin),
    (6, "kit_longbarrel", kit_longbarrel),
    (7, "kit_coils", kit_coils),
]


# ---------------------------------------------------------------------------
# The shield field (D134) — the Shield Capacitor is not a bolt-on part, because
# a shield HAS LIVE STATE and a static overlay cannot show it. It is a ring that
# hums around the drone, never touching it: radius 70 in the chassis's 128-space
# against a hull whose halo ends about 55, so there is a clear standoff gap all
# round. The frame is 192px so the r=70 ring has margin around the 128-wide
# chassis box; the runtime wears it at FIELD_SIZE_MULT (2.25x the player's size,
# which is what keeps the ring the same on-screen size as the old stretched art).
#
# ONE STRIP, NO CLIPS: the frame is chosen per-frame by a pure function of the
# shield's state (kit_visuals::shield_frame), not by an Animation component.
# That keeps four different behaviours — a loop, a one-shot, a static and a
# progress bar — as one indexable strip and one unit-testable picker.
#
#   0..7   hum     the living field, phase-looped
#   8..11  hit     impact bloom decaying, played from the hit bearing
#   12     down    broken: dead emitter stubs
#   13..20 regen   rebuilding, indexed by FRACTION not by time
# ---------------------------------------------------------------------------
SHIELD_FRAME = 192
SHIELD_R = 70.0               # ring radius in the chassis's 128-space
SHIELD_ICE = (120, 200, 255)  # the shield colour the HUD already uses

SHIELD_HUM_START, SHIELD_HUM_COUNT = 0, 8
SHIELD_HIT_START, SHIELD_HIT_COUNT = 8, 4
SHIELD_DOWN_FRAME = 12
SHIELD_REGEN_START, SHIELD_REGEN_COUNT = 13, 8
SHIELD_TOTAL = 21


def _shield_frame_img():
    return Image.new("RGBA", (SHIELD_FRAME * SS, SHIELD_FRAME * SS), (0, 0, 0, 0))


def _shield_scale():
    """Draw proxy scale. ONE 128-space unit is ONE frame pixel: the 192px frame
    exists to give the r=70 ring MARGIN around the 128-wide chassis box, so it
    must not be stretched to fit 128-space. (It used to be SS * SHIELD_FRAME / S,
    which scaled 128-space up to fill the frame — the window stayed 128 wide, the
    r=70 ring ran off all four edges and the bubble rendered as a square.)"""
    return SS


def _shield_ring(mode, t, frac=1.0, hit_ang=0.0):
    """One field frame. `t` is 0..1 phase; `frac` is regen progress."""
    img = _shield_frame_img()
    d = draw(img, _shield_scale())
    c = SHIELD_FRAME / 2                        # frame centre; 1 unit = 1 px
    hum = SHIELD_R + 1.6 * math.sin(t * 2 * math.pi)
    box = [c - hum, c - hum, c + hum, c + hum]

    if mode == "down":
        for k in range(6):
            a = k * 60 + 12
            d.arc(box, a, a + 18, fill=SHIELD_ICE + (44,), width=2)
        return shrink(img, SHIELD_FRAME)

    if mode == "regen":
        half = 180.0 * frac                     # rebuilds symmetrically from the nose
        alpha = int(90 + 60 * frac)
        d.arc(box, -half, half, fill=SHIELD_ICE + (alpha,), width=3)
        for sgn in (-1, 1):
            a = math.radians(sgn * half)
            x, y = c + hum * math.cos(a), c + hum * math.sin(a)
            dot(img, (x, y), 3, (255, 255, 255), 230, s=_shield_scale())
        return shrink(img, SHIELD_FRAME)

    d.arc(box, 0, 360, fill=SHIELD_ICE + (150,), width=3)          # membrane
    d.arc([box[0]+2, box[1]+2, box[2]-2, box[3]-2],
          0, 360, fill=SHIELD_ICE + (60,), width=6)                # outer skin
    # shimmer: arcs sweeping at different speeds and directions
    for k, (speed, length, w, al) in enumerate(
            [(1.0, 70, 4, 220), (-0.62, 40, 3, 180), (1.7, 24, 3, 140)]):
        a0 = (t * 360 * speed + k * 137) % 360
        d.arc(box, a0, a0 + length, fill=SHIELD_ICE + (al,), width=w)
    # lattice cells flickering in a travelling wave
    inner = [c - hum + 5, c - hum + 5, c + hum - 5, c + hum - 5]
    for j in range(12):
        ph = math.sin(2 * math.pi * (t * 2 + j * 0.23))
        al = int(28 + 80 * max(0.0, ph) ** 2)
        a = j * 30 + 15
        d.arc(inner, a, a + 14, fill=(170, 225, 255, al), width=2)

    if mode == "hit":
        al = int(255 * (1.0 - t))
        d.arc(box, hit_ang - 26, hit_ang + 26, fill=(255, 255, 255, al), width=6)
        a = math.radians(hit_ang)
        x, y = c + hum * math.cos(a), c + hum * math.sin(a)
        dot(img, (x, y), 6 + 10 * t, (255, 255, 255), al // 2, s=_shield_scale())
        for j in (-1, 0, 1):
            a2 = hit_ang + j * 30
            d.arc(inner, a2 - 8, a2 + 8, fill=(255, 255, 255, al), width=3)
    return shrink(img, SHIELD_FRAME)


def shield_frames():
    """The strip. Hit frames are drawn at bearing 0 (the nose): the runtime spins
    the whole ring entity to the impact bearing rather than baking 8 bearings."""
    fr = [_shield_ring("hum", i / SHIELD_HUM_COUNT) for i in range(SHIELD_HUM_COUNT)]
    fr += [_shield_ring("hit", i / SHIELD_HIT_COUNT) for i in range(SHIELD_HIT_COUNT)]
    fr += [_shield_ring("down", 0.0)]
    fr += [_shield_ring("regen", i / SHIELD_REGEN_COUNT,
                        frac=i / (SHIELD_REGEN_COUNT - 1))
           for i in range(SHIELD_REGEN_COUNT)]
    assert len(fr) == SHIELD_TOTAL, len(fr)
    return fr


# ---------------------------------------------------------------------------
# Generic enemy builder: a body-shape function + palette + n frames + death
# ---------------------------------------------------------------------------
def enemy_frames(shape_fn, pal, body_col, n=8):
    frames = []
    for i in range(n):
        f = frame()
        b = pulse(i / n)
        shape_fn(f, pal, body_col, b, i / n)
        f = add_halo(shrink(f), body_col, spread=0.16, strength=150)
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
        small = base.resize((int(W*sc), int(W*sc)))
        f.paste(small, (int((W-small.size[0])/2), int((W-small.size[1])/2)), small)
        # fade alpha
        alpha = f.split()[3].point(lambda v: int(v * (1 - t)))
        f.putalpha(alpha)
        if t < 0.6:  # early flash
            fl = frame(); dot(fl, (S/2, S/2), int(18 + 30*t), pal.accent, int(180*(1-t)))
            f = Image.alpha_composite(f, fl)
        f = add_halo(shrink(f), body_col, spread=0.16, strength=int(150*(1-t)))
        death.append(f)
    return frames, death


def spark_shape(f, pal, col, b, t):
    """Scout quad: a tiny four-rotor drone. Smallest silhouette in the game, so
    the body is barely more than a hub — the four rotors carry the read."""
    cx, cy = S/2, S/2
    pods = [(cx+21, cy-21), (cx+21, cy+21), (cx-21, cy-21), (cx-21, cy+21)]
    quad_pods(f, pal, col, b, t, pods, 12, (cx, cy), 5)
    r = 15
    # D186: the hub is a dart, not a diamond — a stretched +X nose so the scout
    # has a front face now that every enemy rotates to its heading.
    neon_poly(f, [(cx+r+10, cy), (cx, cy-r), (cx-r, cy), (cx, cy+r)],
              scale_col(col, 0.65), outline=scale_col(pal.accent, b), ow=3)
    dot(f, (cx, cy), 6, scale_col(pal.accent, b))
    dot(f, (cx+12, cy), 3, (255, 255, 255))


def runner_shape(f, pal, col, b, t):
    """Interceptor: a dart chassis with two swept-back rotor booms. Reads fast
    because the rotors trail the nose instead of bracketing it."""
    cx, cy = S/2, S/2
    pods = [(cx-10, cy-27), (cx-10, cy+27)]
    quad_pods(f, pal, col, b, t, pods, 14, (cx+6, cy), 6)
    neon_poly(f, [(cx+42, cy), (cx+4, cy-15), (cx-22, cy-9),
                  (cx-22, cy+9), (cx+4, cy+15)],
              scale_col(col, 0.6), outline=scale_col(pal.accent, b), ow=3)
    d = draw(f)
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
    d = draw(f)
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
    d = draw(f)
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
    lay = Image.new("RGBA", f.size, (0, 0, 0, 0))
    d = draw(lay)
    d.ellipse([cx-r, cy-r, cx+r, cy+r], fill=fill + (255,))
    d.ellipse([cx-r, cy-r, cx+r, cy+r], outline=outline + (255,), width=ow)
    # Inner rim: a ring hugging the OUTSIDE of the bite, so it survives the punch
    # and the terminator gets an edge as bright as the limb.
    g = br + ow * 0.5
    d.ellipse([cx+dx-g, cy-g, cx+dx+g, cy+g], outline=outline + (255,), width=ow)
    # Punch the shadowed lobe, and clip everything to the lit disc so no stray
    # rim escapes the silhouette.
    mask = Image.new("L", f.size, 0)
    md = draw(mask)
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
            rotor(f, (px, py), 54, scale_col(col, 0.55), rim, 1.0, 0.12, blades=4, w=7, s=1)
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
    dot(f, (cx+72, cy), 20, rim, s=1)
    dot(f, (cx+72, cy), 10, (255, 255, 255), s=1)

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
        dot(f, (cx-230, cy+ey), 12, rim, 230, s=1)
        dot(f, (cx-236, cy+ey), 6, (255, 255, 255), s=1)

    # --- masts ---
    for sy in (-1, 1):
        d.line([(cx+20, cy + sy*72), (cx+4, cy + sy*118)], fill=plate + (255,), width=5)
        dot(f, (cx+4, cy + sy*118), 7, rim, s=1)

    f = f.resize((CO, CO), Image.BOX)
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
    d = draw(f, 1)
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
    d = draw(f, 1)
    c = P/2
    crest = [(c, c-34), (c+28, c-18), (c+28, c+4), (c, c+34), (c-28, c+4), (c-28, c-18)]
    d.polygon(crest, fill=body + (255,))
    d.line(crest + [crest[0]], fill=edge + (255,), width=4)
    d.line([(c-15, c-4), (c, c+10), (c+15, c-4)], fill=(235, 248, 255, 255), width=5)
    for sx in (-1, 1):
        d.rectangle([c + sx*28 - 4, c-8, c + sx*28 + 4, c+2], fill=edge + (255,))
    return add_halo(f, (120, 200, 255), spread=0.16, strength=170)


def coin_sprite():
    """Currency: a UNIT — a hex data-chit, not a struck coin. Dark substrate,
    hard amber hex rim, four circuit traces out to the corners and a stacked
    bar glyph in the middle, so it reads as data rather than as metal. Keeps the
    gold hue (255,210,90) so nothing downstream — HUD colour, Color fallback,
    pickup tint — has to move; only the silhouette changed."""
    f = _pickup_frame()
    d = draw(f, 1)
    c = P/2
    R = 33
    hexa = [(c + R*math.cos(math.pi/3*k), c + R*math.sin(math.pi/3*k)) for k in range(6)]
    # circuit traces: stubs running off the chit's flats, drawn first so the
    # rim overdraws where they meet it.
    for k in range(6):
        x, y = hexa[k]
        d.line([(c + (x-c)*0.55, c + (y-c)*0.55), (x*1.10 - c*0.10, y*1.10 - c*0.10)],
               fill=(255, 180, 50, 200), width=3)
        d.ellipse([x*1.10 - c*0.10 - 3, y*1.10 - c*0.10 - 3,
                   x*1.10 - c*0.10 + 3, y*1.10 - c*0.10 + 3], fill=(255, 225, 140, 230))
    d.polygon(hexa, fill=(46, 34, 12, 255))
    d.line(hexa + [hexa[0]], fill=(255, 210, 90, 255), width=5)
    # the unit glyph: two stacked bars through a vertical stem (a "digital U")
    d.line([(c, c-16), (c, c+16)], fill=(255, 245, 200, 255), width=5)
    for gy in (-8, 8):
        d.line([(c-13, c+gy), (c+13, c+gy)], fill=(255, 235, 160, 255), width=4)
    return add_halo(f, (255, 210, 90), spread=0.14, strength=150)


def blast_cloud():
    """Bomb detonation (#5): a small mushroom cloud with a red outline. Cap +
    stem + a ground collar, each drawn as a red silhouette with a hot core on
    top — so the rim IS the outline rather than a stroke that has to follow a
    lumpy shape. Replaces the flat orange rect the mine blast used to be."""
    f = _pickup_frame()
    d = draw(f, 1)
    c = P/2
    # One silhouette, four overlapping parts — an actual mushroom, not a string
    # of beads. ("ell"/"rr", box, radius); boxes are centre-relative.
    parts = [("ell", (-17, -46, 17, -26), 0),    # rising head
             ("ell", (-31, -36, 31, -8), 0),     # cap
             ("rr", (-10, -14, 10, 28), 9),      # stem
             ("ell", (-33, 22, 33, 41), 0)]      # ground collar

    def silhouette(pad, col):
        for kind, (x0, y0, x1, y1), rad in parts:
            box = [c+x0-pad, c+y0-pad, c+x1+pad, c+y1+pad]
            if box[2] - box[0] < 2 or box[3] - box[1] < 2:
                continue
            if kind == "ell":
                d.ellipse(box, fill=col)
            else:
                d.rounded_rectangle(box, radius=max(1, rad + pad), fill=col)

    silhouette(3, (235, 45, 30, 245))     # the red outline
    silhouette(0, (128, 22, 16, 240))     # scorched body
    silhouette(-7, (255, 150, 45, 225))   # hot core
    silhouette(-13, (255, 235, 190, 230))  # flash
    return add_halo(f, (255, 80, 40), spread=0.16, strength=160)


def poison_cloud():
    """Bio-lab poison patch (D187, retuned): drifting gas, not a flower.

    The first version drew six equal lobes under a 5px opaque red rim, which
    outlined every lobe individually — six petals with a red edge. Now: a dozen
    unequal lobes so the silhouette has no repeat period, the whole thing blurred
    so the edge is vapour rather than a stroke, and the red is a THIN low-alpha
    haze under the body (danger tint, not a border). No bubbles — they read as
    the wrong gas. Fixed layout, no randomness."""
    f = _pickup_frame()
    c = P/2
    # (dx, dy, r) — deliberately uneven: three big masses, the rest wisps.
    lobes = [(-4, 2, 28), (14, -6, 22), (-16, -10, 19), (6, 17, 17),
             (-19, 11, 14), (20, 12, 11), (-2, -22, 15), (25, -17, 9),
             (-27, -2, 10), (11, 27, 9), (-13, 24, 8), (28, 4, 7)]

    def lay(pad, col, alpha):
        lyr = Image.new("RGBA", (P, P), (0, 0, 0, 0))
        dd = draw(lyr, 1)
        for dx, dy, r in lobes:
            dd.ellipse([c+dx-r-pad, c+dy-r-pad, c+dx+r+pad, c+dy+r+pad],
                       fill=col + (alpha,))
        return lyr

    # Thin red haze first (2px of dilation, ~40% alpha), then the gas over it.
    f.alpha_composite(lay(2, (215, 55, 40), 105).filter(ImageFilter.GaussianBlur(2.2)))
    f.alpha_composite(lay(0, (64, 132, 48), 190).filter(ImageFilter.GaussianBlur(2.6)))
    inner = Image.new("RGBA", (P, P), (0, 0, 0, 0))
    di = draw(inner, 1)
    for dx, dy, r in lobes:
        di.ellipse([c+dx-r*0.5, c+dy-r*0.5, c+dx+r*0.5, c+dy+r*0.5],
                   fill=(126, 224, 96, 150))
    f.alpha_composite(inner.filter(ImageFilter.GaussianBlur(4.0)))
    return add_halo(f, (120, 235, 90), spread=0.18, strength=110)


def mine_sprite():
    """Foundry mine: the bomb-emoji read — round dark body, cap, curved fuse, lit
    spark. The body is dark so the hot orange rim and the spark carry it against
    a near-black arena."""
    f = _pickup_frame()
    d = draw(f, 1)
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
        f = add_halo(shrink(f), pal.primary, spread=0.22, strength=180)
        frames.append(f)
    return frames


def bolt_frames(n=4):
    pal = FOUNDRY
    frames = []
    for i in range(n):
        f = frame()
        b = pulse(i / n)
        cx, cy = S/2, S/2
        # elongated dart facing right
        pts = [(cx+30, cy), (cx-18, cy-9), (cx-10, cy), (cx-18, cy+9)]
        neon_poly(f, pts, scale_col(pal.primary, 0.8), outline=scale_col(pal.accent, b), ow=2)
        dot(f, (cx+12, cy), 4, (255, 255, 255))
        f = add_halo(shrink(f), pal.primary, spread=0.2, strength=170)
        frames.append(f)
    return frames


ROCKET_BODY = (196, 206, 218)   # steel casing: reads as hardware, not as plasma
ROCKET_HOT = (255, 190, 90)     # the missile's existing Color and trail hue


def rocket_sprite():
    """HEAT-SEEKING MISSILE — an actual rocket, facing right like every other v2
    projectile: warhead cone, steel casing with a warning band, swept tail fins
    and a lit nozzle. Single frame: the runtime rotates it to the heading and the
    trail emitter already supplies the motion, so animating the casing is waste."""
    f = frame()
    cx, cy = S/2, S/2
    # tail fins first, so the casing overdraws where they meet it
    for sy in (-1, 1):
        neon_poly(f, [(cx-30, cy + sy*7), (cx-14, cy + sy*7),
                      (cx-16, cy + sy*22), (cx-34, cy + sy*18)],
                  scale_col(ROCKET_BODY, 0.45), outline=ROCKET_HOT, ow=2)
    # casing + warhead cone
    neon_poly(f, [(cx-30, cy-10), (cx+12, cy-10), (cx+12, cy+10), (cx-30, cy+10)],
              scale_col(ROCKET_BODY, 0.7), outline=scale_col(ROCKET_BODY, 1.0), ow=2)
    neon_poly(f, [(cx+12, cy-10), (cx+34, cy), (cx+12, cy+10)],
              ROCKET_HOT, outline=(255, 240, 200), ow=2)
    # warning band + seeker eye
    neon_poly(f, [(cx-4, cy-10), (cx+3, cy-10), (cx+3, cy+10), (cx-4, cy+10)],
              scale_col(ROCKET_HOT, 0.8), ow=0)
    dot(f, (cx+18, cy), 3, (255, 255, 255))
    # nozzle flare
    dot(f, (cx-31, cy), 7, ROCKET_HOT, 220)
    dot(f, (cx-34, cy), 4, (255, 255, 255), 230)
    return add_halo(shrink(f), ROCKET_HOT, spread=0.2, strength=170)


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
        d = draw(f)
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
        f = shrink(f.filter(ImageFilter.GaussianBlur(1.2 * SS)))
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
        f = shrink(f.filter(ImageFilter.GaussianBlur(0.8 * SS)))
        frames.append(f)
    return frames


# ---------------------------------------------------------------------------
# HUD ability row (playtest #11)
#
# UIElement has no texture path and adding one is an engine change, so the dash
# button's FACE is two screen-space sprites drawn over the authored 48x48 frame
# — see main.cpp's dash-face block. Both are authored upright: a HUD glyph is
# never rotated, unlike rocket_sprite() which faces right because the runtime
# spins it.
# ---------------------------------------------------------------------------
BOOST_HOT = (255, 180, 80)


def boost_icon():
    """The booster: a rocket standing on its plume, nose UP."""
    f = frame()
    cx = S / 2
    hull, trim = CORE.primary, CORE.accent
    # plume first, so the nozzle overdraws where it meets the casing
    neon_poly(f, [(cx - 10, 76), (cx + 10, 76), (cx + 16, 112), (cx, 100),
                  (cx - 16, 112)],
              scale_col(BOOST_HOT, 0.5), outline=BOOST_HOT, ow=3)
    # swept fins
    for sx in (-1, 1):
        neon_poly(f, [(cx + sx * 11, 48), (cx + sx * 11, 78), (cx + sx * 28, 84),
                      (cx + sx * 22, 58)],
                  scale_col(hull, 0.4), outline=hull, ow=3)
    # casing + nose cone
    neon_poly(f, [(cx - 13, 78), (cx - 13, 40), (cx, 14), (cx + 13, 40),
                  (cx + 13, 78)],
              scale_col(hull, 0.55), outline=trim, ow=3)
    dot(f, (cx, 46), 7, (255, 255, 255), 235)          # porthole
    dot(f, (cx, 82), 6, BOOST_HOT, 230)                # lit nozzle
    return add_halo(shrink(f), hull, spread=0.16, strength=150)


SWEEP_N = 16          # frames; must equal DASH_SWEEP_FRAMES in dash_system.hpp
SWEEP_FRAME = 64      # output frame edge


def sweep_frames(n=SWEEP_N):
    """The dash cooldown as a clock wipe over the WHOLE button.

    Frame i is progress i/n: the not-yet-recharged wedge stays greyed out and a
    lit hand sweeps clockwise from 12 o'clock until the box is clear. The pie
    radius is larger than the frame's half-diagonal, so the corners grey out too
    and the frame edge does the clipping — the box IS the dial, which is the
    whole point of the note (a horizontal bar was the version that got rejected).
    """
    out = []
    w = SWEEP_FRAME * SS
    c = w / 2.0
    r = w                                   # > half-diagonal; clipped to the square
    box = [c - r, c - r, c + r, c + r]
    for i in range(n):
        img = Image.new("RGBA", (w, w), (0, 0, 0, 0))
        d = ImageDraw.Draw(img)             # already at working size: no ScaledDraw
        a0 = -90.0 + 360.0 * i / n          # hand angle, clockwise from 12 o'clock
        d.pieslice(box, a0, 270.0, fill=(6, 10, 22, 210))
        ang = math.radians(a0)
        d.line([(c, c), (c + r * math.cos(ang), c + r * math.sin(ang))],
               fill=CORE.primary + (215,), width=2 * SS)
        out.append(shrink(img, SWEEP_FRAME))
    return out


def main():
    print("make_sprites:")
    # Player
    march_clip = {"march": {"start_frame": 0, "frame_count": 6,
                            "frame_duration": 0.09, "looping": True}}
    write_sprite("player_drone", player_frames(6), 3, march_clip)
    # #2: the Purple Gatling's own atlas. Its catalogue colour is violet but it
    # wore the cyan chassis, so the ship the menu calls purple flew in blue.
    write_sprite("player_drone_violet",
                 player_frames(6, hull=(180, 110, 255), accent=(232, 210, 255),
                               trim=(90, 235, 255)), 3, march_clip)
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
    # The active item's missile. Single frame worn as Images by actives::
    # launch_missiles, so no sidecar — see save_png block below.
    save_png("projectile_rocket", rocket_sprite())
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
    save_png("hazard_poison", poison_cloud())
    save_png("hazard_blast", blast_cloud())
    # D105: the boss's carrier. Single frame, worn as Images by BossSystem.
    save_png("enemy_boss_carrier", carrier_sprite())
    # D133: the upgrade kit. Single-frame overlays worn as Images by the kit
    # followers, authored in the chassis's own 128-space so they composite 1:1.
    # No halo on a kit part: it is composited ON TOP of a chassis that already
    # bakes one, and seven stacked halos put a visible square of haze around the
    # drone (the frame edge clips the blur, and the box then rotates with the
    # hull). The parts read from their own bright outlines.
    for _row, name, fn in KIT_PARTS:
        save_png(name, shrink(fn()))
    # D134: the shield field strip. A SpriteSheet, but with no Animation —
    # kit_visuals::shield_frame picks the frame from the shield's state.
    write_sprite("shield_field", shield_frames(), 7,
                 {"hum": {"start_frame": SHIELD_HUM_START,
                          "frame_count": SHIELD_HUM_COUNT,
                          "frame_duration": 0.08, "looping": True}})
    # Playtest #11: the dash button's face. Same "strip + picker, no Animation"
    # arrangement as the shield field — dash_sweep_frame() indexes it by the
    # recharge FRACTION, which is not a clip.
    save_png("hud_boost", boost_icon())
    write_sprite("hud_dash_sweep", sweep_frames(), 4, {})


if __name__ == "__main__":
    main()
