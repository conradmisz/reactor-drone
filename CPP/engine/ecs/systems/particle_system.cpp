#include "engine/ecs/systems/particle_system.hpp"
#include <cmath>
#include <algorithm>
#include <vector>

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegToRad = kPi / 180.0f;

// Linear interpolation with exact endpoints. The project targets C++17, so
// lerp (C++20) is unavailable; this helper guarantees lerp(a,b,0)==a and
// lerp(a,b,1)==b exactly, which the interpolation boundary tests rely on.
float lerp(float a, float b, float t) {
    if (t <= 0.0f) return a;
    if (t >= 1.0f) return b;
    return a + t * (b - a);
}

// Round a float to the nearest [0,255] color channel. Boundaries are exact:
// lerp(a, b, 0) == a and lerp(a, b, 1) == b, and those integral
// values round-trip through this helper unchanged.
uint8_t to_channel(float value) {
    float rounded = std::round(value);
    if (rounded < 0.0f) rounded = 0.0f;
    if (rounded > 255.0f) rounded = 255.0f;
    return static_cast<uint8_t>(rounded);
}

} // namespace

// ---------------------------------------------------------------------------
// Interpolation helpers
// ---------------------------------------------------------------------------

Color lerp_color(Color start, Color end, float t) {
    return Color{
        to_channel(lerp(static_cast<float>(start.r), static_cast<float>(end.r), t)),
        to_channel(lerp(static_cast<float>(start.g), static_cast<float>(end.g), t)),
        to_channel(lerp(static_cast<float>(start.b), static_cast<float>(end.b), t)),
        to_channel(lerp(static_cast<float>(start.a), static_cast<float>(end.a), t))
    };
}

float lerp_size(float start, float end, float t) {
    return lerp(start, end, t);
}

// ---------------------------------------------------------------------------
// Shape sampling helpers
// ---------------------------------------------------------------------------

Position sample_point(const ParticleEmitter& emitter, Position origin, std::mt19937& rng) {
    switch (emitter.shape) {
        case EmitterShape::Line: {
            std::uniform_real_distribution<float> u(0.0f, 1.0f);
            float s = u(rng);
            return Position{origin.x + s * emitter.line_dx,
                            origin.y + s * emitter.line_dy};
        }
        case EmitterShape::Circle: {
            std::uniform_real_distribution<float> u01(0.0f, 1.0f);
            std::uniform_real_distribution<float> angle_dist(0.0f, 2.0f * kPi);
            // sqrt for a uniform areal distribution within the disk
            float r = emitter.radius * std::sqrt(u01(rng));
            float theta = angle_dist(rng);
            return Position{origin.x + r * std::cos(theta),
                            origin.y + r * std::sin(theta)};
        }
        case EmitterShape::Point:
        case EmitterShape::Cone:
        default:
            return origin;
    }
}

Velocity sample_velocity(const ParticleEmitter& emitter, std::mt19937& rng) {
    float lo = std::min(emitter.min_speed, emitter.max_speed);
    float hi = std::max(emitter.min_speed, emitter.max_speed);
    std::uniform_real_distribution<float> speed_dist(lo, hi);
    float speed = speed_dist(rng);

    float base = emitter.direction * kDegToRad;
    float half = emitter.cone_half_angle * kDegToRad;
    std::uniform_real_distribution<float> spread(-half, half);
    float theta = base + spread(rng);

    return Velocity{speed * std::cos(theta), speed * std::sin(theta)};
}

// ---------------------------------------------------------------------------
// ParticleSystem
// ---------------------------------------------------------------------------

ParticleSystem::ParticleSystem(int max_particles, uint32_t seed)
    : max_particles_(max_particles), rng_(seed) {}

