/**
 * projectile_fizzle.hpp — playtest #6 item 7 (D232): no projectile just
 * blinks out. One short-lived emitter host per death, styled by the shot's
 * own identity flags, spawned wherever a shot ends (wall, end of range, or an
 * enemy for the types whose impact had no burst of its own).
 *
 * The tick_shields idiom: a free inline function, no new system. The host
 * entity carries only Position + Lifetime + ParticleEmitter — presentation
 * by construction.
 */
#pragma once

#include <algorithm>
#include <cstdint>

#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"

namespace fizzle {

/// Style selector: the same flags ProjectileTag carries.
struct Look {
    uint8_t r = 255, g = 200, b = 120;
    bool bolt = false;      // 55 Iron / Hailstorm primaries: a spark snap
    bool slag = false;      // Flak chunk / charge slug: an ember splash
    bool crescent = false;  // Moonshot: a wide, thin shimmer ring
};

inline void spawn(ComponentStorage& s, EntityManager& em,
                  float cx, float cy, const Look& look, float half_size) {
    Entity e = em.create_entity();
    s.add_component<Position>(e, Position{cx, cy});
    s.add_component<Lifetime>(e, Lifetime{0.07f});
    ParticleEmitter fx;
    fx.additive = true;
    fx.start_r = look.r; fx.start_g = look.g; fx.start_b = look.b; fx.start_a = 235;
    fx.end_r = static_cast<uint8_t>(look.r / 3);
    fx.end_g = static_cast<uint8_t>(look.g / 3);
    fx.end_b = static_cast<uint8_t>(look.b / 3);
    fx.end_a = 0;
    if (look.crescent) {
        // The arc dissolves: a ring of slow shimmer at the crescent's radius.
        fx.shape = EmitterShape::Circle;
        fx.radius = std::max(6.0f, half_size);
        fx.emission_rate = 500.0f;
        fx.particle_lifetime = 0.35f;
        fx.min_speed = 4.0f; fx.max_speed = 26.0f;
        fx.start_size = 3.0f; fx.end_size = 0.0f;
    } else if (look.slag) {
        // Molten splash: chunky embers, a touch of hang-time.
        fx.shape = EmitterShape::Point;
        fx.emission_rate = 600.0f;
        fx.particle_lifetime = 0.4f;
        fx.min_speed = 30.0f; fx.max_speed = 170.0f;
        fx.cone_half_angle = 180.0f;
        fx.start_size = std::min(9.0f, 4.0f + half_size * 0.2f);
        fx.end_size = 0.0f;
    } else {
        // Bolt snap: fast, short sparks — reads as the shot breaking up.
        fx.shape = EmitterShape::Point;
        fx.emission_rate = 450.0f;
        fx.particle_lifetime = 0.22f;
        fx.min_speed = 60.0f; fx.max_speed = 220.0f;
        fx.cone_half_angle = 180.0f;
        fx.start_size = 3.5f; fx.end_size = 0.0f;
    }
    s.add_component<ParticleEmitter>(e, fx);
}

}  // namespace fizzle
