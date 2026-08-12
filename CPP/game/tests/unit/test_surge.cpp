/**
 * test_surge.cpp — reactor surge events (engine suite, Lane X, D149).
 *
 * The scheduler is the risky half, and the risk is determinism: it must take the
 * same number of draws whether or not anything fires, or two builds of the same
 * seed diverge. That is what most of this file pins. The rest covers the caps and
 * the teardown — a coolant flood that outlives its arena is a permanent debuff.
 */
#include <catch2/catch_test_macros.hpp>

#include <string>

#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/destruction.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "game/force_field_system.hpp"
#include "game/player_components.hpp"
#include "game/surge_system.hpp"

namespace {

GameConfig config_with(const std::string& effect, float chance) {
    GameConfig cfg;
    cfg.arena.center_x = 480.0f; cfg.arena.center_y = 330.0f; cfg.arena.radius = 320.0f;
    ArenaDef a;
    a.name = "Test";
    if (!effect.empty()) {
        SurgeDef d;
        d.effect = effect;
        d.first_wave = 1;
        d.chance = chance;
        d.duration = 5.0f;
        d.telegraph = 0.0f;      // live immediately, so the tests are short
        d.radius = 200.0f;
        a.surges.push_back(d);
    }
    cfg.arenas.push_back(a);
    return cfg;
}

}  // namespace

TEST_CASE("effect names resolve to kinds, never to row indices", "[Game][surge]") {
    using E = SurgeSystem::Effect;
    CHECK(SurgeSystem::effect_for("slow_field") == E::SlowField);
    CHECK(SurgeSystem::effect_for("sweep_line") == E::SweepLine);
    CHECK(SurgeSystem::effect_for("eruption") == E::Eruption);
    CHECK(SurgeSystem::effect_for("gravity_storm") == E::GravityStorm);
    CHECK(SurgeSystem::effect_for("") == E::Unknown);
    CHECK(SurgeSystem::effect_for("meltdown") == E::Unknown);
}

TEST_CASE("the scheduler takes the same draws with and without a table",
          "[Game][surge][determinism]") {
    // THE R2 TEST. An arena with no surges must consume exactly as many values
    // from the scheduler's stream as one with a full table — otherwise authoring
    // surge content would silently re-roll every later surge in the run.
    EntityManager em1, em2;
    ComponentStorage cs1, cs2;
    Blackboard bb1, bb2;
    ForceFieldSystem f1, f2;
    f1.set_capacity(8); f2.set_capacity(8);

    GameConfig with = config_with("slow_field", 1.0f);   // always fires
    GameConfig without = config_with("", 0.0f);          // no table at all

    SurgeSystem a, b;
    a.set_config(&with);   a.set_seed(42u);
    b.set_config(&without); b.set_seed(42u);

    for (int wave = 1; wave <= 10; ++wave) {
        a.update(cs1, em1, bb1, f1, 0, wave, /*wave_changed=*/true, 1.0f / 60.0f);
        b.update(cs2, em2, bb2, f2, 0, wave, /*wave_changed=*/true, 1.0f / 60.0f);
    }
    CHECK(a.draws_taken() == b.draws_taken());
    CHECK(a.draws_taken() == 30);            // three per wave tick, ten waves
    CHECK(a.live_count() > 0);               // ...and one of them actually fired
    CHECK(b.live_count() == 0);
}

TEST_CASE("the scheduler only rolls on a wave change", "[Game][surge]") {
    EntityManager em; ComponentStorage cs; Blackboard bb; ForceFieldSystem f;
    f.set_capacity(8);
    GameConfig cfg = config_with("slow_field", 0.0f);   // never fires
    SurgeSystem s;
    s.set_config(&cfg);
    s.set_seed(7u);

    for (int i = 0; i < 100; ++i)
        s.update(cs, em, bb, f, 0, 3, /*wave_changed=*/false, 1.0f / 60.0f);
    CHECK(s.draws_taken() == 0);

    s.update(cs, em, bb, f, 0, 4, true, 1.0f / 60.0f);
    CHECK(s.draws_taken() == 3);
}

TEST_CASE("a wave window gates which rows are eligible", "[Game][surge]") {
    EntityManager em; ComponentStorage cs; Blackboard bb; ForceFieldSystem f;
    f.set_capacity(8);
    GameConfig cfg = config_with("slow_field", 1.0f);
    cfg.arenas[0].surges[0].first_wave = 5;
    cfg.arenas[0].surges[0].last_wave = 8;
    SurgeSystem s;
    s.set_config(&cfg);
    s.set_seed(7u);

    s.update(cs, em, bb, f, 0, 4, true, 1.0f / 60.0f);
    CHECK(s.live_count() == 0);              // before the window
    s.update(cs, em, bb, f, 0, 6, true, 1.0f / 60.0f);
    CHECK(s.live_count() == 1);              // inside it
    s.update(cs, em, bb, f, 0, 9, true, 1.0f / 60.0f);
    CHECK(s.live_count() == 1);              // past it: no SECOND event
}

