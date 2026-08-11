# Reactor Drone v2 — voiceover notes

Say **30 waves**, not 50 (README is stale). Say **there is no audio** early.
Balance numbers are provisional — use relative language.

---

## 1. What the core engine is

The CS-5850 class engine — C++17 + SDL3, hand-rolled ECS, no game framework.
Built one module at a time across the semester, each on a different toy game:

| | |
|---|---|
| 010 | ECS spine — entities, components, systems |
| 020 | Timer, InputSystem, Blackboard, movement |
| 030 | ResourceManager, textures, HUD |
| 040 | Camera, debug HUD, CLI debug flags |
| 050 | AABB collisions, collision layers |
| 060 | Circle/OBB, brute-force vs uniform-grid vs quadtree |
| 070 | Embedded Lua + engine bindings |
| 080 | Sprite atlases, time-based animation |
| 090 | Pathfinding, tile maps — **the fork point for this game** |

Two electives got cannibalised into v2: **Option-020 particles** and
**Option-040 UI/menus**.

> "I started from a tower-defense demo and had to bend it into a twin-stick
> shooter without breaking the 150 files I wasn't supposed to touch."

## 2. What v2 added to the *engine*

Measured, not remembered — `ENGINE.md` §2 regenerates it with a `cmp` sweep:
**208 files: 150 byte-identical, 31 modified, 27 new.** ~55k lines total.

**New:**
- `ParticleSystem` — emitters, per-particle colour/size lerp, 4000-particle
  budget, and an `emit` flag separating ageing from spawning
- The UI layer: `UISystem`, `UIRenderSystem`, `ScreenStackSystem`,
  `ScreenFadeSystem`, plus a style table so widget colours are data

**Modified:**
- `RenderSystem` — tint/alpha modulation, additive blend, render-layer
  bucketing, tiled parallax backdrops
- `InputSystem` — logical-space mouse coords, WASD aliases; **Escape stopped
  being an instant quit**, so the game decides it means "pause"
- `ComponentStorage` / `destruction` — new component types, UI included

So what does that actually mean. The class engine could draw a sprite, but every
sprite came out looking exactly like its PNG. There was no way to say "this one
is red now," or "this explosion should glow," or "draw the background behind
everything else." That's the render work — tint, additive blending so glows stack
instead of painting over each other, and layers so the parallax backdrops know
they're backdrops. Small changes, but nothing in this game looks the way it does
without them.

Particles are the fun one. They're not a special effects system bolted on the
side — every particle is just a regular entity with a Particle component, so the
same movement and lifetime systems that handle bullets handle sparks. The catch
is a budget: 4000 live particles, and past that it silently drops them. Which is
fine right up until you kill 96 enemies in one frame and the explosions get
quietly thinner.

The menu layer is the biggest single import. Screens, a screen stack, widgets
that know they're hovered or pressed, fade transitions between them. The nice
part is that screens are declared in the JSON rather than built in code — the
pause menu, the shop, the boss reward picker are all just data.

And the Escape key thing sounds trivial but wasn't. In the class engine Escape
quit the program, full stop, buried in the input system. Pause menus don't exist
in a world where Escape quits. So input stopped making that decision and started
just reporting "Escape happened," and the game decides what that means depending
on what's on screen.

## 3. Systems this added (CPP/game/, ~22k lines)

| | |
|---|---|
| combat | wave_spawner · enemy_seek · enemy_fire · specialty · player_aim · player_fire · projectile_hit · player_damage · enemy_death |
| boss | boss_system · active_items (missiles / laser / forcefield) |
| economy | shop · pickup · item_system · sustain_spawn |
| feel | flash · shield · dash · arena_vfx · parallax |
| readout | game_hud · minimap · pause_stats |
| persist | meta_save (lifetime score, prestige) · run_save |

**Systems never call each other.** They read/write components; anything
cross-cutting goes through the Blackboard by string key. That is why a 27-system
frame stays debuggable.

