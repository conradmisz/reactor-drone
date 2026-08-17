# Feature Spec: Loadout-Gated Boss Items

## Status

Done (playtest #6 item 13, D232)

## User Story

As a pilot, I want boss rewards that key off what I'm flying so that a run's
loadout shapes which powers it can find.

## Requirements

1. An active may declare `requires` in GameData: `weapon:A|B` or `ship:Name`.
   The boss reward offer NEVER shows an item whose requirement the current
   loadout fails. Unrestricted actives (missiles) are always eligible.
2. **Plasma Wake** (55 Iron or Moonshot): while held, SECONDARY projectiles
   (charge slugs, crescent-burst crescents — not primaries) leave patches of
   neon plasma (~2.5 s) that damage (12/s, guesstimate per owner) and slow
   (25%) enemies standing in them.
3. **Cryolator** (Flak Cannon): while held, the breath is ICE — no Burn;
   instead each half-second of exposure adds a Frostbite stack (−10% speed
   each). Four stacks freeze the target solid for 2 s, then stacks clear.
   Picking it turns the Flak's shells icy blue for the rest of the run.
4. **DOZR** (Gryphon): a dash that kills an enemy cuts the remaining dash
   cooldown by 75%.
5. All three occupy the ONE held-item slot and are picked/upgraded on the
   boss_reward screen exactly like the missiles. Plasma Wake and Cryolator
   are passive (E does nothing); DOZR is passive too.

## Acceptance Criteria

1. Given Falcon + 55 Iron, when the reward opens, then Cryolator and DOZR
   are absent and Plasma Wake may appear.
2. Given Cryolator held, when the breath hits one enemy for 2 s, then it
   carries 4 Frostbite stacks, freezes 2 s, and its speed restores after.
3. Given DOZR held, when a dash kill lands, then ship.dash_cd drops to 25%
   of its remaining value that frame.

## Out of Scope

- More boss items (owner's standing TODO), stacking multiple held items.

## Affected Boundaries

- arena_config (ActiveItemDef.requires, parse), boss_system (offer filter),
  active_items (ids), secondary_fire (wake drop + ice breath + frostbite
  tick), dash_system (DOZR refund), enemy_components (Chill gains
  stack/frozen fields; BlizzardTag gains dps — field adds, no new component).
