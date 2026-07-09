# Asset Credits — Reactor Drone

All art is reused from the Class-090 asset set that ships with this course, which
is derived from **Kenney.nl** CC0 (public-domain) sprite packs. No attribution is
legally required for CC0; it is recorded here for provenance.

| Asset | File | Role in Reactor Drone | Source |
|-------|------|-----------------------|--------|
| Boss sprite | `images/enemy_boss.png` (+ `.json`) | **Player drone** (placeholder) | Kenney CC0 (via Class-090) |
| Fast enemy | `images/enemy_fast.png` (+ `.json`) | Enemy type "spark" | Kenney CC0 (via Class-090) |
| Runner enemy | `images/enemy_runner.png` (+ `.json`) | Enemy type "runner" | Kenney CC0 (via Class-090) |
| Armored enemy | `images/enemy_armored.png` (+ `.json`) | Enemy type "hulk" | Kenney CC0 (via Class-090) |
| Explosion | `images/effect_explosion.png` (+ `.json`) | Enemy death animation | Kenney CC0 (via Class-090) |
| Font | `fonts/default.ttf` | HUD / banners | Bundled with the engine |

Projectiles are drawn as engine `Color` rectangles (no texture needed).

## Known art placeholders (v1)
- The **player drone** uses the `enemy_boss` sprite as a stand-in. A dedicated CC0
  top-down drone sprite (e.g. from Kenney "Space Shooter Redux") is the intended
  final art; it drops in by changing `player.sidecar` in `GameData.json` — no code
  change (see the design doc's data-driven asset note).
