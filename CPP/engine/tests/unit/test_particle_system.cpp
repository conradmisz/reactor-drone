/**
 * Unit tests for the particle system (Option-020)
 *
 * Covers:
 *   - Particle / ParticleEmitter add/get/remove on ComponentStorage
 *   - destroy_marked_entities() removes both component types
 *   - Interpolation boundary correctness (lerp_color / lerp_size at t=0, t=1)
 *   - Shape sampling: point exactness, line/circle within bounds, cone
 *     direction within the half-angle
 *   - ParticleSystem emission, aging, and expiry basics
 *
 * Requirements tested: 2.1, 2.2, 2.3, 2.4, 4.1-4.6, 5.1-5.5, 6.1, 9.1, 9.2
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/destruction.hpp"
#include "engine/ecs/systems/particle_system.hpp"

// ---------------------------------------------------------------------------
// Storage wiring
// ---------------------------------------------------------------------------

TEST_CASE("Particle component add/get/remove on ComponentStorage", "[particle][storage]") {
    EntityManager em;
    ComponentStorage storage;
    Entity e = em.create_entity();

    Particle p;
    p.age = 0.25f;
    p.lifetime = 2.0f;
    p.start_size = 6.0f;
    storage.add_component(e, p);

    REQUIRE(storage.has_component<Particle>(e));
    auto got = storage.get_component<Particle>(e);
    REQUIRE(got.has_value());
    CHECK(got->get().age == Catch::Approx(0.25f));
    CHECK(got->get().lifetime == Catch::Approx(2.0f));
    CHECK(got->get().start_size == Catch::Approx(6.0f));

    storage.remove_component<Particle>(e);
    CHECK_FALSE(storage.has_component<Particle>(e));
}

TEST_CASE("ParticleEmitter component add/get/remove on ComponentStorage", "[particle][storage]") {
    EntityManager em;
    ComponentStorage storage;
    Entity e = em.create_entity();

    ParticleEmitter emitter;
    emitter.shape = EmitterShape::Circle;
    emitter.emission_rate = 50.0f;
    emitter.radius = 12.0f;
    storage.add_component(e, emitter);

    REQUIRE(storage.has_component<ParticleEmitter>(e));
    auto got = storage.get_component<ParticleEmitter>(e);
    REQUIRE(got.has_value());
    CHECK(got->get().shape == EmitterShape::Circle);
    CHECK(got->get().emission_rate == Catch::Approx(50.0f));
    CHECK(got->get().radius == Catch::Approx(12.0f));

    storage.remove_component<ParticleEmitter>(e);
    CHECK_FALSE(storage.has_component<ParticleEmitter>(e));
}

TEST_CASE("destroy_marked_entities removes Particle and ParticleEmitter", "[particle][destruction]") {
    EntityManager em;
    ComponentStorage storage;

    Entity particle = em.create_entity();
    storage.add_component(particle, Particle{});
    storage.add_component(particle, DestroyRequest{});

    Entity host = em.create_entity();
    storage.add_component(host, ParticleEmitter{});
    storage.add_component(host, DestroyRequest{});

    destroy_marked_entities(em, storage);

    CHECK_FALSE(storage.has_component<Particle>(particle));
    CHECK_FALSE(storage.has_component<ParticleEmitter>(host));
    CHECK_FALSE(em.is_alive(particle));
    CHECK_FALSE(em.is_alive(host));
}

// ---------------------------------------------------------------------------
// Interpolation boundary correctness
// ---------------------------------------------------------------------------

TEST_CASE("lerp_color returns endpoints exactly at t=0 and t=1", "[particle][interp]") {
    Color start{255, 200, 0, 255};
    Color end{120, 40, 0, 0};

    Color at_start = lerp_color(start, end, 0.0f);
    CHECK(at_start.r == start.r);
    CHECK(at_start.g == start.g);
    CHECK(at_start.b == start.b);
    CHECK(at_start.a == start.a);

    Color at_end = lerp_color(start, end, 1.0f);
    CHECK(at_end.r == end.r);
    CHECK(at_end.g == end.g);
    CHECK(at_end.b == end.b);
    CHECK(at_end.a == end.a);
}

TEST_CASE("lerp_color midpoint stays within channel bounds", "[particle][interp]") {
    Color start{0, 0, 0, 255};
    Color end{255, 100, 50, 0};
    Color mid = lerp_color(start, end, 0.5f);
    CHECK(mid.r == 128);   // round(127.5)
    CHECK(mid.g == 50);
    CHECK(mid.b == 25);
    CHECK(mid.a == 128);   // round(127.5)
}

TEST_CASE("lerp_size returns endpoints exactly at t=0 and t=1", "[particle][interp]") {
    CHECK(lerp_size(4.0f, 0.0f, 0.0f) == Catch::Approx(4.0f));
    CHECK(lerp_size(4.0f, 0.0f, 1.0f) == Catch::Approx(0.0f));
    CHECK(lerp_size(2.0f, 8.0f, 0.5f) == Catch::Approx(5.0f));
}

// ---------------------------------------------------------------------------
// Shape sampling
// ---------------------------------------------------------------------------

TEST_CASE("Point shape samples the emitter origin exactly", "[particle][shape]") {
    std::mt19937 rng(123);
    ParticleEmitter emitter;
    emitter.shape = EmitterShape::Point;
    Position origin{10.0f, -5.0f};

    for (int i = 0; i < 50; ++i) {
        Position p = sample_point(emitter, origin, rng);
        CHECK(p.x == Catch::Approx(origin.x));
        CHECK(p.y == Catch::Approx(origin.y));
    }
}

TEST_CASE("Line shape samples within the configured segment", "[particle][shape]") {
    std::mt19937 rng(7);
    ParticleEmitter emitter;
    emitter.shape = EmitterShape::Line;
    emitter.line_dx = 20.0f;
    emitter.line_dy = 0.0f;
    Position origin{0.0f, 3.0f};

    for (int i = 0; i < 200; ++i) {
        Position p = sample_point(emitter, origin, rng);
        CHECK(p.x >= Catch::Approx(0.0f).margin(1e-4));
        CHECK(p.x <= Catch::Approx(20.0f).margin(1e-4));
        CHECK(p.y == Catch::Approx(3.0f));  // no dy component
    }
}

TEST_CASE("Circle shape samples within the configured radius", "[particle][shape]") {
    std::mt19937 rng(99);
    ParticleEmitter emitter;
    emitter.shape = EmitterShape::Circle;
    emitter.radius = 15.0f;
    Position origin{100.0f, 100.0f};

    for (int i = 0; i < 500; ++i) {
        Position p = sample_point(emitter, origin, rng);
        float dx = p.x - origin.x;
        float dy = p.y - origin.y;
        float dist = std::sqrt(dx * dx + dy * dy);
        CHECK(dist <= Catch::Approx(15.0f).margin(1e-3));
    }
}

TEST_CASE("Cone shape samples velocity direction within the half-angle", "[particle][shape]") {
    std::mt19937 rng(2024);
    ParticleEmitter emitter;
    emitter.shape = EmitterShape::Cone;
    emitter.min_speed = 30.0f;
    emitter.max_speed = 60.0f;
    emitter.direction = 90.0f;        // straight up
    emitter.cone_half_angle = 20.0f;  // degrees

    const float center = 90.0f * static_cast<float>(M_PI) / 180.0f;
    const float half = 20.0f * static_cast<float>(M_PI) / 180.0f;

    for (int i = 0; i < 500; ++i) {
        Velocity v = sample_velocity(emitter, rng);
        float speed = std::sqrt(v.dx * v.dx + v.dy * v.dy);
        CHECK(speed >= Catch::Approx(30.0f).margin(1e-3));
        CHECK(speed <= Catch::Approx(60.0f).margin(1e-3));

        float angle = std::atan2(v.dy, v.dx);
        float delta = angle - center;
        CHECK(std::abs(delta) <= Catch::Approx(half).margin(1e-4));
    }
}

// ---------------------------------------------------------------------------
// ParticleSystem behavior
// ---------------------------------------------------------------------------

TEST_CASE("ParticleSystem spawns particles from an active emitter", "[particle][system]") {
    EntityManager em;
    ComponentStorage storage;
    ParticleSystem system;

    Entity host = em.create_entity();
    ParticleEmitter emitter;
    emitter.active = true;
    emitter.emission_rate = 60.0f;
    emitter.particle_lifetime = 10.0f;  // long-lived so none expire during the test
    storage.add_component(host, Position{0.0f, 0.0f});
    storage.add_component(host, emitter);

    system.update(storage, em, 1.0f);  // 60/s * 1s = 60 particles

    auto particles = storage.entities_with_component<Particle>();
    CHECK(particles.size() == 60);

    // Each particle carries the full render set.
    for (Entity p : particles) {
        CHECK(storage.has_component<Position>(p));
        CHECK(storage.has_component<Velocity>(p));
        CHECK(storage.has_component<Color>(p));
        CHECK(storage.has_component<Size>(p));
    }
}

TEST_CASE("ParticleSystem does not spawn from an inactive emitter", "[particle][system]") {
    EntityManager em;
    ComponentStorage storage;
    ParticleSystem system;

    Entity host = em.create_entity();
    ParticleEmitter emitter;
    emitter.active = false;
    emitter.emission_rate = 60.0f;
    storage.add_component(host, Position{0.0f, 0.0f});
    storage.add_component(host, emitter);

    system.update(storage, em, 1.0f);
    CHECK(storage.entities_with_component<Particle>().empty());
}

TEST_CASE("ParticleSystem marks expired particles with DestroyRequest", "[particle][system]") {
    EntityManager em;
    ComponentStorage storage;
    ParticleSystem system;

    Entity p = em.create_entity();
    Particle particle;
    particle.age = 0.0f;
    particle.lifetime = 0.5f;
    storage.add_component(p, particle);
    storage.add_component(p, Position{0.0f, 0.0f});
    storage.add_component(p, Velocity{0.0f, 0.0f});
    storage.add_component(p, Color{255, 255, 255, 255});
    storage.add_component(p, Size{4.0f, 4.0f});

    // Not yet expired
    system.update(storage, em, 0.25f);
    CHECK_FALSE(storage.has_component<DestroyRequest>(p));

    // Crosses lifetime → marked for destruction
    system.update(storage, em, 0.30f);
    CHECK(storage.has_component<DestroyRequest>(p));
}

TEST_CASE("ParticleSystem applies gravity to particle velocity", "[particle][system]") {
    EntityManager em;
    ComponentStorage storage;
    ParticleSystem system;

    Entity p = em.create_entity();
    Particle particle;
    particle.lifetime = 10.0f;
    particle.gravity_y = -100.0f;  // downward in bottom-left coordinates
    storage.add_component(p, particle);
    storage.add_component(p, Position{0.0f, 0.0f});
    storage.add_component(p, Velocity{0.0f, 0.0f});
    storage.add_component(p, Color{255, 255, 255, 255});
    storage.add_component(p, Size{4.0f, 4.0f});

    system.update(storage, em, 0.1f);
    auto v = storage.get_component<Velocity>(p);
    REQUIRE(v.has_value());
    CHECK(v->get().dy == Catch::Approx(-10.0f));  // -100 * 0.1
}
