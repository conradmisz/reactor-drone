#ifndef SCAR_SYSTEM_HPP
#define SCAR_SYSTEM_HPP

#include <cstdint>
#include <vector>

#include <SDL3/SDL.h>

#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/fx_events.hpp"

/**
 * ScarSystem — the arena remembers the run (#6, Lane V, D145).
 *
 * One accumulation texture the size of the arena, never cleared between frames:
 * every death stamps a scorch into it, and by wave 25 the floor tells the story
 * of the run. Cleared only on an arena shift, so each arena keeps its own history.
 *
 * RENDER-ONLY, the same hard contract as the resonance grid: it consumes the
 * `fx.scar_stamps` list published sim-side and writes pixels. Nothing reads it
 * back, it owns no RNG, and the replay canary cannot see it.
 *
 * The stamp sprite is generated ONCE at runtime into a small texture (a radial
 * scorch falloff) rather than loaded from `assets/images/v2/`.
 * ponytail: that keeps the whole feature inside this file pair — no new committed
 * binary, no generator run, no sidecar. If the art direction ever wants authored
 * scorch shapes, `set_stamp_texture()` takes one and the procedural fallback goes
 * away with it.
 *
 * MCU headroom: one framebuffer-sized target texture, and stamps per frame are
 * bounded twice — by `fx_events::MAX_PER_FRAME` at the publisher and by
 * `max_stamps_per_frame` here, so a mass-death frame costs a known number of
 * blits rather than "however many died".
 */
class ScarSystem {
public:
    ~ScarSystem();

    ScarSystem() = default;
    ScarSystem(const ScarSystem&) = delete;
    ScarSystem& operator=(const ScarSystem&) = delete;

    /**
     * Size the accumulation texture to an arena and place it in the world.
     * `origin_x/y` is the world position of the texture's bottom-left corner.
     * Re-callable; a size change rebuilds (and therefore clears) the texture.
     * Returns false if the texture could not be created — the system then draws
     * nothing rather than crashing a run over a cosmetic layer.
     */
    bool configure(SDL_Renderer* renderer, int width, int height,
                   float origin_x, float origin_y);

    void set_tuning(int max_stamps_per_frame, float alpha) {
        max_per_frame_ = max_stamps_per_frame;
        alpha_ = alpha;
    }

    /// Wipe the accumulated history. Called on an arena shift.
    void clear(SDL_Renderer* renderer);

    /// Stamp this frame's marks into the texture, then draw it under the entities.
    void update_and_render(SDL_Renderer* renderer, const Blackboard& blackboard,
                           const std::vector<fx_events::Stamp>& stamps);

    // --- inspection, for tests ---
    int stamps_applied() const { return stamps_applied_; }
    bool ready() const { return target_ != nullptr; }

private:
    bool ensure_stamp_texture(SDL_Renderer* renderer);

    SDL_Texture* target_ = nullptr;   // the accumulation buffer (never cleared)
    SDL_Texture* stamp_ = nullptr;    // the procedural scorch sprite
    int width_ = 0, height_ = 0;
    float origin_x_ = 0.0f, origin_y_ = 0.0f;
    int max_per_frame_ = 16;
    float alpha_ = 0.55f;
    int stamps_applied_ = 0;          // cumulative, for tests and budgeting
};

#endif  // SCAR_SYSTEM_HPP