void ParticleSystem::update(ComponentStorage& storage, EntityManager& entity_manager, float dt,
                            bool emit) {
    // --- 1. Count live particles (for the global budget) ---
    int live = static_cast<int>(storage.entities_with_component<Particle>().size());

    // --- 2. Emit from each active emitter ---
    auto emitters = emit ? storage.entities_with_component<ParticleEmitter>()
                         : std::vector<Entity>{};
    for (Entity host : emitters) {
        auto emitter_opt = storage.get_component<ParticleEmitter>(host);
        if (!emitter_opt.has_value()) continue;
        ParticleEmitter& emitter = emitter_opt->get();
        if (!emitter.active) continue;

        // Accumulate fractional emission so the average rate is frame-rate
        // independent. n whole particles emit this frame; the remainder carries.
        emitter.emit_accumulator += emitter.emission_rate * dt;
        int n = static_cast<int>(std::floor(emitter.emit_accumulator));
        if (n > 0) {
            emitter.emit_accumulator -= static_cast<float>(n);
        }

        // Optional per-emitter cap on spawns this frame (0 = global budget only).
        if (emitter.max_particles > 0) {
            n = std::min(n, emitter.max_particles);
        }

        // v2: spawn relative to the host position plus the emitter's offset, so an
        // emitter attached to a moving host (projectile trail, thruster) emits from
        // the right spot as the host moves.
        Position origin{emitter.offset_x, emitter.offset_y};
        if (auto pos = storage.get_component<Position>(host); pos.has_value()) {
            origin.x = pos->get().x + emitter.offset_x;
            origin.y = pos->get().y + emitter.offset_y;
        }

        for (int i = 0; i < n; ++i) {
            if (live >= max_particles_) break;  // global budget reached: silent drop

            Entity p = entity_manager.create_entity();
            storage.add_component(p, sample_point(emitter, origin, rng_));
            storage.add_component(p, sample_velocity(emitter, rng_));
            storage.add_component(p, Color{emitter.start_r, emitter.start_g,
                                           emitter.start_b, emitter.start_a});
            storage.add_component(p, Size{emitter.start_size, emitter.start_size});
            // v2: additive emitters tag their particles for glow blending. The Tint
            // is identity-colour so modulate_color keeps the interpolated Color; only
            // the additive blend mode is applied by the RenderSystem.
            if (emitter.additive) {
                storage.add_component(p, Tint{255, 255, 255, 255, true});
            }

            Particle particle;
            particle.age = 0.0f;
            particle.lifetime = emitter.particle_lifetime;
            particle.start_r = emitter.start_r;
            particle.start_g = emitter.start_g;
            particle.start_b = emitter.start_b;
            particle.start_a = emitter.start_a;
            particle.end_r = emitter.end_r;
            particle.end_g = emitter.end_g;
            particle.end_b = emitter.end_b;
            particle.end_a = emitter.end_a;
            particle.start_size = emitter.start_size;
            particle.end_size = emitter.end_size;
            particle.gravity_x = emitter.gravity_x;
            particle.gravity_y = emitter.gravity_y;
            storage.add_component(p, particle);

            ++live;
        }
    }

    // --- 3. Age, interpolate, apply gravity, expire ---
    auto particles = storage.entities_with_component<Particle>();
    for (Entity e : particles) {
        auto particle_opt = storage.get_component<Particle>(e);
        if (!particle_opt.has_value()) continue;
        Particle& p = particle_opt->get();

        p.age += dt;
        if (p.age >= p.lifetime) {
            storage.add_component(e, DestroyRequest{});  // deferred destruction
            continue;
        }

        float t = p.age / p.lifetime;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;

        if (auto color = storage.get_component<Color>(e); color.has_value()) {
            color->get() = lerp_color(
                Color{p.start_r, p.start_g, p.start_b, p.start_a},
                Color{p.end_r, p.end_g, p.end_b, p.end_a}, t);
        }
        if (auto size = storage.get_component<Size>(e); size.has_value()) {
            float sz = lerp_size(p.start_size, p.end_size, t);
            size->get().width = sz;
            size->get().height = sz;
        }
        if (auto vel = storage.get_component<Velocity>(e); vel.has_value()) {
            vel->get().dx += p.gravity_x * dt;
            vel->get().dy += p.gravity_y * dt;
        }
    }
}
