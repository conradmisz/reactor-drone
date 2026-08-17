# Feature Spec: Map Modifiers

## Status

Draft — owner idea from playtest #5 (2026-08-16), NOT scheduled. Recorded so
it survives the session; interview the owner before building.

## User Story

As a player, I want arenas to carry composable modifiers so that any map can
surprise me — The Drift stops being "the drift map" and becomes any map with
the drift modifier rolled onto it.

## Sketch (owner's words, lightly structured)

- Refactor: `drift` becomes a MODIFIER an arena can carry, not an arena
  identity. (tick_drift already takes plain dx/dy — the mechanics half is
  modifier-shaped today; the data half is not.)
- Other modifiers named: periodic weather storms; shifting terrain; black
  holes appearing.
- Likely data shape: `modifiers: [{type, ...knobs}]` per arena entry (or
  rolled per run like the arena shuffle), each with its own tick in
  arena_mechanics.hpp.

## Open questions (ask before building)

1. Rolled per run (roguelike variety) or authored per arena (identity)?
2. Can modifiers stack? Drift + black holes at once?
3. Do The Shroud's light_radius and The Drift's current migrate into the
   system, or stay grandfathered as arena identity?
4. Wave-scaling — do modifiers intensify on the second pass (26+)?

## Out of Scope

Everything, currently — this file is a parking spot, not a plan.
