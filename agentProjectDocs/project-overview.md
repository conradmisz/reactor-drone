# Reactor Drone v2 — Neon Arena Survival

## Overview

A top-down arena survival shooter built on the CS-5850 / hearth-and-hollow
(Class-110) C++17 + SDL3 ECS engine. You fly a maintenance drone inside a
reactor arena, mouse-aiming against ring-spawned enemy waves across a 20-wave
arc, banking currency from loot drops and spending it in a between-waves shop
on permanent upgrades and equipment. v2 is a total visual and gameplay
overhaul of the original *Reactor Drone*: procedurally generated neon art,
particles, screen shake, four themed arenas with solid obstacles, A* enemy
pathfinding, and a data-driven UI/menu layer. It is a personal project, not
the graded course submission.

## Goals

1. A 20-wave run that is worth replaying — the shop decision at waves 4/8/12/16
   is the core of it, and it is currently priced entirely by guesswork.
2. Everything a player sees is authored as data (`assets/GameData.json`,
   generated PNGs) rather than hardcoded, so iterating on feel needs no rebuild.
3. Keep the engineering gates from the course green permanently: zero warnings,
   100% ctest, hermetic fresh-clone build, deterministic seeded replay.
4. Keep the engine reusable and its provenance measurable — `ENGINE.md` says
   exactly which files are ours versus the class baseline.

## Core User Flow

1. Title screen → SPACE/click to start.
2. Fly the drone (WASD/arrows), aim with the mouse, hold to fire. Enemies
   ring-spawn and seek/path toward you; contact hazards and enemy contact hurt.
3. Kill enemies → they drop currency pickups (and rarely a shop key) you walk
   over. Shields absorb before hull; i-frames gate repeat contact.
4. Clear a wave → the arena keeps running until it is empty, then the wave
   advances. Every 4th cleared wave the run freezes on the **wave-intermission
   prompt**: SHOP OPEN or NEXT WAVE, nothing advances until you pick.
5. Shop (`PHASE_SHOP`): page 1 UPGRADES (6 escalating-price permanent upgrades),
   TAB → page 2 GEAR (4 passive items, 4 one-use consumables). `1`-`8` buy,
   `B` launches. A shop key lets you open it mid-run with `B`.
6. Arena theme swaps by wave (Core 1 / Foundry 6 / Bio-lab 11 / Prism 16) with a
   crossfade; enemies re-tint to the arena's complementary neon.
7. Wave 20 cleared → victory. Hull to zero → game over. Click/SPACE to restart.

## Features

### Combat & progression
- **Gameplay pack v2.3** (D221-D227, spec `specs/gameplay-pack-v2.3.md`):
  persistent **scrap** currency banked per wave/boss/victory; a 4-drone roster
  (Falcon free, Owl 400, Gryphon 800, Gatling locked for a later release) bought
  with scrap, each with its own hull/shield/speed/dash profile and a special
  (Falcon equipment-cooldown, Owl phoenix veil, Gryphon ram dash).
- **First-class weapons** (55 Iron / Moonshot / Flak Cannon / Hailstorm),
  installable on any owned drone, each with a right-mouse **secondary** on a
  shared 10s slot: hold-to-charge slug, radial crescent burst, lava stream with
  a burn DoT, and a traveling blizzard that chills.
- **Per-run arena shuffle** (seeded; Prism never opens a run, Singularity stays
  the wave-30 finale) and **solid enemies** — the drone no longer passes through
  them, dash excepted.
- **Two-phase boss**: an enrage below 50% HP (faster cadence, denser patterns,
  aggressive repositioning), a reward menu that waits for the boss's adds, and a
  boss-item catalogue trimmed to Heat-Seeking Missiles.
- **Hangar** (the old run_setup): live drone preview, aligned pip stat sheet,
  ship/weapon selection, scrap purchases, plus a cosmetic shop and inventory;
  and an end-of-run **flight report** showing scrap earned.
- **Cosmetics**: ship, trail and projectile colour slots; three paints granted
  by drone ownership and three more bought with scrap.
- 20-wave arc: waves 1-11 fixed-count, 12-20 timed; per-wave `hp_mult` /
  `speed_mult` scaling over 3 enemy types (spark / runner / hulk).
- **30-wave arc** (supersedes the 20- and 50-wave tables above): waves 1-15
  fixed-count, 16-30 timed, bosses on 10/20/30, generated from one linear formula
  so pressure is monotone by construction (D125).
- **Prestige**: finishing the arc offers a restart that strips this run's
  upgrades and buys +10% hull / +5% speed / +8% damage per level (capped at 5),
  persisted as one field in `saves/meta.json` (D127-D131).
- Normal / Hard chosen on the title menu; a difficulty is multipliers over the
  one wave table (count, spacing, hp, speed, credits, hazard damage, plus how
  many waves earlier enemy types unlock) and is enemy-side only (D50).
- A wave advances only when its quota/timer is done **and** the arena is clear;
  a 30 s stall watchdog force-kills stragglers through the normal death path.
- **Units** economy: 1-3 pickups per kill (each worth its type's value plus a flat
  2) plus a 0.5% shop-key roll, magnet-able, expiring on a 14 s `Lifetime` (D193).
- From wave 15 the drone passively vacuums units inside 140 px, and 3+ enemies
  dying in one frame within 180 px pay out 15-value BIG UNITs (D193).
