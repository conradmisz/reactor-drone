# Feature Spec: Art overhaul (iteration 5, Lane L)

## Status

Done

## User Story

As a player, I want the ships, enemies and the boss to read as *drones* — and my
own shots to read as mine — so the arena is legible at a glance instead of a
field of neon polygons.

## Requirements

1. **#10 Foundry boss** — the boss must render as one capital-class carrier
   drone. It currently draws `enemy_hulk.png` through an `Images` component,
   which is the *whole 4x4 atlas*, i.e. the literal "grid of hexagons".
2. **#11 Enemies** — spark / runner / hulk / warden graduate from single
   polygons to multi-part drone silhouettes, still MONO luminance (arena
   `enemy_tint` colour-mods them), still right-facing and horizontally
   symmetric, still 8 march + 6 death frames at 128px, 4 columns.
3. **#8 Player** — the drone reads as a quad-rotor drone, not an arrow.
4. **#1 Projectiles** — player shots and their trails are red.
5. **#3 Moon shooters** — shots leave the crescent's mouth, and the crescent
   turns to face its target so the mouth is the side the shot leaves from.

## Acceptance Criteria

1. Given a boss wave, when the boss spawns, then its sprite is a single carrier
   (one image, no atlas grid) tinted by the arena.
2. Given any regenerated sprite, when `test_manifest.py` runs, then every
   sidecar still matches its atlas (frame counts and animations unchanged).
3. Given a moon shooter at (cx,cy) aiming along `a`, when it fires, then the
   shot origin is `(cx,cy) + muzzle_forward(tier)*size` along `a`, never the
   centre, and the entity's `Rotation.angle == a`.
4. Given tier 1/2/3, when `moon_muzzle_frac` is evaluated, then it is strictly
   inside (0, 0.5) and increases with tier (the bigger crescent has the deeper
   mouth) — a negative value would fire out of the back, which is the bug.
5. Given a fixed seed, when the headless canary runs twice, then the summary is
   byte-identical — none of the above draws RNG.

## Out of Scope

- Backdrops (`make_backdrops.py` is not run — it rewrites unrelated arenas).
- The moon crescent shapes themselves (D93, unchanged).
- New sprites for the four specialty units: they reuse the enemy atlases, so
  they inherit the overhaul with no `GameData.json` edit.
- Anything Lane M/N/O owns.

## Affected Boundaries

- `assets/generator/v2/make_sprites.py` and its committed output in
  `assets/images/v2/`.
- `CPP/game/enemy_fire_system.{hpp,cpp}` (muzzle offset + facing).
- `CPP/game/player_fire_system.cpp` (shot colour).
- `CPP/game/boss_system.cpp` (one `Images` path).

## Task Breakdown

1. Redraw player / spark / runner / hulk / warden; add the carrier.
2. Regenerate, eyeball the PNGs, run `test_manifest.py`.
3. `muzzle_forward` + facing in `EnemyFireSystem`, with a unit test.
4. Red projectiles.
5. Point the boss at the carrier.
6. Gate: build, tests, canary twice, screenshot.

## Open Questions

- None.
