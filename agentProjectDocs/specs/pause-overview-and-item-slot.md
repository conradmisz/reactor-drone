# Feature Spec: Pause overview, active-item slot, minimap placement

## Status

Done (Lane M, iteration 5 — items #2, #5, #13, #4)

## User Story

As a player I want the pause screen to be readable and to tell me what my drone
actually is right now, an on-screen reminder of the active item a boss gave me
and the key that fires it, and a minimap that sits square in the corner with the
health packs visible on it.

## Requirements

1. **#2** No two widgets on the `pause` screen overlap. The regression is
   diagnosed, not nudged: the cause is named in `decisions.md`, and a test fails
   if any future widget is dropped on top of another.
2. **#5** The pause screen lists hull, shield, speed, fire rate and damage, one
   line per *purchased* upgrade with its cumulative effect, and the equipped
   item / consumable / active.
3. **#13** A square slot in the bottom-left of the HUD names the boss-reward
   active, with its key and cooldown printed along the bottom of the square.
   Hidden when no active is held, and it follows the same phase visibility rule
   as the rest of the HUD.
4. **#4** The minimap's top and right margins are equal *in window pixels*, and
   health packs draw as green blips.

## Acceptance Criteria

1. Given the shipped `GameData.json`, when the `pause` screen's authored rects
   and the code-placed stat-line rects are compared pairwise, then no two
   intersect (`test_pause_screen.cpp`).
2. Given a drone with 2 Hull Platings and a Magnet Core, when the stat lines are
   built, then they contain `Hull Plating x2` with `+50 hull` and a `GEAR` line
   naming Magnet Core.
3. Given no active item, when the HUD updates, then all three item-slot widgets
   have zero-size rects; given one, the key line reads `[E]` (or `AUTO` for the
   repulsion device, which is not on a key) plus `READY` or the seconds left.
4. Given the shipped config, when the minimap rect is mapped through
   `ui_canvas_transform` at 980x660, then the top and right margins agree to
   within a pixel (`test_minimap.cpp`).
5. Given a Health pickup, when the minimap builds its blips, then that blip's
   style is `minimap_health`; a Currency pickup keeps `minimap_pickup`.
6. Edge case: 8 upgrades bought and all three gear slots filled must still fit
   the 16 line slots.

## Out of Scope

- Prestige bonuses (Lane O owns the state; see Open Questions).
- Icon art for the item slot — there is none in this project, so the slot is a
  short text tag.
- Any change to `fit_text_in_rect`; it was not the bug.

## Affected Boundaries

- `assets/GameData.json` → `screens.pause`, `screens.gameplay`, `ui_styles`,
  `minimap`
- `CPP/game/pause_stats.{hpp,cpp}` (new), `minimap_system.cpp`, one `main.cpp`
  hook block

## Task Breakdown

1. Screenshot the current pause screen and name the root cause.
2. Re-author the pause screen on the D88 grid with room for the stat block.
3. `pause_stats.hpp` — pure `stat_lines()` + a widget pool on the `pause` screen.
4. Item-slot widgets on the `gameplay` screen, driven from the same system.
5. Minimap x + `minimap_health` style + the blip-kind branch.
6. Tests, gate, decisions.

## Open Questions

- **Prestige bonuses (#5's last clause) are not reachable.** Lane O has not
  landed, so there is no accessor and no meta field to read. `stat_lines()`
  takes a `prestige` argument and emits a `PRESTIGE +N%` line when it is > 0;
  `main.cpp` passes `blackboard.get_or<int>("meta.prestige_level", 0)`, which is
  0 today. Lane O sets that key and the line appears — no edit to either lane's
  files.
