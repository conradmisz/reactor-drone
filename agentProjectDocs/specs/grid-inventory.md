# Feature Spec: Grid Inventory

## Status

Done (playtest #2, D228)

## User Story

As a pilot, I want the inventory to be a grid of selectable item cells (the
MMO-bag pattern) so that equipping a weapon or a paint is one click on the
thing itself, not cycling a text row.

## Requirements

1. The INVENTORY screen shows four cell rows: WEAPONS (4 cells), SHIP COLOR,
   TRAIL COLOR, PROJECTILE COLOR (1 default cell + one per catalogue colour).
2. Clicking an owned cell equips it immediately and persists to meta.json.
   Ship/trail colours apply to the selected ship; projectile colour to the
   next run's weapon. Slot cell 0 = "the item's own default" (clears the slot).
3. The equipped cell reads as selected (UIState.disabled + `shop_tab`, the D88
   tab convention) and is unclickable; unowned cells render dim (`card` +
   disabled) and clicks on them do nothing.
4. No two widgets on the screen may partially overlap (bug 013).

## Acceptance Criteria

1. Given Gryphon selected and Forest owned, when SHIP COLOR → FOREST is
   clicked, then meta.json ship_colors.Gryphon == "Forest" and the hangar
   preview reskins that frame.
2. Given only Falcon owned, when the WEAPONS row renders, then Moonshot/Flak/
   Hailstorm cells are dim and clicking them changes nothing.
3. Given any screen in GameData.json, when test_screen_layout runs, then no
   two widgets on one screen partially overlap (containment allowed).

## Out of Scope

- Icons in cells (UIElement carries no texture — cells are text buttons).
- Drag-and-drop, stacking, bag tabs.

## Affected Boundaries

- assets/GameData.json → screens.inventory (data-authored screen, D61).
- main.cpp refresh_inventory + inventory click handlers; cycle_color_slot
  deleted (grid selects directly).
- New game unit test test_screen_layout.cpp.
