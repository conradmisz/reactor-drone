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

## v2 — procedurally generated neon art (all original)

The v2 visual overhaul replaces the placeholder Kenney art with an **entirely
procedural, original** asset set generated offline by the scripts under
`assets/generator/v2/` (pure Pillow for images, stdlib `wave`+`math` for audio).
Nothing is downloaded at build time; every committed PNG/WAV has a generator that
reproduces it deterministically. These outputs are original works placed in the
public domain (CC0) alongside the course art.

| Asset | Files | Role | Generator |
|-------|-------|------|-----------|
| Player drone | `images/v2/player_drone.*` | Player | `make_sprites.py` |
| Enemies | `images/v2/enemy_{spark,runner,hulk,warden}.*` | 4 enemy types | `make_sprites.py` |
| Projectiles | `images/v2/projectile_{plasma,bolt}.*` | Player/enemy shots | `make_sprites.py` |
| Effects | `images/v2/effect_{explosion,impact}.*` | Death / hit bursts | `make_sprites.py` |
| Glow textures | `images/v2/glow_*.png`, `muzzle_star.png` | Additive glow/muzzle | `make_glow.py` |
| Backdrops | `images/v2/bg_{core,foundry,biolab,prism,galaxy}_{far,mid,near}.png` | 3 parallax layers × 5 arenas | `make_backdrops.py` |
| Arena props | `images/v2/{wall,pillar,vent}_{core,foundry,biolab,prism,galaxy}.png` | Boundary wall / obstacles / hazards, one bespoke shape per theme | `make_backdrops.py --props-only` |
| SFX | `Audio/{laser,explosion,hurt,level_up,wave_chime}.wav` | Gameplay cues | `make_sfx.py` |
| Ambient loops | `Audio/ambient_{core,foundry,biolab}.wav` | Per-arena drones | `make_sfx.py` |

Palettes (three synthwave schemes) live in `assets/generator/v2/palette.py`, the
single source of truth imported by every generator. Consistency is checked by
`assets/generator/v2/test_manifest.py` (every sidecar's atlas exists and its frame
grid fits). A CC0 Kenney "Space Shooter Redux" overlay is an optional flavour add
(`cut_kenney.py`, not required); the game is complete on the procedural set alone.

## Legacy v1 art (retained, unused by v2)
The original Kenney-derived placeholders remain in `images/*.png` for reference;
v2 GameData points at `images/v2/*` instead.
