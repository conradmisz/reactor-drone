# Feature Spec: Arena minimap (Game Note #7)

## Status

Done — Lane B, iteration 3 (D58). Unit-tested and headless-verified; not playtested.

## User Story

As a pilot in a 2800-unit-wide arena with a follow camera, I want a small overview
of where everything is so that I know which way the swarm is coming from and
where the loot I left behind is.

## Requirements

1. A square frame in the HUD, positioned and sized from the `minimap` config block
   in the 800x600 design canvas.
2. Blips for the player (cyan), enemies (red), the boss (larger red) and
   pickups/coins (gold).
3. The arena circle maps onto the frame preserving bearing; anything outside the
   arena clamps to the rim, and no blip is ever drawn outside the frame.
4. Blips are a **pool** allocated once and repositioned per frame — no per-frame
   entity creation or destruction.
5. The blip count is capped at `minimap.max_blips`. When the cap bites it is
   **logged**, never dropped silently, and the player and the boss are never the
   blips that get dropped.
6. `minimap.enabled: false` allocates nothing and draws nothing.

## Acceptance Criteria

1. Given a body at the arena centre, its blip is at the frame centre; given one
   half-way to the east wall, its blip is half-way to the frame's east edge.
2. Given a body 9000 units north-east of the centre, its blip sits on the 45°
   rim at the frame's inscribed radius, fully inside the frame.
3. Given 720 bearings x six radial scales including far outside the arena, every
   resulting rect is contained by the frame.
4. Given a zero arena radius or a blip wider than the frame, the result is the
   frame centre — no division by zero, no inverted mapping.
5. Given `max_blips: 8` and 40 enemies, exactly 8 blips are drawn, the overflow is
   logged once, and 30 further frames create and destroy no entities.
6. Given `max_blips: 3` with a player, a boss and 20 enemies, the player blip and
   the boss blip are both present.
7. Given `enabled: false`, the pool is empty after 10 frames.

## Out of Scope

- Zoom, panning, or a fog-of-war rule — the whole arena is always shown.
- Blips for obstacles, hazards or projectiles.
- A minimap on the title/game-over screens (it lives on `gameplay`, which is
  always active during a run).

## Affected Boundaries

- `CPP/game/minimap_math.hpp` and `minimap_system.{hpp,cpp}` (new), the `minimap`
  hook in `main.cpp`, the `minimap` block plus three `ui_styles` and one
  `gameplay`-screen panel in `GameData.json`.

## Task Breakdown

1. `minimap_math.hpp` — the pure mapping with rim clamping.
2. `MinimapSystem` — pool allocation, priority collection, cap logging.
3. `GameData.json`: enable the block, add the frame panel and blip styles.
4. `test_minimap.cpp`; delete the "inert" line in `test_scaffolding.cpp`.

## Open Questions

None. One assumption in the plan turned out to be **wrong** and is recorded in
D58: blips cannot be `ScreenPosition`-only world entities, because nothing draws
those. They are UI widgets instead.
