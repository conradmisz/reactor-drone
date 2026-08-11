/**
 * BloomSystem — render-target bloom for the classic SDL_Renderer path (v3 Tier 1).
 *
 * The world is rendered into an offscreen scene target instead of the
 * backbuffer. At resolve time the scene is copied down a chain of half-size
 * render targets — SDL's linear texture filtering makes every halving a free
 * box blur — and each level is composited back over the backbuffer with
 * additive blending. On a near-black field the emissive sprites are the only
 * bright content, so no bright-pass threshold is needed: the chain *is* the
 * halo.
 *
 * Degrades to a no-op: if any target texture cannot be created (dummy or
 * offscreen video drivers reject render targets on some platforms), or the
 * config disables bloom, begin()/resolve() do nothing and the frame renders
 * exactly as it did before this system existed. Headless replay and the
 * screenshot path are therefore untouched.
 *
 * Presentation-only by construction: no system reads anything this writes, so
 * the deterministic replay canary cannot move.
 */
#pragma once

#include <SDL3/SDL.h>
#include <vector>

#include "engine/ecs/systems/bloom_math.hpp"

/** Authored in GameData.json under the optional top-level "bloom" block. */
struct BloomConfig {
    bool enabled = true;
    int levels = 4;                      // downsample chain depth
    std::vector<float> intensities;      // per-level additive weight [0,1]
    float default_intensity = 0.35f;     // for levels beyond the authored list
};

class BloomSystem {
public:
    /**
     * Create the scene target and downsample chain at the logical surface
     * size. On any failure the system disables itself and frees what it made.
     */
    BloomSystem(SDL_Renderer* renderer, int logical_w, int logical_h,
                const BloomConfig& config);
    ~BloomSystem();

    BloomSystem(const BloomSystem&) = delete;
    BloomSystem& operator=(const BloomSystem&) = delete;

    /** Redirect rendering into the scene target. No-op when disabled. */
    void begin();

    /**
     * Restore the backbuffer target, draw the scene 1:1, then composite the
     * blur chain additively over it. No-op when disabled. Call after the last
     * world/UI draw and before screenshot/present.
     */
    void resolve();

    /** True when targets exist and the config enables bloom. */
    bool active() const { return active_; }

private:
    SDL_Renderer* renderer_;
    BloomConfig config_;
    SDL_Texture* scene_ = nullptr;               // full-size scene target
    std::vector<SDL_Texture*> chain_;            // halving blur targets
    int w_ = 0, h_ = 0;
    bool active_ = false;

    void destroy_targets();
};
