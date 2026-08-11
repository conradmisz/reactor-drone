# Reactor Drone v2 — speaker view

Expansion of `video-presenter.md`. Talking points per item — pick 2 of the 3-4,
don't read all of them.

---

# Slide 1 — The core engine, and what I added to it

## Framing the baseline

- The engine is coursework: C++17 and SDL3 with a hand-rolled ECS, no Unity, no
  Godot, nothing that calls itself a framework.
- It was built a module at a time across a semester, each on a throwaway game —
  the ECS on moving squares, collisions on Asteroids, animation on a match-3,
  pathfinding on a tower defense.
- I forked the tower-defense build. So the honest starting line is: an engine
  that knew how to walk units down a path toward a base.
- The good line: *"I didn't start from an empty file, and I didn't start from an
  engine either. I started from a tower-defense demo and had to bend it into a
  twin-stick shooter."*

## The 150 / 31 / 27 number

- 208 source files: 150 byte-identical to the class engine, 31 modified, 27 new.
- It's measured, not remembered — `ENGINE.md` regenerates it by running `cmp`
  against the baseline repo, so it can't drift into a flattering story.
- The point of quoting it isn't that 27 is a big number. It's that I know which
  27, and can say the other 150 are untouched with a straight face.
- It also set the constraint for the whole project: the engine changes had to be
  additive, because everything I broke down there broke every class exercise too.

## RenderSystem — tint, additive, layers

- What it could do: take a texture, put it on screen at a position. That's it.
  Every sprite came out looking exactly like its PNG file.
- **Tint** is a component holding rgba plus an `additive` flag. That one
  component is why the same enemy sprite reads as a different creature in each
  arena — the art is pure luminance, the colour arrives at spawn from the arena.
- **RenderLayer** is just a sort key, but without it there's no reliable way to
  say "backdrop behind everything" — draw order was entity creation order.
- Small changes in code, load-bearing in look. Nothing in this game looks the way
  it does without those three.

## ParticleSystem

- Ported from the particles elective. The design decision worth mentioning:
  particles aren't a separate subsystem, they're **ordinary entities** with a
  Particle component — so the same movement and lifetime systems that handle
  bullets handle sparks.
- An emitter is a component too. Attach one to anything and it trails; the
  thruster plume is literally an emitter parented to the drone.
- Each particle interpolates colour and size across its own lifetime, which is
  what turns a dot into a spark that cools and shrinks.
- There's a global budget of 4000 live particles and past it they're silently
  dropped. Fine 99% of the time, and then you force-kill 96 enemies in one frame
  and the explosions get quietly thinner. I raised it from 2000; that's headroom,
  not a measurement.

## InputSystem

- It did keys, and it did the mouse in raw window pixels — which is the wrong
  space for everything (see slide 3).
- WASD got aliased onto the arrow keys, which sounds like nothing but is the
  difference between the game feeling like a PC game and feeling like homework.
- The interesting change is Escape. In the class engine Escape quit the program,
  full stop, buried inside the input system. Pause menus cannot exist in a world
  where Escape quits.
- So input stopped deciding and started reporting: it publishes "Escape happened
  this frame" and the game decides what that means depending on what's on screen.
  Same for mouse-down and mouse-up edges, which the UI needs.

## The UI layer

- The biggest single import — screens, a screen stack, widgets that track
  hovered/pressed/disabled, and fade transitions between screens.
- The nice part is that screens are **declared in the JSON**, not built in code.
  Twelve of them: main menu, run setup, save slots, settings, records, how to
  play, the shop, pause, the boss reward picker, the prestige offer.
- Widget colours are a style table, also data — so restyling the whole game is
  editing one block, not hunting for hardcoded colours.
- One gotcha worth a sentence if the pause menu is on screen: the gameplay HUD
  screen is never popped, so z-order sorts globally across every open screen. Get
  it wrong and the hull gauge draws on top of the pause panel. It did.

## What stayed untouched

- Collision — including all three strategies, brute force, uniform grid and
  quadtree — camera, animation, Lua, pathfinding, the resource manager, the timer.
