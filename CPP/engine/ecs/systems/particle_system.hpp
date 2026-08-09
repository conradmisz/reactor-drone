/**
 * ParticleSystem - data-driven ECS particle simulation (Option-020)
 *
 * Each frame the ParticleSystem:
 *   1. Spawns particles from every active ParticleEmitter at `emission_rate`
 *      particles/second (a fractional accumulator on the emitter carries the
 *      remainder across frames so the average rate is frame-rate independent).
 *   2. Ages every Particle by dt.
 *   3. Interpolates each particle's Color and Size from its start endpoint to
 *      its end endpoint by t = age / lifetime (clamped to [0,1]).
 *   4. Applies the particle's gravity to its Velocity.
 *   5. Attaches a DestroyRequest to any particle whose age >= lifetime
 *      (deferred destruction — the destruction pipeline removes it end-of-frame).
 *
 * A global live-particle budget bounds allocation: once the live particle count
 * reaches the budget, further emission is silently suppressed for the frame.
 *
 * Particles are plain entities carrying Position, Velocity, Color, Size, and
 * Particle, so they ride the existing MovementSystem and RenderSystem — no new
 * movement or rendering code is required for colored-rectangle particles.
 *
 * The shape-sampling and interpolation helpers are pure free functions so they
 * can be unit-tested without running the game loop.
 *
 * Coordinate system: bottom-left origin, Y up. Downward gravity is negative dy.
 */

#ifndef PARTICLE_SYSTEM_HPP
#define PARTICLE_SYSTEM_HPP

#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include <random>

/**
 * Default global cap on the number of live particles. The ParticleSystem stops
 * emitting once this many Particle entities exist, preventing runaway emitters
 * from exhausting memory or tanking the frame rate.
 */
inline constexpr int DEFAULT_MAX_PARTICLES = 2000;

// ---------------------------------------------------------------------------
// Pure helpers — interpolation
// ---------------------------------------------------------------------------

/**
 * Linearly interpolate a Color per channel (including alpha) by t.
 * At t == 0 returns `start` exactly; at t == 1 returns `end` exactly.
 * t is used as-is (callers clamp to [0,1]).
 */
Color lerp_color(Color start, Color end, float t);

/**
 * Linearly interpolate a size by t.
 * At t == 0 returns `start` exactly; at t == 1 returns `end` exactly.
 */
float lerp_size(float start, float end, float t);

// ---------------------------------------------------------------------------
// Pure helpers — shape sampling
// ---------------------------------------------------------------------------

/**
 * Sample a spawn position for one particle from the emitter's shape, relative
 * to `origin` (the emitter's world position):
 *   Point  → origin exactly
 *   Line   → origin + u * (line_dx, line_dy), u in [0,1]
 *   Circle → a uniformly distributed point within `radius` of origin
 *   Cone   → origin exactly (the cone affects velocity direction, not position)
 */
Position sample_point(const ParticleEmitter& emitter, Position origin, std::mt19937& rng);

/**
 * Sample an initial velocity for one particle: a speed uniformly drawn from
 * [min_speed, max_speed] in a direction drawn from
 * [direction - cone_half_angle, direction + cone_half_angle] degrees.
 * For Point/Line/Circle shapes the cone parameters still govern direction
 * (use cone_half_angle = 180 for a full circle of directions).
 */
Velocity sample_velocity(const ParticleEmitter& emitter, std::mt19937& rng);

// ---------------------------------------------------------------------------
// ParticleSystem
// ---------------------------------------------------------------------------

class ParticleSystem {
public:
    explicit ParticleSystem(int max_particles = DEFAULT_MAX_PARTICLES, uint32_t seed = 0xC0FFEEu);

    /**
     * Advance the particle simulation by dt seconds: emit from active emitters,
     * age existing particles, interpolate color/size, apply gravity, and mark
     * expired particles for deferred destruction.
     *
     * `emit` = false ages and retires existing particles without spawning any new
     * ones. Callers that keep ticking the simulation in a phase where entity
     * lifetimes are *not* ticking need this: an emitter host that would normally
     * have expired stays alive and, left emitting, saturates the particle budget
     * for as long as that phase lasts.
     */
    void update(ComponentStorage& storage, EntityManager& entity_manager, float dt,
                bool emit = true);

    int max_particles() const { return max_particles_; }

private:
    int max_particles_;
    std::mt19937 rng_;  // Single seeded engine for all sampling
};

#endif // PARTICLE_SYSTEM_HPP
