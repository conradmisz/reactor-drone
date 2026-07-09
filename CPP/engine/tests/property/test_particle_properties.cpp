/**
 * Property-based tests for the particle system (Option-020)
 *
 * Universal invariants:
 *   - Emission rate produces the expected average particle count over N frames
 *     (within tolerance, absorbing fractional-accumulator rounding)
 *   - Particles expire exactly when age >= lifetime
 *   - Color/size interpolation stays within [start, end]
 *   - The global live-particle budget is never exceeded across many emit cycles
 *
 * Requirements tested: 6.3, 9.3, 9.4, 9.5
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>
#include <algorithm>
#include <cmath>
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/destruction.hpp"
#include "engine/ecs/systems/particle_system.hpp"

// Configurable test iteration counts (per .kiro/steering/property-test-bounds.md)
constexpr int NUM_OUTER_TESTS = 10;
constexpr int NUM_INNER_TESTS = 5;

// Property 1: Emission rate yields the expected average count over N frames.
TEST_CASE("Emission rate produces expected particle count over N frames", "[particle][property]") {
    auto rate = GENERATE(take(NUM_OUTER_TESTS, random(10.0f, 120.0f)));
    auto frames = GENERATE(take(NUM_INNER_TESTS, random(30, 120)));

    EntityManager em;
    ComponentStorage storage;
    ParticleSystem system(100000);  // budget high enough not to clamp

    const float dt = 1.0f / 60.0f;

    Entity host = em.create_entity();
    ParticleEmitter emitter;
    emitter.active = true;
    emitter.emission_rate = rate;
    emitter.particle_lifetime = 1e9f;  // effectively immortal for this test
    storage.add_component(host, Position{0.0f, 0.0f});
    storage.add_component(host, emitter);

    for (int f = 0; f < frames; ++f) {
        system.update(storage, em, dt);
    }

    int count = static_cast<int>(storage.entities_with_component<Particle>().size());
    float expected = rate * static_cast<float>(frames) * dt;
    // Accumulator carry means the count is floor(expected); allow a small band
    // for floating-point accumulation error.
    CHECK(static_cast<float>(count) >= expected - 2.0f);
    CHECK(static_cast<float>(count) <= expected + 1.0f);
}

// Property 2: Particles expire exactly at age >= lifetime.
TEST_CASE("Particles expire exactly when age reaches lifetime", "[particle][property]") {
    auto lifetime = GENERATE(take(NUM_OUTER_TESTS, random(0.1f, 3.0f)));
    auto dt = GENERATE(take(NUM_INNER_TESTS, random(0.005f, 0.1f)));

    EntityManager em;
    ComponentStorage storage;
    ParticleSystem system;

    Entity p = em.create_entity();
    Particle particle;
    particle.age = 0.0f;
    particle.lifetime = lifetime;
    storage.add_component(p, particle);
    storage.add_component(p, Position{0.0f, 0.0f});
    storage.add_component(p, Velocity{0.0f, 0.0f});
    storage.add_component(p, Color{255, 255, 255, 255});
    storage.add_component(p, Size{4.0f, 4.0f});

    // Advance until expiry, asserting the invariant at every step.
    int guard = 0;
    while (storage.has_component<Particle>(p) &&
           !storage.has_component<DestroyRequest>(p) &&
           guard < 100000) {
        float age_before = storage.get_component<Particle>(p)->get().age;
        system.update(storage, em, dt);
        float age_after = storage.get_component<Particle>(p)->get().age;

        bool marked = storage.has_component<DestroyRequest>(p);
        if (marked) {
            CHECK(age_after >= lifetime);
        } else {
            CHECK(age_after < lifetime);
        }
        CHECK(age_after == Catch::Approx(age_before + dt));
        ++guard;
    }
    CHECK(storage.has_component<DestroyRequest>(p));
}

// Property 3: Interpolated color and size stay within [start, end].
TEST_CASE("Interpolated color and size stay within endpoints", "[particle][property]") {
    auto t = GENERATE(take(NUM_OUTER_TESTS, random(0.0f, 1.0f)));
    auto sr = GENERATE(take(NUM_INNER_TESTS, random(0, 255)));
    auto er = GENERATE(take(NUM_INNER_TESTS, random(0, 255)));

    Color start{static_cast<uint8_t>(sr), 10, 200, 255};
    Color end{static_cast<uint8_t>(er), 250, 0, 0};

    Color c = lerp_color(start, end, t);

    auto in_bounds = [](int v, int a, int b) {
        int lo = std::min(a, b);
        int hi = std::max(a, b);
        return v >= lo - 1 && v <= hi + 1;  // ±1 for rounding
    };
    CHECK(in_bounds(c.r, start.r, end.r));
    CHECK(in_bounds(c.g, start.g, end.g));
    CHECK(in_bounds(c.b, start.b, end.b));
    CHECK(in_bounds(c.a, start.a, end.a));

    float start_size = 5.0f;
    float end_size = 0.0f;
    float sz = lerp_size(start_size, end_size, t);
    CHECK(sz >= std::min(start_size, end_size) - 1e-4f);
    CHECK(sz <= std::max(start_size, end_size) + 1e-4f);
}

// Property 4: The global live-particle budget is never exceeded.
TEST_CASE("Live particle count never exceeds the budget", "[particle][property]") {
    auto budget = GENERATE(take(NUM_OUTER_TESTS, random(5, 200)));
    auto rate = GENERATE(take(NUM_INNER_TESTS, random(500.0f, 5000.0f)));

    EntityManager em;
    ComponentStorage storage;
    ParticleSystem system(budget);

    Entity host = em.create_entity();
    ParticleEmitter emitter;
    emitter.active = true;
    emitter.emission_rate = rate;       // far exceeds the budget per frame
    emitter.particle_lifetime = 1e9f;   // never expire
    storage.add_component(host, Position{0.0f, 0.0f});
    storage.add_component(host, emitter);

    const float dt = 1.0f / 60.0f;
    for (int f = 0; f < 50; ++f) {
        system.update(storage, em, dt);
        int live = static_cast<int>(storage.entities_with_component<Particle>().size());
        CHECK(live <= budget);
    }
}
