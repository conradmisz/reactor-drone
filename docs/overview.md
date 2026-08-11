# Reactor Drone 


---

## 1 — Engine additions

Built module by module:
ECS spine → timers/input/blackboard → resource manager + HUD → camera + debug →
collisions → spatial partitioning → Lua → atlases + animation → pathfinding.
I forked from the tower-defense build.

**208 source files: 150 byte-identical to the class engine, 31 modified, 27 new.**

**The class engine**
- **EntityManager / ComponentStorage / Blackboard** — the ECS spine: ids, typed component maps, and one shared bag for anything global.
- **Timer** — the frame clock.
- **InputSystem** — keys and a raw window-pixel mouse.
- **MovementSystem / RotationSystem / WrapSystem / LifetimeSystem** — position, facing, screen wrap, and things that expire.
- **CollisionSystem** — plus brute-force, uniform-grid and quadtree strategies.
- **RenderSystem** — draws a sprite exactly as its PNG, one flat pass.
- **ResourceManager** — texture loading and caching.
- **CameraSystem / CameraControlSystem** — follow, zoom, lookat.
- **AnimationSystem** — sprite-sheet clips on a timer.
- **HUDSystem / DebugHUDSystem / ScreenshotSystem** — text overlays, the debug readout, and headless captures.
- **ScriptSystem + Lua** — embedded scripting and engine bindings.
- **Pathfinding + tile maps** — grid routing, inherited from the tower defense.

**Added to game**
- **ParticleSystem** — emitters that spawn particles as ordinary entities, each one fading in colour and shrinking as it ages, against a 4000-particle budget.
- **Tint** — per-entity colour and alpha, with an additive mode so glows stack instead of painting over each other.
- **RenderLayer** — an explicit draw order, so backdrops stay behind everything.
- **Tiled backdrops** — scrolling background layers with a crossfade between arenas.
- **UISystem** — widgets that know they're hovered, pressed or disabled.
- **UIRenderSystem** — draws the panels, labels, buttons, sliders and checkboxes, and shrinks any text that would overflow its widget.
- **ScreenStackSystem** — a stack of screens, so opening the shop over the game and closing it again is one push and one pop.
- **Logical-space mouse** — the pointer converted out of window pixels into the world, published separately from the screen-space one the buttons use.
- **Escape as an event** — it used to quit the program outright; now the game decides what it means, which is the only reason a pause menu can exist.


---

## 2 — The game systems

1. Nothing calls anything
2. Each system looks for entities carrying componenets that are relevant to it
3. Anything global goes to blackboard

**Combat**
- **wave_spawner** — reads the wave table and spawns enemies on a ring around the arena, so there's no safe corner.
- **enemy_seek** — steers anything with a seek component toward the player.
- **enemy_fire** — Fires on a per-type cooldown, escalating from slow shots to tracking to piercing lasers.
- **specialty** — the per-theme units: poison patches, dropped mines, armour, and splitters that spawn two smaller drones on death.
- **player_aim** — turns the drone to face the cursor.
- **player_fire** — spawns projectiles on the aim angle, honouring fire rate, spread, extra shots and ricochet.
- **projectile_hit** — reads collision results and applies damage; **damage_apply** resolves it against hull and shields.
- **enemy_death** — score, XP, the explosion, and the loot drop.

**Boss & abilities**
- **boss_system** — the wave-10/20/30 fight: summon timer, arena-themed attack, and the black-hole transform at half health on the finale.
- **active_items** — the three boss rewards on a 30 s cooldown: homing missiles, a 360° four-beam laser, and a forcefield that auto-fires below 20% hull.

**Economy**
- **shop_system** — the upgrade panel and the full shop: eight stacking upgrade lines with a 1.5× price growth, plus gear and consumables.
- **pickup / item_system / sustain_spawn** — credits and drops on the floor, equipped gear effects, and the mid-wave health/ammo trickle.

**Feel**
- **flash** — the white hit-flash; the last thing each frame to write a Tint.
- **shield** — regen on a quiet timer that any incoming hit restarts.
- **dash** — Space, 10 s cooldown, 0.15 s at 900 u/s, and it damages what you pass through.
- **arena_vfx / parallax** — the crossfade between arenas with props swapping mid-fade, and the scrolling backdrop layers.

**Readout & persistence**
- **game_hud / minimap / pause_stats** — hull and shield gauges, the arena map, and the pause screen's stat sheet.
- **meta_save / run_save** — two small JSON files: lifetime score plus prestige, and resumable run state. Neither touches the simulation.

---

## 3 - Difficulties, cut faetures, bugs 

**Aiming at the cursor.** The drone pointed *near* the mouse but never exactly at
it, and the error got worse toward the edges of the screen. The mouse arrives in
window pixels but the game world is somewhere else entirely the window is
letterboxed, the camera is zoomed and follows the player, and the world's Y axis
runs the opposite way to the screen's so all three of those had to be undone,
in order, before the cursor's real position fell out.

**Making things glow.** Layering more sprites just made things solid, not bright
and neon like I wanted. Normally a sprite covers up whatever is underneath it,
so ten stacked flares look the same as one. Glow needed the *additive* draw mode,
where overlapping pixels add together and get brighter instead of covering each
other, so a dense burst gets a hot white core on its own.

**Upgrade items trailing the ship.** Bought upgrades are drawn as parts bolted to
the drone — and they lagged one frame behind, worst during a dash, and froze
entirely between waves. The follow code copied the player's position right after
aiming, except for movement, the arena clamp and the obstacle push-out all ran *later*
in the frame, so the parts were always rendering the previous frame's position.

**Multiplayer** I think this game could work well with multiplayer, but way too
ambitious.