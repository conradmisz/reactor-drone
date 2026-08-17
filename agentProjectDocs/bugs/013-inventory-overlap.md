---
id: 013
title: Inventory PROJECTILE COLOR row draws under the BACK button
status: resolved
severity: medium
area: ui
opened: 2026-08-16
resolved: 2026-08-16
---

## Symptom

On the INVENTORY screen the PROJECTILE COLOR row and the BACK button overlap —
BACK sits on top of the row's right end, hiding "(click to cycle)" and making
the two click targets ambiguous. Screenshot: playtest #2, `~/Pictures/overlap.png`.

## Reproduce

1. Main menu → PLAY → INVENTORY.
2. Look at the bottom of the panel.

Deterministic — the rects are authored: `inv_proj_color` {224,100,352,38}
spans x224-576 / y100-138; `inv_back` {480,92,96,36} spans x480-576 / y92-128.
They intersect at x480-576, y100-128.

## Ruled Out

- **Tested:** checked whether UIRenderSystem does any layout/collision
  avoidance. **Observed:** it draws authored rects verbatim (D85 only shrinks
  text). **Eliminates:** runtime cause — this is authored-data error.

## Suspects

1. **Authored rects collide** — confirmed by arithmetic above.

## Resolution

The tier-7 inventory screen was authored with the BACK button inside the
projectile row's band and nothing checked for it. Fixed by the playtest #2
(D228) inventory rebuild: the screen is now a grid of cells with BACK in its
own band, and `test_screen_layout.cpp` fails the build if any two widgets on
one screen partially overlap (containment = panels is allowed), so every
screen is checked from now on, not just this one. Verified: the new layout
passes the test and a screenshot of the rebuilt screen shows no overlap.
