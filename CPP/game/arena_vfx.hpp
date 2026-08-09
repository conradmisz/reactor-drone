#ifndef ARENA_VFX_HPP
#define ARENA_VFX_HPP

/**
 * Arena-transition VFX (Iteration 3, Lane E / #2).
 *
 * The outgoing arena's props used to vanish on one frame, which read as a glitch
 * against the 5s backdrop crossfade. They now crumble across that same window.
 *
 * Two halves live here:
 *   - the curves (`smoothstep`, `staggered_t`) — pure arithmetic, no engine types;
 *   - the prop bookkeeping — needs a ComponentStorage but not a window, so the
 *     "colliders are gone on the shift frame" and "nothing survives the window"
 *     rules are testable instead of being buried in `main()`'s lambdas.
 *
 * D76 — the crossfade's smoothstep moved here from a lambda in the render block,
 * so the props ease on literally the same curve as the backdrop.
 */

#include <algorithm>
#include <cstdint>
#include <vector>

#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/components.hpp"
#include "engine/ecs/entity_manager.hpp"

namespace arena_vfx {

// ---------------------------------------------------------------------------
// Curves — pure, unit-testable without an engine at all
// ---------------------------------------------------------------------------

/// Hermite ease, clamped to [0,1]. The backdrop crossfade's curve.
inline float smoothstep(float t) {
    const float x = std::clamp(t, 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

/**
 * Progress of prop `index` of `count`, at window progress `t01`.
 *
 * The props are fanned out across `stagger_span` of the window: prop 0 starts at
 * 0, the last prop starts at `stagger_span`, and every prop takes the remaining
 * `1 - stagger_span` to finish. So the whole set is guaranteed done at t01 == 1
 * — which is what makes "no prop survives the crossfade" a property rather than
 * a hope.
 *
 * `stagger_span` is clamped below 1 so the duration can never be zero.
 * Returns a clamped 0..1 progress; feed it to smoothstep() for the eased value.
 */
inline float staggered_t(float t01, int index, int count, float stagger_span) {
    // One prop has nobody to be staggered against, so it gets the whole window
    // rather than a compressed slice of it.
    if (count <= 1) return std::clamp(t01, 0.0f, 1.0f);
    const float span = std::clamp(stagger_span, 0.0f, 0.95f);
    const float slot = static_cast<float>(std::clamp(index, 0, count - 1)) /
                       static_cast<float>(count - 1);
    const float start = span * slot;
    return std::clamp((t01 - start) / (1.0f - span), 0.0f, 1.0f);
}

// ---------------------------------------------------------------------------
// Prop animation
// ---------------------------------------------------------------------------

/// A prop mid-animation, plus the authored geometry the scale curve multiplies.
/// `e` may already be dead (Lifetime got there first), so every use guards on the
/// component still existing rather than on the id.
struct AnimProp { Entity e; float cx, cy, w, h; };

/// Snapshot `props`' authored geometry (centre + size) for later scaling.
inline std::vector<AnimProp> capture_props(ComponentStorage& storage,
                                           const std::vector<Entity>& props) {
    std::vector<AnimProp> out;
    out.reserve(props.size());
    for (Entity e : props) {
        auto pos = storage.get_component<Position>(e);
        auto sz = storage.get_component<Size>(e);
        if (!pos.has_value() || !sz.has_value()) continue;
        const float w = sz->get().width, h = sz->get().height;
        out.push_back({e, pos->get().x + w * 0.5f, pos->get().y + h * 0.5f, w, h});
    }
    return out;
}

/**
 * Hand the outgoing arena's props over to the death animation.
 *
 * Ordering is the whole point and is not negotiable: the Collider comes off
 * first and unconditionally, so a pillar that is still visibly crumbling can
 * never block a shot, a dash or a path. Everything after that is decoration.
 * (The hazard's ContactDamage is left in place — with no Collider it never
 * reports a CollidedWith, so it can never fire, and `remove_component` is not
 * instantiated for it.)
 *
 * Debris only goes on props that had a Collider, i.e. obstacles and hazards.
 * The decorative wall ring has none — ~97 segments at radius 1400, mostly
 * off-camera, and emitting from all of them is what would blow the 2000-particle
 * budget. Measured peak for the worst arena: **336** live particles over the 5s
 * window (test_arena_vfx.cpp re-measures it and fails if it runs away).
 */
inline std::vector<AnimProp> teardown_props(ComponentStorage& storage,
                                            const std::vector<Entity>& props,
                                            uint8_t r, uint8_t g, uint8_t b,
                                            float window_seconds) {
    std::vector<AnimProp> dying;
    dying.reserve(props.size());
    for (Entity e : props) {
        const bool solid = storage.has_component<Collider>(e);
        storage.remove_component<Collider>(e);

        auto pos = storage.get_component<Position>(e);
        auto sz = storage.get_component<Size>(e);
        if (!pos.has_value() || !sz.has_value()) continue;
        const float w = sz->get().width, h = sz->get().height;
        dying.push_back({e, pos->get().x + w * 0.5f, pos->get().y + h * 0.5f, w, h});

        if (solid) {
            // Crumble rather than puff: the emitter rides the prop for its whole
            // death, so debris trickles out of a shrinking silhouette. Replaces
            // a hazard's ember vent (add_component overwrites).
            ParticleEmitter debris;
            debris.shape = EmitterShape::Circle;
            debris.radius = std::min(w, h) * 0.4f;
            debris.additive = false;
            debris.emission_rate = 14.0f;
            debris.particle_lifetime = 0.8f;
            debris.min_speed = 30.0f; debris.max_speed = 140.0f;
            debris.cone_half_angle = 180.0f;
            debris.start_size = 6.0f; debris.end_size = 1.0f;
            debris.start_r = r; debris.start_g = g; debris.start_b = b; debris.start_a = 235;
            debris.end_r = 40; debris.end_g = 40; debris.end_b = 45; debris.end_a = 0;
            storage.add_component<ParticleEmitter>(e, debris);
        }
        // Belt to the tick's braces: if the shift stalls (a phase change freezes
        // shift_timer) the props still retire on their own.
        storage.add_component<Lifetime>(e, Lifetime{window_seconds});
    }
    return dying;
}

/// Rewrite one prop's Size/Position — and its Collider, if it still has one, so a
/// half-grown prop is never an invisible wall — to `scale` about its authored
/// centre. False once the entity is gone.
inline bool scale_prop(ComponentStorage& storage, const AnimProp& p, float scale) {
    auto sz = storage.get_component<Size>(p.e);
    auto pos = storage.get_component<Position>(p.e);
    if (!sz.has_value() || !pos.has_value()) return false;
    const float w = p.w * scale, h = p.h * scale;
    sz->get().width = w;  sz->get().height = h;
    pos->get().x = p.cx - w * 0.5f;  pos->get().y = p.cy - h * 0.5f;
    if (auto col = storage.get_component<Collider>(p.e)) {
        col->get().width = w;  col->get().height = h;
    }
    return true;
}

/// Drive a whole set at window progress `t01`. `shrink` inverts the curve, so
/// the same call animates a teardown or an arrival.
inline void animate(ComponentStorage& storage, const std::vector<AnimProp>& set,
                    float t01, float stagger_span, bool shrink) {
    const int n = static_cast<int>(set.size());
    for (int i = 0; i < n; ++i) {
        const float k = smoothstep(staggered_t(t01, i, n, stagger_span));
        scale_prop(storage, set[static_cast<size_t>(i)], shrink ? 1.0f - k : k);
    }
}

/// Nothing of the old arena may outlive the crossfade, animated or not.
inline void destroy_all(EntityManager& em, ComponentStorage& storage,
                        std::vector<AnimProp>& set) {
    for (const AnimProp& p : set) storage.add_component<DestroyRequest>(p.e, DestroyRequest{});
    destroy_marked_entities(em, storage);
    set.clear();
}

}  // namespace arena_vfx

#endif  // ARENA_VFX_HPP
