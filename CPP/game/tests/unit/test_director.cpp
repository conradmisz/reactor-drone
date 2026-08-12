/**
 * test_director.cpp — the Adaptive Director (engine suite, Lane Q, D142).
 *
 * The director is invisible by design, so the tests pin the promises rather than
 * a curve: disabled is exactly 1.0, the multiplier never leaves its authored
 * bounds, a hurt player gets MORE spacing than a cruising one, and the stress
 * scalar it publishes is bounded — every consumer downstream assumes [0,1].
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "game/director_system.hpp"
#include "game/wave_spawner_system.hpp"

using Catch::Matchers::WithinAbs;

namespace {

constexpr float DT = 1.0f / 60.0f;

DirectorConfig live_config() {
    DirectorConfig c;
    c.enabled = true;
    return c;
}

Entity make_player(EntityManager& em, ComponentStorage& cs, float hp) {
    Entity p = em.create_entity();
    cs.add_component<PlayerTag>(p, PlayerTag{});
    cs.add_component<Health>(p, Health{hp, 100.0f});
    return p;
}

float run(ComponentStorage& cs, Blackboard& bb, const DirectorConfig& cfg,
          DirectorState& st, int frames) {
    float m = 1.0f;
    for (int i = 0; i < frames; ++i)
        m = tick_director(cs, bb, cfg, st, DT, /*active=*/true);
    return m;
}

}  // namespace

TEST_CASE("a disabled director multiplies by exactly 1.0", "[Game][director]") {
    EntityManager em; ComponentStorage cs; Blackboard bb;
    make_player(em, cs, 5.0f);            // nearly dead: maximum stress if live
    DirectorConfig cfg;                   // enabled defaults false
    DirectorState st;
    for (int i = 0; i < 60; ++i)
        REQUIRE(tick_director(cs, bb, cfg, st, DT, true) == 1.0f);
    CHECK(bb.get<float>("director.stress") == 0.0f);
}

TEST_CASE("an untouched player gets pressure EARLY", "[Game][director]") {
    EntityManager em; ComponentStorage cs; Blackboard bb;
    make_player(em, cs, 100.0f);          // full hull, no damage, no kills
    DirectorConfig cfg = live_config();
    DirectorState st;
    const float m = run(cs, bb, cfg, st, 120);
    CHECK(m < 1.0f);
    CHECK_THAT(m, WithinAbs(cfg.min_mult, 1e-4f));
    CHECK(bb.get<float>("director.stress") == 0.0f);
}

TEST_CASE("a scramble buys the player breathing room", "[Game][director]") {
    EntityManager em; ComponentStorage cs; Blackboard bb;
    Entity p = make_player(em, cs, 100.0f);
    DirectorConfig cfg = live_config();
    DirectorState st;

    const float cruising = run(cs, bb, cfg, st, 60);

    // Take a beating: hull down to a quarter over a second of frames.
    auto& hp = cs.get_component<Health>(p)->get();
    for (int i = 0; i < 60; ++i) {
        hp.current = std::max(25.0f, hp.current - 1.25f);
        tick_director(cs, bb, cfg, st, DT, true);
    }
    const float hurt = tick_director(cs, bb, cfg, st, DT, true);

    CHECK(hurt > cruising);
    CHECK(bb.get<float>("director.stress") > 0.0f);
}

TEST_CASE("the multiplier never leaves the authored bounds", "[Game][director]") {
    EntityManager em; ComponentStorage cs; Blackboard bb;
    Entity p = make_player(em, cs, 100.0f);
    DirectorConfig cfg = live_config();
    DirectorState st;
    auto& hp = cs.get_component<Health>(p)->get();

    int kills = 0;
    for (int i = 0; i < 600; ++i) {
        // Alternate savage damage and kill streaks, i.e. both extremes.
        hp.current = (i % 40 < 20) ? std::max(1.0f, hp.current - 5.0f) : 100.0f;
        if (i % 3 == 0) bb.set<int>("sim.kills", ++kills);
        const float m = tick_director(cs, bb, cfg, st, DT, true);
        REQUIRE(m >= cfg.min_mult - 1e-4f);
        REQUIRE(m <= cfg.max_mult + 1e-4f);
        const float s = bb.get<float>("director.stress");
        REQUIRE(s >= 0.0f);
        REQUIRE(s <= 1.0f);
    }
}

TEST_CASE("kills relieve stress a hurt player has built up", "[Game][director]") {
    EntityManager em; ComponentStorage cs; Blackboard bb;
    Entity p = make_player(em, cs, 100.0f);
    DirectorConfig cfg = live_config();
    cfg.hull_weight = 0.0f;    // isolate the damage/kill terms from standing hull
    DirectorState st;
    auto& hp = cs.get_component<Health>(p)->get();

    for (int i = 0; i < 60; ++i) {
        hp.current = std::max(40.0f, hp.current - 1.0f);
        tick_director(cs, bb, cfg, st, DT, true);
    }
    const float after_damage = bb.get<float>("director.stress");
    REQUIRE(after_damage > 0.0f);

    int kills = 0;
    for (int i = 0; i < 60; ++i) {
        bb.set<int>("sim.kills", ++kills);   // one kill per frame: a rout
        tick_director(cs, bb, cfg, st, DT, true);
    }
    CHECK(bb.get<float>("director.stress") < after_damage);
}

TEST_CASE("a frozen phase holds stress instead of decaying it", "[Game][director]") {
    EntityManager em; ComponentStorage cs; Blackboard bb;
    Entity p = make_player(em, cs, 100.0f);
    DirectorConfig cfg = live_config();
    DirectorState st;
    auto& hp = cs.get_component<Health>(p)->get();
    for (int i = 0; i < 60; ++i) {
        hp.current = std::max(30.0f, hp.current - 1.2f);
        tick_director(cs, bb, cfg, st, DT, true);
    }
    const float entering_shop = tick_director(cs, bb, cfg, st, DT, true);

    // 10 seconds of shopping must not reset the pacing the fight earned.
    for (int i = 0; i < 600; ++i)
        CHECK(tick_director(cs, bb, cfg, st, DT, /*active=*/false) == entering_shop);
}

TEST_CASE("healing is not negative damage", "[Game][director]") {
    EntityManager em; ComponentStorage cs; Blackboard bb;
    Entity p = make_player(em, cs, 30.0f);
    DirectorConfig cfg = live_config();
    DirectorState st;
    auto& hp = cs.get_component<Health>(p)->get();
    run(cs, bb, cfg, st, 30);
    hp.current = 100.0f;                    // a big heal in one frame
    for (int i = 0; i < 30; ++i) {
        const float m = tick_director(cs, bb, cfg, st, DT, true);
        REQUIRE(m >= cfg.min_mult - 1e-4f);
        REQUIRE(bb.get<float>("director.stress") >= 0.0f);
    }
}

TEST_CASE("the spawner's spacing multiplier scales spacing and nothing else",
          "[Game][director][spawner]") {
    WaveSpawnerSystem s;
    CHECK(s.spacing_mult() == 1.0f);        // the authored table, exactly
    s.set_spacing_mult(1.3f);
    CHECK(s.spacing_mult() == 1.3f);
    // A zero or negative multiplier would stall or invert the wave; it is refused
    // at the setter rather than guarded at every use.
    s.set_spacing_mult(0.0f);
    CHECK(s.spacing_mult() == 1.0f);
    s.set_spacing_mult(-2.0f);
    CHECK(s.spacing_mult() == 1.0f);
}
