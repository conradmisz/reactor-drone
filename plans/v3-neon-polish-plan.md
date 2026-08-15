# Reactor Drone v3 — Neon Polish plan (branch: visual-overhaul)

Goal: the Wii Play *Laser Hockey* read — near-black field, few crisp lines, real bloom,
motion trails, rock-solid frame delivery. Five tiers, each independently shippable and
committed separately. Every tier leaves the standing gates green:

- Zero warnings (`-Wall -Wextra -Wpedantic`, Lua `tmpnam` exempt).
- 100% ctest (`runTestsAll.py`).
- Deterministic replay canary — **must fire, not idle** (corrected 2026-08-13):
  `SDL_VIDEODRIVER=dummy ./CPP/build/game/game --seed 42
  --keys $(seq -f '%g:SPACE' 10 4 2990) --stopframe 3000` prints a byte-identical
  summary before/after the tier. All tiers are presentation-only, so it must never
  move. The original `--keys 10:SPACE` form was INERT: one SPACE press only starts
  the run, ending `score 0 / units 0`, so it never reached the hit-stop path Tier 3
  itself flagged as the risk. Expected now: `score 100, units 24, wave 1`.
- `ENGINE.md` updated in the same commit as any engine change (its maintenance rule).

Verification per tier additionally includes `--screenshot` captures at fixed frames,
compared by eye (screenshots land in `logs/`).

---

## Tier 0 — Frame delivery + art restraint  *(no new components)*

1. **Vsync**: `SDL_SetRenderVSync(renderer.get(), 1)` after renderer creation in
   `main.cpp`. Timer keeps its busy-wait as a floor; measured `delta_time` already
   drives windowed play, `--seed` runs stay deterministic (`set_deterministic` is
   seed-gated). Nothing else changes.
2. **Pre-scaled sprites**: enemy/player art is 512² drawn at ~40px — a 12:1 bilinear
   minification shimmer. In `make_sprites.py`, render at 4× the largest in-game draw
   size and downsample with Pillow LANCZOS (offline supersampling). Commit regenerated
   PNGs. Generators stay offline.
3. **Backdrop restraint**: in `make_backdrops.py`, kill the busy near layer — flat
   near-black clear, one thin grid on the mid layer, sparse stars far. Regenerate,
   commit. Per-palette accent count drops: body colors converge on dark desaturated
   base + ONE neon hue + white highlights.

## Tier 1 — Render-target bloom  *(one new engine file)*

`CPP/engine/ecs/systems/bloom_system.{hpp,cpp}` (~150 lines):
- On init: create the scene target (logical 980×660) plus a downsample chain of
  4 render-target textures (÷2 each, linear scale mode).
- Frame: `SDL_SetRenderTarget(scene)` before world render → after world render,
  blit scene down the chain (linear filtering = free box blur per step), then
  composite each level back over the backbuffer with `SDL_BLENDMODE_ADD` and a
  per-level intensity from GameData (`"bloom": {levels, intensities[], enabled}`).
- No bright-pass: emissives are already the only bright content on near-black.
- `main.cpp`: two calls (`bloom.begin()` before `render_layers`, `bloom.resolve()`
  after `ui_render_system.render` — UI *inside* bloom keeps menus glowing like the
  Laser Hockey HUD; if it reads mushy, move resolve before UI. Decide by screenshot).
- Headless/dummy driver must keep working: if target creation fails, bloom disables
  itself and the pipeline is exactly today's (screenshot path unaffected).
- Pure math (chain sizes, intensity clamp) in `bloom_math.hpp` + unit test.

## Tier 2 — Emissive separation  *(asset convention + render bucket)*

1. Generators emit two PNGs per sprite: `X_base.png` (dark hull, crisp edge) and
   `X_glow.png` (neon lines only, white, tinted at runtime like enemy sprites).
2. New optional component field: `Images::glow_texture` (or a parallel `GlowSprite`
   component if storage is cheaper — decide at the code). RenderSystem draws base in
   the normal pass; glow sprites are drawn additively into a separate **emissive
   target** that seeds the bloom chain instead of the full scene.
