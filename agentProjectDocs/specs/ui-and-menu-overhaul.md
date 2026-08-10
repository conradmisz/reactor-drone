# Feature Spec: UI and menu overhaul (Lane H)

## Status

Done — verified by unit tests, a headless replay canary, and four screenshots
read back at the pixel level.

## User Story

As a player, I want menus whose text stays inside its box, a HUD that only
appears when I am flying, and a radar I can ignore, so that the game reads like a
finished game instead of a debug overlay.

## Requirements

1. No label or button caption may draw outside its widget rect, on any screen,
   now or after any future edit. (D85)
2. The arena HUD — gauges, readouts, minimap — must not draw on the title screen,
   under the shop panel, or on the game-over/victory screens. (D86)
3. The `HULL` label must be fully on screen. (D85 + rect re-authoring)
4. The minimap is smaller and reads as a radar pane rather than a black box. (#1)
5. The LEVELS page states what a level buys. (#5)
6. `"arena shift"` becomes `"REACTOR SHIFT"` everywhere it is visible. (#13)
7. Menu design research is applied and written down. (#10 — below)

## Acceptance Criteria

1. Given any rect and any measured string, `fit_text_in_rect` returns a box
   inside the rect — asserted over ~1500 combinations, plus NaN, zero and
   collapsed-rect cases, with no window. ✅ `test_ui_text_fit.cpp`
2. Given `phase != PLAYING|INTERMISSION`, every minimap blip and the frame are
   zero-size and the pool is still allocated. ✅ same file
3. Given the shipped `GameData.json`, the radar is ≤100 design px, top-right, and
   clear of the gauge column. ✅ same file
4. A title-screen screenshot contains no score, hull bar, credits or radar, and
   no text crosses the panel edge. ✅ frame 30, read back as pixels
5. A shop screenshot shows every card caption inside its card and the detail pane
   text inside the pane. ✅ frame 61
6. The replay canary is byte-identical across two runs. ✅

## Menu design research, and where each finding was applied

Findings are from the standard game-UI/HCI body of practice (Fitts' law on hit
targets, modular scale and vertical rhythm from typographic layout, Gestalt
proximity/alignment for grouping, WCAG-style contrast floors, and the "one
primary action" convention used by essentially every console front-end).

| Finding | Applied as |
|---|---|
| **A grid beats per-widget nudging.** Ad-hoc pixel values are what produce drift and overflow. | Every rect in `screens` is on a 4px grid; panels use a 24px inset content column. |
| **One optical left edge per panel.** Mixed centering makes a stack of lines scan slowly. | All labels are flush-left in one column; only button captions are centered. |
| **Hit targets ≥ ~44px.** Fitts' law: small targets cost time and misclicks, and this game is played mouse-in-motion. | Every button is ≥44px tall (was 38 on `menu_ship`, 40 on the shop cards/tabs). |
| **A type/colour scale, not a font-size soup.** One font means hierarchy has to come from size, weight-by-brightness and spacing. | Three steps: `title` (bright cyan) > `subtitle` (near-white) > new `caption` (dim blue-grey). |
| **Vertical rhythm.** Rows one line plus a half-line apart scan as a list; tighter reads as a block. | HUD rows at 26-32px pitch; menu sections separated by 32-48px, not by whatever was left over. |
| **Group with proximity and a rule, not with boxes.** | A 2px `rule` under every heading; the shop's two columns are two surfaces rather than one crowded panel. |
| **State feedback must be unambiguous in both channels.** Colour-only hover fails for low-contrast displays and for anyone not looking at it. | Hover/press/disabled differ in *both* bg and text on every interactive style. |
| **"Selected" and "disabled" must not look alike.** The shop disables the current tab so it cannot be re-clicked — which made the tab you are *on* look broken. | New `shop_tab` style whose disabled state is the bright selected look (D88). |
| **One primary action per screen.** | Exactly one `pulse_hz` widget per screen; the eight shop rows moved to a flatter `card` style so they stop competing. |
| **Contrast over a moving background.** | Panel alpha 225 → 242; the radar frame became a translucent smoked pane with a neon rim instead of an opaque black square. |
| **Never make the player chase information.** | The shop tooltip became a fixed detail pane that idles on the page hint (D89). |
| **Say what a thing does, in the player's words.** | `REACTOR SHIFT` (#13); per-page hints; `LV0 > LV1  120 cr` on the LEVELS page (#5). |

## Out of Scope

- Text wrapping and ellipsis (D85 — shrink-to-fit was enough and is 15 lines).
- A `game_over` / `victory` / `options` / `save_slots` screen.
- The pause screen's SAVE button (Lane K).
- The shop catalogue data and `shield_regen_frac`.

## Affected Boundaries

- **Engine:** `ui_render_math.hpp` (new pure `fit_text_in_rect`),
  `ui_render_system.cpp` (calls it). ENGINE.md updated in the same commit.
- **Game:** `game_hud_system.*`, `minimap_system.*`, `shop_system.*`, one string
  in `main.cpp`.
- **Data:** the `screens`, `ui_styles` and `minimap` blocks of `GameData.json`.

## Open Questions

- None. One note for the integrator: `hud_visible_in_phase()` hard-codes the
  `Phase` enum values because the enum lives in `main.cpp`. If the enum grows a
  seventh phase that flies the drone, that predicate is the one place to change,
  and the unit test names the values.
