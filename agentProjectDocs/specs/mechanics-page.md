# Feature Spec: Mechanics page

## Status

Scoped (not started).

## User Story

As a player, I want an in-game "mechanics breakdown" screen reachable from the
main menu so I can learn how waves, bosses, moons, dash, the shop and prestige
actually work without alt-tabbing to the out-of-game field manual.

## Requirements

1. A **MECHANICS** button on the main menu opens a new `mechanics` screen; the
   main_menu button block is re-laid-out in JSON to fit a 7th button (the panel
   is currently full).
2. The screen follows the `how_to_play` shape: panel + title + rule +
   ~12-16 caption labels + BACK, all data in `GameData.json → screens` on the
   800×600 canvas.
3. Content is condensed from `docs/features.html` (the D135 field manual):
   waves/bosses, moons, dash, shop, prestige. Condensed — not a dump.
4. If the content overflows one screen, split into two sub-screens with
   NEXT/PREV buttons swapping via `CMD_CLEAR_TO`.
5. BACK reuses the shared `on_back_click` handler; ESC also returns to the
   menu — the screen name goes into the hardcoded ESC list in `main.cpp`
   (~1156-1165) and gets one routing branch in the menu click chain
   (~1845-1969).
6. New widget names are added to the menu-screen pin test.

## Acceptance Criteria

1. Given the main menu, clicking MECHANICS (mouse or `--clicks`) shows the
   mechanics screen; BACK and ESC both return to the menu.
2. Given a two-sub-screen layout, NEXT/PREV swap pages via CMD_CLEAR_TO and
   BACK/ESC work from either page.
3. Given the existing menu pin test extended with the new widget names, the
   full ctest suite passes and the replay canary is byte-identical twice
   (menu-only change — the sim is untouched).
4. Given a screen at 800×600, no label clips or overlaps the panel.

## Out of Scope

- Any new widget type or engine (`CPP/engine/`) change.
- Interactive/animated demos; images; per-mechanic detail beyond a caption line.
- Rewriting `docs/features.html` or the how_to_play screen.
- In-run access (pause menu) — menu-only.

## Affected Boundaries

- `assets/GameData.json` (new screen(s), main_menu button re-layout)
- `CPP/game/main.cpp` (screen constant, one click-fn branch, ESC list entry)
- `CPP/game/tests/unit/` menu screen pin test (new widget names)

## Task Breakdown

1. Condense `docs/features.html` into the caption-label copy; decide one page
   vs two.
2. Add the screen(s) + re-laid-out main_menu buttons to `GameData.json`.
3. Wire main.cpp: screen constant, MECHANICS click branch, ESC list.
4. Extend the pin test; gate-green (warning-free build, ctest, canary ×2) and
   a headless `--clicks` walk-through.

Estimate: ~half a day; near-zero new C++.

## Open Questions

- One page or two? Resolve while condensing the copy in task 1 — the split
  mechanism is already scoped either way.
