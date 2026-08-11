---
id: 002
title: Upgrade kit / shield field / aura trail one frame behind the ship
status: resolved
severity: medium
area: game
opened: 2026-08-10
resolved: 2026-08-10
---

## Symptom

The upgrade-kit overlays (and the shield field ring and item aura) do not
stay affixed to the drone — they visibly lag behind it while it moves, worst
during a dash. Between waves (intermission victory lap) the parts freeze in
place entirely while the hull flies on.

## Reproduce

1. Run the game, buy any kit upgrade (e.g. Hull Plating) so a part is worn.
2. Fly in a straight line, then dash (SPACE): the part trails the hull by a
   frame; the faster the ship, the larger the gap.
3. Clear a wave, fly during the between-waves prompt: the part stops moving.

## Environment

master @ 4607ebd ancestry, any seed, windowed or headless. Data-independent.

## Ruled Out

- **Tested:** read the frame order in main.cpp vs ENGINE.md §3.
  **Observed:** the Phase 5c equipment-visuals block ran right after
  `player_aim.update`, copying the player's `Position` — but
  `movement.update`, `clamp_to_arena` and `push_out_of_solids` all ran
  *later* in the frame, then rendering used the followers' stale copy.
  **Eliminates:** render interpolation / camera as suspects — pure frame
  ordering.
- **Tested:** checked the intermission branch for any follow logic.
  **Observed:** it runs movement + clamp for the player but never updated
  the followers at all. **Eliminates:** nothing — second symptom, same root.

## Suspects

1. **Follow block runs pre-movement** — confirmed (see Resolution).

## Resolution

Root cause: the equipment-visuals block copied the player's pre-movement
`Position`; the ship then moved, was clamped and pushed out of solids before
render, so every follower drew one frame behind (and never at all in the
intermission branch, which lacked the block).

Fix: extracted the block into `update_equipment_visuals` (lambda above the
phase machine in main.cpp) and call it after `push_out_of_solids` in the
playing branch and after the intermission clamp. Aim is still read after
`player_aim.update`, satisfying the Phase 5c constraint. ENGINE.md §3
updated in the same commit.

Verified: build clean, replay canary (`--seed 42 --keys 5:SPACE
--stopframe 3000` twice) byte-identical, and a windowed playtest pending —
dash test in step 2 to be confirmed on the next play session.