The way this hangs together is the ECS bit doing its job. Nothing in that table
knows about anything else in that table. The spawner makes enemies, the seek
system moves whatever has a seek component toward the player, the collision
system notices overlaps, the hit system reads those overlaps and applies damage,
the death system cleans up and drops loot. Each one just looks for entities with
the components it cares about and ignores the rest. When something needs to be
genuinely global — the current wave number, say — it goes on the Blackboard,
which is a string-keyed bag any system can read.

Which is why adding features stayed cheap. The dash is basically one system that
briefly multiplies your velocity and marks you as dangerous to touch. The shield
is one system with a timer. Bosses are an enemy with more health, a summon timer
and a couple of arena-specific tricks. None of those needed the existing systems
to change.

The order does matter though, and a few of those orderings were bugs first.
Shields tick *before* damage, so a hit landing this frame restarts the regen
timer instead of the timer overwriting the hit. The between-waves intermission
deliberately doesn't freeze the arena — it did at first, and it stranded every
credit the wave's last kill had dropped, sitting there on the floor where you
could see it and not reach it.

Last thing worth mentioning: saving is two tiny JSON files and neither one
touches the simulation. One holds lifetime score and prestige, the other holds
run state — wave, credits, hull, gear, seed. No snapshot of the world, no list
of entities. Both read once at startup. That's deliberate, and it's what keeps
the deterministic replay honest.

```
  ┌─ DATA ────────────────────────────────────────────────────────┐
  │  GameData.json — arenas[9] enemy_types[10] waves[30] ships[2] │
  │  shop · boss · actives · ui_styles · screens[12]   ← MENUS    │
  │  images/v2/*.png + sidecars · generators run OFFLINE only     │
  └───────────────────────────────┬───────────────────────────────┘
  ┌─ GAME ────────────────────────▼───────────────────────────────┐
  │  main.cpp phase machine                                       │
  │  TITLE → PLAYING ⇄ INTERMISSION → SHOP → GAMEOVER / VICTORY   │
  │  combat · boss · economy · feel · readout · persist           │
  └───────────────────────────────┬───────────────────────────────┘
  ┌─ ENGINE SYSTEMS ──────────────▼───────────────────────────────┐
  │  [=] class  [~] modified  [+] new                             │
  │  movement collision(brute/grid/quadtree) lifetime animation   │
  │  rotation wrap camera hud debug_hud screenshot script    [=]  │
  │  render  input  player_control                           [~]  │
  │  particle │ ui · ui_render · screen_stack · screen_fade  [+]  │
  └───────────────────────────────┬───────────────────────────────┘
  ┌─ ENGINE CORE ─────────────────▼───────────────────────────────┐
  │  EntityManager · ComponentStorage · Blackboard · destruction  │
  │  Timer · ResourceManager · loaders · pathfinding · Lua        │
  └───────────────────────────────┬───────────────────────────────┘
  ┌─ SDL3 + SDL3_image + SDL3_ttf ▼  (no SDL2 shim) ──────────────┘
```

## 4. Gameplay features

- **30 waves, 9 arenas, 4 themes** — Core / Foundry / Bio-lab / Prism cycled
  twice, tougher the second pass, then Singularity for the finale
- **Ring spawning** — enemies arrive on a circle, no safe corner
- **10 enemy types.** Moon shooters escalate: slow shots → tracking → piercing
  lasers. Specialty units per theme: poison spitters, mine droppers, armoured
  bulwarks, Prism splitters that break in two when killed
- **Bosses every 10 waves**, themed to their arena; the final one turns the
  arena into a black hole at half health
- **Boss rewards** — pick one of three actives on a 30 s cooldown: homing
  missiles, a 360° four-beam laser, or a forcefield that auto-fires below 20%
  hull, healing you and holding enemies out
