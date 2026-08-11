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
| `panel` | Modal backing panel — `(8,12,26,242)` |
| `title` / `subtitle` | Screen headings — cyan `(120,225,255)` / near-white |
| `shop_button` | The pulsing primary call to action — magenta bg, amber text |
| `default_button` | Secondary action |
| `hud_bar_bg`, `hud_chip`, `hud_label` | HUD frame furniture |
| `caption` | Third step of the type scale — dim supporting text (D88) |
| `rule` | A 2px divider; a `panel` whose bg *is* the line (D88) |
| `card` | Shop rows — flatter than `default_button` on purpose (D88) |
| `shop_tab` | Tab strip. Its **disabled** state is the *selected* look (D88) |
| `minimap_frame` | The radar pane: translucent smoke + a neon rim |
| `hud_slot_frame` | The ability row's two boxes: `minimap_frame`'s rim with **no** fill, because sprites are drawn under them (D193 rev) |
| `hud_hp_ok` / `hud_hp_warn` / `hud_hp_crit` | HP fill, swapped by fraction |
| `hud_shield` | Shield fill |
| `ghost` | The hidden-button style (D137): bg = panel colour, because buttons always fill their rect with blend NONE. Blank the caption too — text ignores style alpha |

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
  `gameplay` (the HUD — always on the stack, so its visibility is a *phase*
  question, see D86), `wave_intermission`, `pause`, `main_menu`, `run_setup`,
  `save_slots`, `settings`, `records`, `how_to_play`, `shop`, `boss_reward`
  and `prestige_offer`. Title screens replace each other via CLEAR_TO (D137);
  hidden buttons use the `ghost` style. Planned: `game_over`, `victory`.
- **The layout grid (D88)**: 4px grid, one left-aligned content column inset 24px
  from its panel edge, buttons ≥44px tall, headings followed by a `rule`, one
  `pulse_hz` widget per screen. Labels are flush-left; only button captions
  centre. Author rects that fit at full size.
- **Text cannot overflow its rect (D85).** `UIRenderSystem` shrinks any label or
  button caption that does not fit. That is a floor, not a layout engine — a rect
  authored too small makes text small, it does not make it wrap.
- **Modal pattern**: a `panel` rect, a `title` label, a `rule`, a `subtitle`,
  then two buttons side by side — the primary one carries `pulse_hz` (1.1) so the
  eye lands on it.
- **z-order**: panel `0`, everything on it `10`.
- **World render layers**: 0 backdrop, 2 enemies, 3 player, 4 pickups, 5 shield
  field, 60/61 the dash button's screen-space face. UI composites last, over the
  world *and* the HUD — which is why anything drawn as a sprite under a widget
  needs that widget's fill to be transparent.
- The shop is a data-authored screen (D61): a card panel on the left, drone
  preview and a fixed detail pane on the right (D89). The `1`-`8` / `TAB` / `B`
  keyboard path survives as the headless fallback.
- **`GameHUDSystem`'s text rows are authored in the design canvas too** (D87) —
  they are drawn by `HUDSystem` in window pixels, so the system applies
  `ui_canvas_transform` itself. Anything added to that HUD must do the same.

## Icons

No icon font. Everything is generated sprite art or text.

`UIElement` has no texture path, so a widget can never carry an image — but a
HUD icon does not have to be a widget. The dash button's booster and its
circular cooldown dial are ordinary sprite entities drawn in *screen* space
(`hud_boost.png`, `hud_dash_sweep.png`), parked over the authored
`hud_dash_frame` rect. If you add another: `CameraSystem` overwrites
`ScreenPosition` for everything that has a `Position`, and `RenderSystem` only
iterates entities that have one — so the placement must be written **after**
`camera.update`, which is where `main.cpp` does it.
