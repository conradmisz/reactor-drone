---
id: 004
title: Additive particles bloom as box-shaped halos
status: fixed
severity: medium
area: render
opened: 2026-08-13
---

## Symptom

Every entity with an additive emitter (thruster, each shot, vents, boss aura)
radiates a square halo. Reported as "all of the shadows or light looks like a
box that is radiating from each entity... doesn't look very natural."

## Cause

Particles get `Color` + `Tint{additive}` and **no texture**. In
`render_walk`'s Color branch, an additive-tinted particle draws through
`draw_entity` with a `nullptr` texture — an SDL fill rect, i.e. a hard-edged
SQUARE. Those squares seed the bloom emissive target, and the chain's linear
downsample spreads the hard edge into a growing box.

Pre-existing since Tier 2. It became obvious only when v3 Tier 6b raised
`bloom.intensities` from .35/.30/.25/.20 to .55/.47/.39/.31 (+57%).

## Ruled Out

- Not the trails (D213): reproduces on entities with no trail.
- Not the GPU renderer: identical on `--classic-renderer`.

## Attempted and REJECTED: textured particles

Gave `ParticleEmitter` a `texture` field and pointed the 13 additive emitters
at the existing `v2/glow_disc_64.png`, so particles draw as soft discs. It
looked like the root-cause fix. It is a severe performance regression:

    60 frames, title screen:  1.4s baseline -> 34-40s textured  (~27x)

`draw_entity`'s texture path does SIX SDL texture-state calls per draw — set
colour/alpha/blend, draw, then reset all three — on a texture shared by every
particle. Each state change flushes SDL's batch, so N particles become N
flushed draw calls. Cost scales with live particle count (measured 0.28 ->
0.48 -> 0.68 s/frame as particles accumulated). Reverted in full.

A real fix needs batching, not a texture: draw all particles in one
`SDL_RenderGeometry` call the way `render_glow_lines` already does, with the
soft falloff in the mesh's UVs. That is a particle-renderer rewrite and was
not attempted.

## Mitigation shipped

`bloom.intensities` pulled back to .46/.40/.33/.27 and `default_intensity` to
0.38 — between the original .35 and Tier 6b's .55. The squares still seed the
bloom; they are just less amplified. This treats the symptom, deliberately.

## Fixed (2026-08-14, v3 Tier 9, D215)

The "real fix" this file called for is what shipped: `RenderSystem::render_particles`
builds ONE mesh of camera-facing quads UV'd across `v2/glow_disc_64.png` and hands
it to a single `SDL_RenderGeometry` call, exactly as `render_glow_lines` already
did for ribbons. `render_walk` now skips additive-tinted `Color` entities, so the
fill-rect path — the actual square — is never taken for a particle.

No performance regression. Canary command (3000 frames, dummy driver), 3 reps each:

    before: 144.74 / 141.12 / 137.26 s   (median 141.12)
    after:  139.36 / 139.39 / 139.25 s   (median 139.36)

Against the ~27x of the per-particle-texture attempt, batching costs nothing
measurable: six SDL state calls per FRAME instead of six per PARTICLE.

The `bloom.intensities` mitigation was reverted — back up to Tier 6b's
.55/.47/.39/.31 / 0.45, since there are no hard edges left to amplify.

One calibration was needed: `glow_disc_64.png` falls from alpha 243 at its centre
to 59 at quarter-radius, so a 1:1 quad reads as a ~2px dot, far dimmer than the
fill-rect it replaces. `DISC_SCALE` (2.5) sizes the quad so the solid core lands
on the particle's real footprint and the soft halo spills outside it.

**Uncovered a second, pre-existing bug while fixing this** — the UI's fills were
silently relying on particles setting the renderer draw blend mode. See ENGINE.md
section 5, first entry.