- **Dash** (Space, 10 s cooldown) damages what you pass through — escape *and* attack
- **8 upgrade lines** with stack caps and 1.5× price growth, plus gear and
  consumables. Upgrade panel every wave, full shop every fifth
- **Persistent progression** — lifetime score unlocks ships (Purple Gatling at
  4,000: 12 shots/s × 6 dmg vs 4 × 20). Ships are data: a sprite + a weapon block
- **Prestige** — finish the arc, restart it permanently stronger with upgrades gone
- **Two difficulties** — Hard is 1.5× enemies, 0.7× interval, 1.3× HP, 1.6× boss,
  and pays 1.4×. It raises the threat, never the tax
- **Save & resume mid-run** from the pause menu
- **Everything above is data** — one 110 KB `GameData.json`. No rebuild to tune.

## 5. If you have 20 spare seconds

Four gates kept green: **zero warnings** (`-Wall -Wextra -Wpedantic`),
**8/8 ctest, ~24k assertions**, **hermetic build**, **deterministic seeded
replay** — every system draws from the RNG on every path in a fixed order, so a
branch changes how many draws are *used*, never how many are *taken*.

```
SDL_VIDEODRIVER=dummy ./CPP/build/game/game --seed 42 --keys 10:SPACE --stopframe 3000
```

> "Determinism wasn't for speedrunners. It was so I could prove a change didn't
> alter the game before I ever looked at it."

**Not done:** no audio, balance is eyeballed, particle budget is headroom rather
than a measured ceiling.

## 6. Challenges faced

### The bubble shield

The Shield Capacitor was the one upgrade that couldn't be a static overlay. Every
other shop row bolts a part onto an authored hardpoint on the chassis — plating on
the flank rails, a heatsink in the tail socket — and those are single-frame images
drawn in the chassis's own 128 px space, so they composite 1:1 no matter how big
the player is drawn. A shield has live state. It hums, it takes a hit, it breaks,
it rebuilds. A bolt-on part can't show any of that.

**Sizing was the hard part.** The bubble has to sit *around* the drone at whatever
size the player is currently drawn at, and the first attempts kept landing on the
hull instead of clear of it, or getting clipped at the edges. Part of that was
just deriving the size instead of authoring it separately: the ring is drawn at a
fixed multiple of the player's width and height, and re-centred on the player's
centre every frame rather than pinned to the player's top-left. One constant,
`FIELD_SIZE_MULT`, controls the whole thing. When there's no capacitor bought, the
entity isn't hidden with a flag, it's just parked at size zero.

The clipping had a nastier cause, and it was in the sprite generator rather than
the game. The field's frame is 192 px, but the ring is authored in the chassis's
128-unit space like every other part — and the generator was *stretching* that
128-unit space to fill the 192 px frame instead of treating the bigger frame as a
wider window onto it. So the visible window stayed 128 units wide no matter how
big the frame got, the ring overran all four edges, and Pillow cropped it flat:
a bubble with four straight sides. The fix was one line — one authoring unit maps
to one frame pixel, leaving 26 px of margin around the ring — plus raising
`FIELD_SIZE_MULT` to `288/128` so the bubble kept exactly the on-screen diameter
it already had, just uncropped.

**And it's animated, not a single sprite.** The field is a 21-frame strip on one
atlas — 8 frames of hum, 4 of the impact bloom, 1 broken frame, 8 regen frames —
and it deliberately has *no* Animation component. Those four things behave
differently: the hum is a loop, the bloom is a one-shot, the broken frame is
static, and regen is a progress bar indexed by how full the shield is, not by
time. Only one of those is really a clip. So instead of four clips plus the
machinery to switch between them, the game writes the frame index itself every
frame from a pure function of the ship's state. The bloom reads its progress
through the hit window rather than the free-running loop phase — otherwise which
bloom frame you got would depend on *when* in the hum loop you happened to be hit.
The ring is also rotated to the bearing the damage came from, which does nothing
visible except aim the bloom, because everything else on it is a circle.
