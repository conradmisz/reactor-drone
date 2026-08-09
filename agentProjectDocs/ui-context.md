# UI Context

This is a game, not a web app: there is no CSS, no component library and no
DOM. "UI tokens" here means two things — the **arena palettes** in
`assets/generator/v2/palette.py` (the single source of truth for every generated
sprite/backdrop and for each arena's `enemy_tint`) and the **`ui_styles` block**
in `assets/GameData.json` (widget colours per state). Neither is ever bypassed
with a literal colour in C++.

## Theme

Synthwave / neon sci-fi. Near-black clear colour, bright saturated neons drawn
with additive glow blending, heavy bloom-by-overdraw from the particle system.
Each arena is one coherent palette, and enemies are always the *complementary*
neon so they never disappear into the backdrop.

## Palettes (`assets/generator/v2/palette.py`)

| Arena | Waves | clear | primary | secondary | enemy tint |
| --- | --- | --- | --- | --- | --- |
| Core | 1-5 | `(10,10,20)` | `(60,230,255)` cyan | `(255,70,200)` magenta | magenta |
| Foundry | 6-10 | `(20,10,6)` | `(255,150,40)` orange | `(255,210,70)` amber | cyan |
| Bio-lab | 11-15 | `(6,20,12)` | `(80,255,140)` green | `(180,120,255)` violet | violet |
| Prism | 16-20 | `(14,10,22)` | `(200,190,255)` pale violet | `(255,255,255)` white | hue-cycled per frame |

`MONO` is not an arena: enemy sprites are generated against it so the art is
pure luminance and runtime colour-mod reproduces any hue. Prism is deliberately
achromatic because its enemies tie-dye through the whole wheel.

## Widget styles (`GameData.json` → `ui_styles`)

Each style id carries four states (`normal` / `hovered` / `pressed` /
`disabled`), each `{bg: [r,g,b,a], text: [r,g,b,a]}`.

| Style id | Used for |
| --- | --- |
| `panel` | Modal backing panel — `(10,14,30,225)` |
| `title` / `subtitle` | Screen headings — cyan `(120,225,255)` / near-white |
| `shop_button` | The pulsing primary call to action — magenta bg, amber text |
| `default_button` | Secondary action |
| `hud_bar_bg`, `hud_chip`, `hud_label` | HUD frame furniture |
| `hud_hp_ok` / `hud_hp_warn` / `hud_hp_crit` | HP fill, swapped by fraction |
| `hud_shield` | Shield fill |

Adding a widget colour means adding a style id here, never a literal in
`ui_render_system.cpp`.

## Typography

One font: `assets/fonts/default.ttf` (DejaVu, licence committed alongside).
SDL3_ttf renders it; there is no second face and no icon font.

## Layout

- **Design canvas: 800×600.** Every `screens[].widgets[].rect` is authored in
  it. `ui_canvas_transform` letterboxes it onto the 980×660 logical surface
  (scale 1.1, x-offset 50). Both drawing and hit-testing apply the transform, so
  the drawn rect and the clickable rect cannot drift apart.
- **Screens are data**, in `GameData.json` → `screens`. Current screens:
  `gameplay` (the HUD, always active), `wave_intermission`, `pause`, and
  `main_menu` (the title screen's difficulty select — NORMAL pulses,
  HARD carries the `shop_button` style). Planned: `game_over`, `victory`,
  `save_slots`, `options`.
- **Modal pattern**: a `panel` rect, a `title` label, a `subtitle` label, then
  two buttons side by side — the primary one carries `pulse_hz` (1.1 on the
  shop button) so the eye lands on it.
- **z-order**: panel `0`, everything on it `10`.
- **World render layers**: 0 backdrop, 2 enemies, 3 player, 4 pickups. UI
  composites last, over the world *and* the HUD.
- The shop itself is still the pre-UI-layer implementation: `Text` +
  `ScreenPosition` entities driven by the `1`-`8` keys and `TAB`, not widgets
  (D11 — clickable cards were deferred).

## Icons

None. Everything is generated sprite art or text.
