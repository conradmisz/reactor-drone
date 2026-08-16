# Feature Spec: Gameplay Pack v2.3 (drones, weapons, scrap, hangar)

## Status

Approved

## User Story

As a pilot, I want collectible drones with distinct weapons, a persistent
scrap currency, and a hangar to manage them, so that runs feed a progression
loop instead of resetting to zero.

## Requirements

1. 3 playable drones (Falcon/Owl/Gryphon) with per-ship hull/shield/speed/
   dash stats, a unique sprite, and a special attribute each. 4th drone
   (current Purple Gatling, owner of Hailstorm) implemented but locked —
   later release.
2. 4 weapons (55 Iron, Moonshot, Flak Cannon, Hailstorm), each with
   damage / fire rate / recharge rate / range, a distinct projectile look,
   and a right-mouse secondary on a 10 s cooldown (55 Iron: hold-to-charge,
   cooldown scales with hold).
3. Persistent scrap on meta.json: earned per wave, more per boss, bonus at
   wave 30; shown on a new end-of-run stats screen. Drones are bought with
   scrap (lifetime-score unlock retires). Buying a drone grants its weapon
   and its body/trail/projectile colors account-wide.
4. Hangar (expanded run_setup): drone preview, stat sheet with ALIGNED
   5-pip meters, change-ship / change-weapon, cosmetic shop + inventory
   entries, big green LAUNCH bottom-left.
5. Cosmetics: ship-color + trail-color slots per drone, projectile-color
   slot per weapon; cosmetic shop sells extra colors for scrap; inventory
   lists owned weapons and cosmetics (ship stuff / projectile colors).
6. Seeded-random arena order per run; Prism/Prism II never first;
   Singularity stays the wave-30 finale.
7. Boss rework: 2-phase enrage below 50% HP (denser patterns, hunt/
   reposition movement, fixes stuck-behind-structures); reward menu only
   after boss AND its summoned adds die; boss-item catalog trimmed to
   Heat-Seeking Missiles.
8. Fix batch: player no longer passes through enemies (dash excepted, with
   bounce-out); 4 recharging pickups on later waves; bigger health pickups;
   higher early-wave credit drop rate; ESC in shop closes shop only; trail
   originates at drone rear (tracer feel); leaderboard button on main menu;
   website disclaimers (leaderboard wipes, local-save loss); GEAR + LEVELS
   shop tabs removed (UPGRADES catalog unchanged — see D220).

## Acceptance Criteria

1. Given a fresh meta.json, when the game boots, the player owns Falcon +
   55 Iron and the hangar shows them equipped.
2. Given a finished run (victory or defeat), the stats screen shows scrap
   earned and meta.json scrap increases by exactly that amount.
3. Given enough scrap, buying Owl in the hangar grants Moonshot plus purple
   body/trail/projectile colors, usable on other drones/weapons.
4. Given seed A twice, arena order is identical; given seeds A and B, orders
   differ; no run ever starts on Prism/Prism II; wave 30 is Singularity.
5. Given a boss below 50% HP, its attack cadence and movement visibly
   change; the reward menu appears only once its adds are dead.
6. Given right mouse held with 55 Iron, releasing fires a charged shot whose
   damage and cooldown scale with hold time (cooldown ≤ 10 s).
7. Given a pre-v2.3 meta.json / run save, loading neither crashes nor loses
   score/prestige (edge case).

## Out of Scope

Prestige iteration, wave-count changes, more boss items (TODO), 4th-drone
release (TODO), per-ship tailored in-run upgrades, persistent cloud saves.

## Affected Boundaries

arena_config (ShipDef/WeaponDef/GameConfig), player_fire_system,
boss_system, meta_save/run_save, shop_system, GameData.json screens +
tuning blocks, main.cpp hooks/router, backend worker.js (disclaimers).

## Open Questions

(none — resolved in owner interview 2026-08-15; logged as D221)