TEST_CASE("live events are capped", "[Game][surge]") {
    EntityManager em; ComponentStorage cs; Blackboard bb; ForceFieldSystem f;
    f.set_capacity(8);
    GameConfig cfg = config_with("slow_field", 1.0f);
    cfg.arenas[0].surges[0].duration = 1000.0f;    // never retires during the test
    SurgeSystem s;
    s.set_config(&cfg);
    s.set_seed(7u);

    for (int wave = 1; wave <= 20; ++wave)
        s.update(cs, em, bb, f, 0, wave, true, 1.0f / 60.0f);
    CHECK(s.live_count() == SurgeSystem::MAX_LIVE);
}

TEST_CASE("a coolant flood drags what stands in it and nothing else",
          "[Game][surge]") {
    EntityManager em; ComponentStorage cs; Blackboard bb; ForceFieldSystem f;
    f.set_capacity(8);
    GameConfig cfg = config_with("slow_field", 1.0f);
    SurgeSystem s;
    s.set_config(&cfg);
    s.set_seed(3u);
    s.update(cs, em, bb, f, 0, 1, true, 1.0f / 60.0f);
    REQUIRE(s.live_count() == 1);
    const SurgeSystem::Live ev = s.live()[0];

    Entity inside = em.create_entity();
    cs.add_component<Position>(inside, Position{ev.x, ev.y});
    cs.add_component<Velocity>(inside, Velocity{100.0f, 0.0f});
    cs.add_component<PlayerTag>(inside, PlayerTag{});

    Entity outside = em.create_entity();
    cs.add_component<Position>(outside, Position{ev.x + ev.radius * 4.0f, ev.y});
    cs.add_component<Velocity>(outside, Velocity{100.0f, 0.0f});
    cs.add_component<PlayerTag>(outside, PlayerTag{});

    s.update(cs, em, bb, f, 0, 1, false, 1.0f / 60.0f);
    CHECK(cs.get_component<Velocity>(inside)->get().dx < 100.0f);
    CHECK(cs.get_component<Velocity>(outside)->get().dx == 100.0f);
}

TEST_CASE("a gravity storm registers a force source instead of owning physics",
          "[Game][surge]") {
    EntityManager em; ComponentStorage cs; Blackboard bb; ForceFieldSystem f;
    f.set_capacity(8);
    GameConfig cfg = config_with("gravity_storm", 1.0f);
    SurgeSystem s;
    s.set_config(&cfg);
    s.set_seed(11u);
    // Mirroring the real frame order: the storm re-registers a ONE-FRAME source
    // every tick and `forces` consumes it in the same frame, so the source count
    // stays at one however long the storm runs — nothing accumulates and nothing
    // has to be unregistered when it ends.
    for (int i = 0; i < 5; ++i) {
        s.update(cs, em, bb, f, 0, 1, i == 0, 1.0f / 60.0f);
        CHECK(f.live_sources() == 1);
        f.update(cs, 1.0f / 60.0f);
        CHECK(f.live_sources() == 0);
    }
}

TEST_CASE("events retire, and clear takes their carriers with them",
          "[Game][surge]") {
    EntityManager em; ComponentStorage cs; Blackboard bb; ForceFieldSystem f;
    f.set_capacity(8);
    GameConfig cfg = config_with("eruption", 1.0f);
    cfg.arenas[0].surges[0].duration = 0.5f;
    SurgeSystem s;
    s.set_config(&cfg);
    s.set_seed(5u);

    s.update(cs, em, bb, f, 0, 1, true, 1.0f / 60.0f);
    REQUIRE(s.live_count() == 1);
    REQUIRE(s.live()[0].has_carrier);
    const Entity carrier = s.live()[0].carrier;
    REQUIRE(cs.has_component<ContactDamage>(carrier));

    for (int i = 0; i < 60; ++i) s.update(cs, em, bb, f, 0, 1, false, 1.0f / 60.0f);
    CHECK(s.live_count() == 0);
    destroy_marked_entities(em, cs);
    CHECK_FALSE(cs.has_component<ContactDamage>(carrier));   // no orphan hazard

    // clear() is the arena-shift path: a flood must not outlive its arena.
    s.update(cs, em, bb, f, 0, 2, true, 1.0f / 60.0f);
    REQUIRE(s.live_count() == 1);
    const Entity second = s.live()[0].carrier;
    s.clear(cs, em);
    CHECK(s.live_count() == 0);
    destroy_marked_entities(em, cs);
    CHECK_FALSE(cs.has_component<ContactDamage>(second));
}

TEST_CASE("a telegraphed event does not bite until it goes live",
          "[Game][surge]") {
    EntityManager em; ComponentStorage cs; Blackboard bb; ForceFieldSystem f;
    f.set_capacity(8);
    GameConfig cfg = config_with("eruption", 1.0f);
    cfg.arenas[0].surges[0].telegraph = 1.0f;
    SurgeSystem s;
    s.set_config(&cfg);
    s.set_seed(5u);
    s.update(cs, em, bb, f, 0, 1, true, 1.0f / 60.0f);
    REQUIRE(s.live_count() == 1);
    const Entity carrier = s.live()[0].carrier;

    // Visible from frame one (the warning IS the point), but harmless.
    CHECK(cs.get_component<ContactDamage>(carrier)->get().amount == 0.0f);
    for (int i = 0; i < 61; ++i) s.update(cs, em, bb, f, 0, 1, false, 1.0f / 60.0f);
    CHECK(cs.get_component<ContactDamage>(carrier)->get().amount > 0.0f);
}