3. Bloom then reads ONLY emissive content → cores blow to white, hulls stay dark.
   This is the "hot vs soft" upgrade.
4. Backward compatible: a sprite with no `_glow` sidecar renders exactly as today.

## Tier 3 — Motion  *(two small components)*

1. **`Trail` component** + `trail_system`: ring buffer of the last N transforms
   (N, spacing, fade from GameData); redrawn behind the entity as decaying additive
   sprites into the emissive target. Player drone, projectiles, dash. Pure ring-buffer
   math in a header + unit/property test.
2. **Hit-stop**: on kill/boss-hit, freeze the sim (not the renderer) for 40–80 ms.
   Implemented as a `hitstop_timer` the main loop checks alongside `sim` —
   frame-count based so replays stay deterministic (it consumes frames, not wall
   time... NO: hit-stop changes frame counts → changes `--stopframe` results. Instead:
   hit-stop scales `delta_time` to 0 for K frames — RNG draws still happen in fixed
   order, systems integrate zero motion, frame counter still advances. Canary safe
   only if K frames of zero-dt produce identical draw counts — they do, since draws
   are per-update not per-dt. Verify canary explicitly — with the FIRING canary;
   an idle run never raises `hitstop_left` and proves nothing.)
3. **Camera punch**: existing trauma system gains a zoom impulse (decays with trauma).
4. **Impact squash**: on hit, a 100 ms scale-x/scale-y pulse via existing `Size` +
   a `Squash` timer folded into `Flash`-style feedback (reuse `flash_system` idiom).

## Tier 4 — GPU render states + shaders  *(toolchain cost lands here)*

SDL 3.5 `SDL_CreateGPURenderer` + `SDL_CreateGPURenderState` — custom SPIR-V fragment
shaders on ordinary SDL_Renderer draws. NVIDIA + Vulkan ICD present on this machine.

1. Renderer creation becomes `SDL_CreateGPURenderer(NULL, window)` behind a
   `--classic-renderer` escape hatch (and automatic fallback when GPU init fails —
   dummy/offscreen drivers keep the classic path, so headless testing is untouched).
2. Shaders authored in GLSL, compiled offline to SPIR-V with `glslc` (install via
   `shaderc` / Vulkan SDK, checked into `assets/shaders/` as `.spv` — same
   offline-generator discipline as the PNGs; build never compiles shaders).
3. Effects, each data-gated in GameData and all presentation-only:
   - Proper thresholded Gaussian bloom (replaces Tier 1 chain when GPU path active;
     Tier 1 stays as the classic-renderer fallback).
   - Chromatic aberration + vignette (subtle, always on).
   - Radial shockwave distortion on boss death / arena shift (reads the existing
     `arena-vfx` hook's state).
   - Color-grade LUT per arena palette (32³ LUT PNGs from a new offline generator).
4. `bloom_system` grows a strategy split: `classic` (Tier 1) / `gpu` — selected once
   at init. ENGINE.md documents which path each build takes.

## Tier 5 — Neon line renderer  *(the permanent crispness fix)*

`CPP/engine/ecs/systems/line_render_system.{hpp,cpp}` on `SDL_RenderGeometry`:
- Polyline → triangle-strip ribbon with per-vertex UV driving a soft-falloff glow
  texture (reuse `glow_disc` as the 1D falloff), additive, camera-transformed,
  resolution-independent at any zoom.
- Pure geometry math (`line_mesh_math.hpp`: miter joins, ribbon widths, UV walk)
  unit + property tested — this is the real engineering of the tier.
- Consumers, in order: arena boundary circle (currently invisible geometry), obstacle
  outlines, laser beams (replaces the beam sprite), enemy shot tracers, dash arc.
- `LineGlow` component: `points[], width, color, additive` — authored from GameData
  for static shapes, written by systems for dynamic ones.

---

## Order & git

Tiers land in order 0→5, one commit each (Tier 4 may be several: renderer swap /
each effect). After each: gates + canary + screenshots. Push `visual-overhaul` with
`-u` on first push. Merging waits for the pre-merge branch (user's call).