- Thruster dash on **SPACE** — and SPACE alone, it no longer fires (D193) — 10 s
  cooldown shown on its own HUD button, damages what it passes through (D120);
  the stack gains one more charge for every boss killed (D192).
- Heat-seeking missiles fly nose-first as rockets and launch in two salvos 0.45 s
  apart, each detonating in a 200 px blast (D193).
- Primary fire runs off a **battery**: 12 s of continuous fire, and emptying it
  locks the trigger for the full 3 s recharge (D192).
- Bosses carry a named health bar, and a mine's blast hurts enemies as well as
  the drone (D192).
- Foundry mines are destroyable: a shot pops one from outside its 100px blast (D121/D122).
- Shop upgrades show on the drone — the engine plume heats up as they stack (D123).
- Shop: Hull Plating, Shield Capacitor, Aux Thruster, Overclock, Heavy Rounds,
  Twin Barrel, Long Barrel, Ricochet Coils — price escalates per purchase
  (`price_growth` 1.5).
- Long Barrel buys range as `projectile_lifetime`, never speed (D97); Ricochet
  Coils gives each shot a bounce budget that reflects it off obstacles and the
  arena ring instead of killing it (D98).
- Gear: Magnet Core, Repulsor Field, Reactive Plating, Salvager (1 item slot);
  Repair Kit, Overdrive, EMP Burst, Phase Shift (1 consumable slot, `Q` to use).
- Shields regen after a quiet delay and drain before hull; a fully-absorbed hit
  still costs i-frames, trauma and a flash.
- Boss waves spawn nothing but the boss and its summons (D182); coins blink
  accelerating over their last 3 s before expiring (D183).
- Player shots fire in the complement of the ship's hull hue (D184); every
  enemy shot is red (D185); every enemy rotates to face its heading (D186);
  poison patches are red-rimmed vapour clouds (D187).
- Pause sheet shows 5-circle stat pips and per-upgrade percentages (D188).
- Shop: purchases are press-and-hold with a progress strip (D189), the preview
  drone wears the kit and tries parts on when a row is hovered (D190), and the
  cards are a two-column aligned table with green/red pip previews (D191).

### Arenas & world
- Four arenas (Core / Foundry / Bio-lab / Prism), each with 3 parallax backdrop
  layers, 15-20 solid obstacles, 6-9 contact hazards, and an `enemy_tint`.
- Big arena with a follow camera, a visible boundary wall, and a circle clamp
  plus obstacle push-out applied to player and enemies.
- A* enemy pathfinding around obstacles (`repath_interval` 0.35 s, 40 px grid),
  falling back to straight-line seek when line-of-sight is clear.

### Presentation
- Procedural asset pipeline (`assets/generator/v2/`, offline, output committed):
  neon sprites, parallax backdrops, palettes, `.wav` synth.
- Particle system: thruster cone, projectile trails, death bursts, pickup pops,
  arena-shift shockwave, item auras. 2000-particle global budget.
- Additive glow blending, `Tint` colour/alpha mod, `RenderLayer` bucketing,
  sprite flip.
- Hit feedback: trauma-driven screen shake (seeded, replay-safe) and `Flash`.
- Drone art language (D107): player and enemies are rotor-and-boom drones with
  blades that turn with the animation phase; the boss is a 256px "Capital Drone
  Carrier" (D105) and its adds wear the spawner's sprites (D106).
- Player fire is red (D108); moon shooters turn to face their target and shoot
  out of the crescent's mouth (D109).
- Data-authored UI layer (Option-040 port): `ui_styles` + `screens` in
  `GameData.json`, pulsing buttons, HUD chips/bars driven as UI widgets.

### Tooling
- Deterministic headless driving: `--seed --keys --clicks --hover --screenshot
  --stopframe`; the shutdown line prints Frames / Score / Credits / Wave / Phase.
- `python run.py` menu; Catch2 unit + property suites, 8 ctest targets.
- `--dev` / `--god` playtest mode: unlimited units, **B** opens the shop on
  demand, **F5** skips a wave, `--level N` picks the starting wave (D193).

## Scope

### In Scope

Scope is deliberately **playtest-driven**: play the game, judge how it feels,
iterate, add features as they suggest themselves. The committed near-term work
is Part 4 (menu screens) and Part 5 (run-state save/load); audio is last and
must stay rip-out-able.

### Out of Scope

- **The course submission.** No `final` tag, no `submission/` rehearsal, no
  grading. This is an evolved v2 in its own repo. The *engineering* gates are
  kept anyway because they are cheap and they hold the codebase together.
- **Multiplayer / networking.** Single-player, forever.
- **World-snapshot save/load.** Part 5 is a flat run-state struct only — no
  `SerializationRegistry`, no `component_save_table`, no `LoadSystem` port.
- **New enemy types.** The 3 existing types get scaled per wave instead.
- **A Lua menu layer.** The `ui.*` bindings are ported and tested but inert;
  clicks are read off the Blackboard.
- **Speculative engine generality.** No system exists without a caller in this
  game.

## Success Criteria

1. A full 20-wave run can be played end to end without a soft-lock, and the
   four credit numbers in the balance log have real measured values.
2. `python runTestsAll.py` → 8/8 ctest, and the build emits zero warnings from
   our code under `-Wall -Wextra -Wpedantic`.
3. Two runs of `game --seed 42 --keys ... --stopframe N` print an identical
   summary line.
4. Every tunable a playtest would want to change is a `GameData.json` edit with
   no rebuild.
