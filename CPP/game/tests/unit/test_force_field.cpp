/**
 * test_force_field.cpp — the force-field layer (engine suite, Lane T, D144).
 *
 * The layer is inert by shape rather than by a flag, so the first thing pinned is
 * that "no sources" really is "no work and no change". After that: direction,
 * falloff, the fixed capacity, lifetimes, and the per-frame delta clamp that keeps
 * a strong well from outrunning the obstacle push-out.
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "game/enemy_components.hpp"
#include "game/force_field_system.hpp"
#include "game/player_components.hpp"

using Catch::Matchers::WithinAbs;

namespace {

constexpr float DT = 1.0f / 60.0f;

Entity make_body(EntityManager& em, ComponentStorage& cs, float x, float y,
                 bool player) {
    Entity e = em.create_entity();
    cs.add_component<Position>(e, Position{x, y});
    cs.add_component<Size>(e, Size{40.0f, 40.0f});
    cs.add_component<Velocity>(e, Velocity{0.0f, 0.0f});
    if (player) cs.add_component<PlayerTag>(e, PlayerTag{});
    else        cs.add_component<EnemyTag>(e, EnemyTag{});
    return e;
}

ForceFieldSystem::Source well(float x, float y, float radius, float strength) {
    ForceFieldSystem::Source s;
    s.x = x; s.y = y; s.radius = radius; s.strength = strength; s.lifetime = 5.0f;
    return s;
}

}  // namespace

TEST_CASE("with no sources the pass changes nothing", "[Game][forces]") {
    EntityManager em; ComponentStorage cs;
    Entity e = make_body(em, cs, 100.0f, 100.0f, false);
    ForceFieldSystem f;
    f.set_capacity(32);
    for (int i = 0; i < 60; ++i) f.update(cs, DT);
    const Velocity& v = cs.get_component<Velocity>(e)->get();
    CHECK(v.dx == 0.0f);
    CHECK(v.dy == 0.0f);
    CHECK(f.live_sources() == 0);
}

TEST_CASE("a positive well pulls IN, a negative one pushes OUT", "[Game][forces]") {
    EntityManager em; ComponentStorage cs;
    Entity e = make_body(em, cs, 200.0f, 300.0f, false);   // centre (220,320)
    ForceFieldSystem f;
    f.set_capacity(8);

    f.add_source(well(400.0f, 320.0f, 400.0f, 300.0f));    // to the body's right
    f.update(cs, DT);
    CHECK(cs.get_component<Velocity>(e)->get().dx > 0.0f);  // pulled toward it

    cs.get_component<Velocity>(e)->get() = Velocity{0.0f, 0.0f};
    f.clear();
    f.add_source(well(400.0f, 320.0f, 400.0f, -300.0f));
    f.update(cs, DT);
    CHECK(cs.get_component<Velocity>(e)->get().dx < 0.0f);  // shoved away
}

TEST_CASE("a body outside the radius is untouched", "[Game][forces]") {
    EntityManager em; ComponentStorage cs;
    Entity e = make_body(em, cs, 0.0f, 0.0f, false);
    ForceFieldSystem f;
    f.set_capacity(8);
    f.add_source(well(1000.0f, 1000.0f, 100.0f, 900.0f));
    f.update(cs, DT);
    const Velocity& v = cs.get_component<Velocity>(e)->get();
    CHECK(v.dx == 0.0f);
    CHECK(v.dy == 0.0f);
}

TEST_CASE("the pull falls off linearly toward the rim", "[Game][forces]") {
    EntityManager em; ComponentStorage cs;
    Entity near = make_body(em, cs, 380.0f, 300.0f, false);   // centre (400,320)
    Entity far  = make_body(em, cs, 180.0f, 300.0f, false);   // centre (200,320)
    ForceFieldSystem f;
    f.set_capacity(8);
    f.add_source(well(500.0f, 320.0f, 400.0f, 300.0f));
    f.update(cs, DT);

    const float dn = std::fabs(cs.get_component<Velocity>(near)->get().dx);
    const float df = std::fabs(cs.get_component<Velocity>(far)->get().dx);
    CHECK(dn > df);
    CHECK(df > 0.0f);
}

TEST_CASE("a source can exclude the player or the enemies", "[Game][forces]") {
    EntityManager em; ComponentStorage cs;
    Entity enemy = make_body(em, cs, 300.0f, 300.0f, false);
    Entity player = make_body(em, cs, 340.0f, 300.0f, true);
    ForceFieldSystem f;
    f.set_capacity(8);

    ForceFieldSystem::Source s = well(400.0f, 320.0f, 400.0f, 400.0f);
    s.affect_player = false;
    f.add_source(s);
    f.update(cs, DT);
    CHECK(cs.get_component<Velocity>(enemy)->get().dx != 0.0f);
    CHECK(cs.get_component<Velocity>(player)->get().dx == 0.0f);
}

TEST_CASE("capacity is fixed and overflow is refused, not grown",
          "[Game][forces]") {
    ForceFieldSystem f;
    f.set_capacity(3);
    CHECK(f.add_source(well(0.0f, 0.0f, 50.0f, 10.0f)));
    CHECK(f.add_source(well(0.0f, 0.0f, 50.0f, 10.0f)));
    CHECK(f.add_source(well(0.0f, 0.0f, 50.0f, 10.0f)));
    CHECK_FALSE(f.add_source(well(0.0f, 0.0f, 50.0f, 10.0f)));
    CHECK(f.live_sources() == 3);
    CHECK(f.capacity() == 3);

    // A degenerate source is refused outright rather than living as a no-op.
    f.clear();
    ForceFieldSystem::Source bad = well(0.0f, 0.0f, 0.0f, 10.0f);
    CHECK_FALSE(f.add_source(bad));
    bad = well(0.0f, 0.0f, 50.0f, 10.0f);
    bad.lifetime = 0.0f;
    CHECK_FALSE(f.add_source(bad));
}

TEST_CASE("sources expire, and a one-frame impulse acts on its own frame",
          "[Game][forces]") {
    EntityManager em; ComponentStorage cs;
    Entity e = make_body(em, cs, 300.0f, 300.0f, false);
    ForceFieldSystem f;
    f.set_capacity(8);

    ForceFieldSystem::Source s = well(400.0f, 320.0f, 400.0f, 600.0f);
    s.lifetime = DT;                     // exactly one frame
    f.add_source(s);
    f.update(cs, DT);
    const float after = cs.get_component<Velocity>(e)->get().dx;
    CHECK(after > 0.0f);                 // it acted...
    CHECK(f.live_sources() == 0);        // ...and is gone

    f.update(cs, DT);
    CHECK(cs.get_component<Velocity>(e)->get().dx == after);   // no further pull
}

TEST_CASE("one frame's velocity delta is clamped", "[Game][forces]") {
    EntityManager em; ComponentStorage cs;
    Entity e = make_body(em, cs, 380.0f, 300.0f, false);
    ForceFieldSystem f;
    f.set_capacity(8);
    // An absurd well plus a long frame: without the clamp this would launch the
    // body far enough that the obstacle push-out could not resolve the overlap.
    f.add_source(well(400.0f, 320.0f, 400.0f, 1e6f));
    f.update(cs, 0.5f);
    const Velocity& v = cs.get_component<Velocity>(e)->get();
    CHECK(std::sqrt(v.dx * v.dx + v.dy * v.dy) <= 900.0f + 1e-2f);
}

TEST_CASE("clear drops every live field", "[Game][forces]") {
    ForceFieldSystem f;
    f.set_capacity(8);
    f.add_source(well(0.0f, 0.0f, 50.0f, 10.0f));
    f.add_source(well(0.0f, 0.0f, 50.0f, 10.0f));
    REQUIRE(f.live_sources() == 2);
    f.clear();
    CHECK(f.live_sources() == 0);
}

TEST_CASE("shrinking the capacity drops the overflow rather than corrupting it",
          "[Game][forces]") {
    ForceFieldSystem f;
    f.set_capacity(8);
    for (int i = 0; i < 8; ++i) f.add_source(well(0.0f, 0.0f, 50.0f, 10.0f));
    f.set_capacity(2);
    CHECK(f.live_sources() == 2);
    CHECK(f.capacity() == 2);
}