- That's the bulk of the 150 identical files, and it's the argument that the
  class engine was actually well-factored: a game this different needed nothing
  from those.
- Amusing detail: Lua is still compiled in and a Lua state is still created,
  purely because the UI system's constructor wants one. This game keeps no menu
  scripts. It's inert rather than broken — an empty state resolves every callback
  to nil, which the UI reads as "no callback".

---

# Slide 2 — The game systems

## The framing (say this before the list, once)

- ~22k lines, and the thing that makes it survivable is that **nothing in that
  list calls anything else in that list.**
- Each system asks the storage for entities carrying the components it cares
  about and ignores everything else. Spawner makes enemies; seek moves whatever
  has a seek component; collision notices overlaps; hit reads overlaps and deals
  damage; death cleans up and drops loot. Four systems in a chain, none of them
  aware of the others.
- Anything genuinely global — the wave number, the difficulty — goes on the
  Blackboard, a string-keyed bag any system can read.
- Which is why adding features stayed cheap: the dash is one system that briefly
  multiplies velocity and marks you dangerous to touch. Shields are one system
  with a timer. Neither required an existing system to change.

## Combat

- **wave_spawner** reads a 30-row wave table out of the JSON — count, interval,
  which enemy types, health and speed multipliers. Waves 1-15 are fixed-count,
  16-30 are timed. Ring spawning is the design call: enemies arrive on a circle
  around the arena, so camping a corner doesn't work.
- **enemy_seek** is deliberately dumb — steer at the player. The pathfinding the
  engine inherited is unused here; an open arena doesn't need A*.
- **enemy_fire** is the moon shooters, and they're the difficulty curve in
  miniature: tier 1 lobs slow shots, tier 2 tracks you, tier 3 fires a piercing
  laser. They get introduced at waves 3, 9 and 18.
- **specialty** units are per theme, so the arena you're in tells you what you're
  about to fight — Bio-lab spitters leave poison patches, Foundry miners drop
  mines, Core bulwarks are armoured, Prism splitters break into two when killed.
- **player_aim / player_fire** are the twin-stick half: aim writes a rotation
  angle, fire spawns projectiles along it, applying fire rate, spread, extra
  shots and ricochet from whatever you've bought.
- **projectile_hit** and **damage_apply** are split on purpose — one decides
  *that* damage happened, the other decides what it does to hull versus shields.
  Enemies and the player go through the same resolver.

## Boss & abilities

- **boss_system** runs waves 10, 20 and 30. A boss is an enemy with a lot of
  health, a summon timer that drips in adds, and an attack themed to the arena
  it spawned on — the Foundry boss drops mines, the Bio-lab boss spits poison.
- The finale transforms the arena into a black hole at half health, mid-fight.
  It's the one moment the map changes while you're in it.
- **active_items** are the reward: kill a boss, pick one of three abilities on a
  30-second cooldown. Homing missiles, a four-beam laser that sweeps 360°, or a
  forcefield.
- The forcefield is the interesting one because it fires *itself* — it triggers
  automatically below 20% hull, heals you and holds enemies out. It's a safety
  net you can't forget to press.

## Economy

- **shop_system** is a panel every wave and the full shop every fifth. Eight
  upgrade lines: hull, shields, thrusters, fire rate, damage, extra shots,
  projectile range, ricochet.
- Each line has a stack cap and a 1.5× price growth, so the same upgrade costs
  more every time you take it — that's what stops one line running away with the run.
- **pickup** is why the between-waves phase isn't frozen: credits drop on the
  floor and you fly a victory lap collecting them.
- **sustain_spawn** trickles pickups mid-wave so a long fight doesn't become a
  war of attrition you can't win.

## Feel

- **flash** is the white hit-flash, and it's deliberately the last system each
  frame to write a Tint — so a hit always wins over whatever ambient colour the
  arena wanted.
- **shield** regenerates on a quiet timer, and it ticks *before* damage in the
  frame so an incoming hit restarts the timer rather than the timer overwriting
  the hit. That ordering was a bug first.
