# Feature Spec: Shop as a real menu + gear upgrades

## Status

In progress (Lane C, iteration 3 Phase 5 — notes #1 and #11)

## User Story

As a player, I want the shop to be a real clickable menu with hover tooltips and
a preview of my drone as it will look equipped, so that buying is a considered
choice instead of reading a text overlay and pressing a number.

## Requirements

1. A `shop` screen authored in `GameData.json` (800x600 design canvas): one
   clickable **card** per catalogue row, three page tabs, a LEAVE button.
2. `ShopSystem` resolves those widgets by name through `ui.widget_id.<name>`
   (the `GameHUDSystem` pattern) and consumes clicks off
   `UISystem::UI_CLICK_KEY`, removing the key after handling.
3. Hovering a card rewrites a hidden tooltip label pair (name + detail) and
   moves it beside the hovered card.
4. A drone preview shows the hull plus the hovered item's aura; with nothing
   hovered it shows the currently equipped loadout.
5. **Gear levels (#11):** the fitted passive item can be levelled. Price is
   `base * price_growth^level`; a level raises the item's published
   `ship.item_amount` by a fixed step. Only fitted gear is listed.
6. The `1`-`8` / `TAB` / `B` keyboard path keeps working unchanged, including on
   the new UPGRADE page, so existing headless scripts and tests still drive it.
7. No engine change, no new component type, no CMake edit.

## Acceptance Criteria

1. Given the shop is open, when the player clicks a card, then the same purchase
   happens as pressing that row's digit, and `UI_CLICK_KEY` is cleared.
2. Given the pointer is over card *n*, when the frame runs, then the tooltip
   labels carry that catalogue row's name and detail and sit beside that card.
3. Given the pointer leaves every card, then the tooltip collapses (empty text,
   zero-size panel) and the preview falls back to the equipped loadout.
4. Given no item is fitted, when the player opens the UPGRADE page and clicks
   row 0, then nothing is bought, currency is unchanged and a message says so.
5. Given a Repulsor Field at level 2 with `price_growth 1.5` and base 120, then
   the next level costs `round(120 * 1.5^2) = 270`.
6. Given `GameData.json` carries no `shop` screen, then the shop still opens and
   behaves exactly as the pre-Lane-C text list (graceful fallback).
7. The replay canary is byte-identical across two runs.

## Out of Scope

- Any change to the catalogue rows themselves (names, effects, prices).
- Consumable levelling: nothing reads a per-run consumable amount, so a level
  there would be a dead number (see D62).
- Raising the intermission to *every* wave — that cadence line is Lane A's.

## Affected Boundaries

- `CPP/game/shop_system.{hpp,cpp}` (owner)
- `assets/GameData.json` -> `screens.shop` only
- `main.cpp` -> the `// === HOOK: shop-menu ===` block only

## Task Breakdown

1. Author `screens.shop` (panel, title, credits, 3 tabs, 8 cards, LEAVE,
   tooltip panel + 2 labels, preview caption).
2. `ShopSystem::menu_tick` — resolve ids, push/pop the screen, route clicks,
   refresh cards/tooltip/preview.
3. Gear levels: `gear_price`, `owns_gear`, `upgrade_gear`, page 2.
4. Tests: contract test on the screen, price math, unowned rejection, tooltip
   follows hover.

## Open Questions

- None blocking. Flagged for the integrator: the wave intermission is still
  raised only on the `% 5 == 0` stop, so the UPGRADE page is only reachable
  there today. When Lane A/integration raises an intermission every wave, the
  page is already built and needs no further work here.
