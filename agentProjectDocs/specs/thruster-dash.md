# Feature Spec: Thruster dash (Game Note #5)

## Status

Done — Lane B, iteration 3 (D57). Unit-tested and headless-verified; not playtested.

## User Story

As a pilot boxed in by a swarm, I want a short burst of thrust that hurts what it
passes through so that being surrounded is a problem I can *solve* rather than
one I can only avoid.

## Requirements

1. LSHIFT (or RSHIFT) fires a burst of `dash.speed` along the drone's movement
   direction, or its aim direction when it is standing still, for `dash.duration`.
2. The burst is gated by `ShipState.dash_cd`, reset to `dash.cooldown` on trigger.
3. Every enemy the burst overlaps takes `dash.damage` **exactly once per dash**.
4. One dash costs the player **at most one** hit, whatever it ploughs through.
5. `--keys N:LSHIFT` fires it from a headless script.
6. The visual reuses the drone's existing thruster emitter — no new emitter, no
   new entity, no measurable particle-budget cost.

## Acceptance Criteria

1. Given a dash is triggered, when the frame resolves, velocity is exactly
   `dash.speed` along the captured heading; after `duration` has elapsed, ordinary
   movement is back in control.
2. Given a dash was triggered, when LSHIFT is held continuously for less than
   `cooldown`, the burst does not restart; past `cooldown` it does.
3. Given two enemies parked on the drone for the whole burst, each accumulates
   exactly one `DamageEvent` of `dash.damage`. A *later* dash may damage the same
   enemy again.
4. Given five enemies on the drone, `player.iframes` is untouched on the first
   contact frame (so `PlayerDamageSystem` resolves that one hit normally) and is
   held above the remaining burst on every frame after it.
5. Given a dash through empty space, no `DamageEvent` is created and
   `player.iframes` is never written.
6. Given the drone is stationary and aimed north, the burst goes north.

## Out of Scope

- A dash HUD/cooldown readout.
- Dash-through-obstacles rules: the burst is ordinary velocity, so the existing
  collision response applies unchanged.
- Any upgrade axis (charges, shorter cooldown) — that is Lane C's gear work.

## Affected Boundaries

- `CPP/game/dash_system.hpp` (new), the `dash` hook in `main.cpp`,
  `VALID_KEY_NAMES` in `cli_parser.cpp`, the `dash` block in `GameData.json`.

## Task Breakdown

1. `DashState` + `tick_dash`.
2. Hook block: key read (physical + scripted) and the `static DashState`.
3. `LSHIFT` in `VALID_KEY_NAMES`.
4. `test_dash.cpp`.

## Open Questions

None. (The plan flagged that nothing in the notes names a binding; LSHIFT is the
assumption it proposed and it is what shipped.)