- **dash** — Space, 10-second cooldown, 0.15 seconds at 900 units a second, and
  it damages what you pass through. Escape and attack in one button.
- **arena_vfx** is the crossfade between arenas, with the props swapping
  mid-fade and a shockwave. It only fires on a cleared wave, which is also the
  only reason the particle budget survives it.

## Readout & persistence

- **game_hud**, **minimap** and **pause_stats** exist because the game got too
  complicated to infer — you can't guess your shield state from the sprite.
- **meta_save** is two numbers: lifetime score and prestige level. Lifetime score
  unlocks ships across runs.
- **run_save** is the mid-run save: wave, credits, hull, gear, ship, seed. What
  it deliberately is *not* is a snapshot of the world — no entity list, no
  positions.
- Both read once at startup, and that's the whole trick. A save system that
  reads from inside a running system would have broken deterministic replay,
  which was the one property I wasn't willing to trade.

---

# Slide 3 — The systems that fought back

## Aiming at the cursor

- The symptom: the drone pointed *near* the mouse, never *at* it. Slightly off in
  the middle of the screen, badly off at the edges, and differently off if you
  resized the window — which is the signature of a scale error, not a maths error.
- The cause was three stacked transforms with only two being undone. The window
  is letterboxed, so window pixels aren't renderer pixels. The camera follows the
  player with a zoom and a lookat. And world space is bottom-left origin while
  the screen is top-left, so Y is flipped.
- The fix was ordering and exactness: run the pointer through
  `SDL_RenderCoordinatesFromWindow` first to get renderer space, then invert the
  camera's affine transform precisely — the same equation the camera applies,
  algebraically reversed, rather than something that looked about right.
- The other half was admitting there are **two mouse positions, not one**. A
  screen-space one for clicking buttons and a world-space one for aiming, both
  published every frame. They're different spaces and reusing one for the other
  is how you get buttons that click a few pixels off.
- Worth saying: it went from "feels laggy" to exact with no smoothing, no
  interpolation, no filter. It was never a smoothness problem, it was a wrong
  answer arriving on time.

## Making things glow

- First attempt was the obvious one: draw more sprites on top of each other.
  That made things *opaque*, not *bright* — normal alpha blending replaces what's
  underneath, so ten stacked flares look exactly like one.
- Glow needs **additive** blending, where overlapping pixels sum toward white.
  That's the whole trick: brightness accumulates where particles pile up, so a
  dense burst has a hot core and a soft edge for free, with no extra art.
- The art had to change to match — soft edgeless discs, not shapes with outlines.
  Bullets in this game are glow blobs, not sprites, which also stops them reading
  as solid objects you could dodge behind.
- Then per-particle colour lerp over lifetime does the rest: spawn hot and white,
  cool toward the arena's colour, shrink, die. It's three numbers in a component
  and it's most of what people mean when they say a game "feels good".
- The cost is the budget. Additive glow is only convincing at high particle
  counts, which is why the cap moved from 2000 to 4000 and why I had to actually
  measure a boss fight with every ability firing.

## The kit that trailed the ship

- Context first: bought upgrades are visible. Buy hull plating and plating
  appears bolted to the drone; there's a shield field ring and an item aura too.
- The bug: they lagged exactly one frame behind the hull. Barely visible while
  cruising, obvious during a dash — the faster you moved the wider the gap — and
  between waves they froze in place entirely while the ship flew off without them.
- Root cause was pure frame ordering. The follow code copied the player's
  position right after aiming, but movement, the arena clamp and the obstacle
  push-out all ran *later* in the frame. So render always drew the parts at where
  the ship had been, not where it ended up.
- The fix was moving one call to after the push-out, and adding it to the
  between-waves branch, which never had it at all — one root cause, both symptoms.
- The tempting wrong fix was smoothing the followers toward the ship, which would
  have hidden it at low speed and looked worse during a dash. They're copies, not
  physics — they should be exact, and the answer was to copy the right number.
