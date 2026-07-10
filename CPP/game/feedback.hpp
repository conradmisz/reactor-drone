/**
 * feedback.hpp — pure hit-feedback math (v2).
 *
 * Screen shake uses a "trauma" scalar in [0,1] that producers add to on impact
 * and that decays over time; the actual pixel amplitude is trauma-squared so
 * small trauma is barely felt and large trauma is punchy. Flash converts a
 * Flash component into a Tint that fades from the flash colour back to the
 * identity tint over the flash's lifetime.
 *
 * Every tunable (max shake, decay rate, per-event trauma, flash duration and
 * colours) lives in assets/GameData.json and reaches these functions as a
 * parameter — nothing here is a balance constant.
 *
 * All functions are pure and side-effect free so they unit-test without a game
 * loop. Header-only (inline).
 */
#ifndef FEEDBACK_HPP
#define FEEDBACK_HPP

#include <algorithm>
#include <cstdint>
#include "engine/ecs/components.hpp"       // Tint
#include "player_components.hpp"           // Flash

namespace feedback {

/** Clamp trauma into [0,1]. */
inline float clamp_trauma(float t) {
    return std::max(0.0f, std::min(1.0f, t));
}

/** Accumulate an impact onto the current trauma, saturating at 1 (and 0). */
inline float add_trauma(float current, float amount) {
    return clamp_trauma(current + amount);
}

/** Pixel shake amplitude for a given trauma: max_px * trauma^2. Monotone,
 *  zero at trauma 0, max_px at trauma 1. Out-of-range trauma is clamped. */
inline float shake_amplitude(float trauma, float max_px) {
    float t = clamp_trauma(trauma);
    return max_px * t * t;
}

/** Decay trauma by dt seconds at rate_per_sec (linear), never below zero. */
inline float decay_trauma(float trauma, float dt, float rate_per_sec) {
    float t = trauma - rate_per_sec * dt;
    return t < 0.0f ? 0.0f : t;
}

/**
 * Tint for a flash: each channel interpolates from the flash colour (at full
 * life) back to 255 (at expiry), so an expired Flash yields exactly the identity
 * tint {255,255,255,255} and removing the component is visually a no-op.
 *
 * Deliberately opaque and non-additive. Fading the *alpha* to zero instead —
 * the obvious "additive glow" reading — makes the flashed entity disappear on
 * both render paths (SDL_SetTextureAlphaMod on the sprite path, modulate_color
 * on the colour-rect path) and then pop back the frame the Flash is removed.
 * Colour-modulating toward identity is continuous and never hides the entity.
 */
inline Tint flash_tint(const Flash& f) {
    float frac = (f.duration > 0.0f) ? (f.time_left / f.duration) : 0.0f;
    frac = std::max(0.0f, std::min(1.0f, frac));

    auto toward_identity = [frac](uint8_t c) -> uint8_t {
        return static_cast<uint8_t>(255.0f + (static_cast<float>(c) - 255.0f) * frac + 0.5f);
    };
    return Tint{toward_identity(f.r), toward_identity(f.g), toward_identity(f.b),
                255, /*additive=*/false};
}

}  // namespace feedback

#endif  // FEEDBACK_HPP
