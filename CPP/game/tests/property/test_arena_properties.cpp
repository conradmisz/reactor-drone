/**
 * Property-based tests for the Class-110 "Reactor Drone" arena systems.
 *
 * Uses GENERATE with the mandatory iteration counts. Properties covered:
 *  - aim round-trip: a velocity built from aim_angle points at the target;
 *  - ring spawn: every spawned enemy sits on the arena's spawn ring;
 *  - XP monotonicity: level never decreases and xp stays non-negative;
 *  - upgrade picker: always returns an in-bounds pool index.
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>

#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"

#include "game/aim_math.hpp"
#include "game/player_components.hpp"
#include "game/enemy_components.hpp"
#include "game/arena_config.hpp"
#include "game/upgrade_system.hpp"
#include "game/experience_system.hpp"
#include "game/wave_spawner_system.hpp"

using Catch::Matchers::WithinAbs;

constexpr int NUM_OUTER_TESTS = 10;
constexpr int NUM_INNER_TESTS = 5;

TEST_CASE("Property: a velocity from aim_angle points at the target", "[Game][arena][property]") {
    float tx = GENERATE(take(NUM_OUTER_TESTS, random(-500.0f, 500.0f)));
    float ty = GENERATE(take(NUM_INNER_TESTS, random(-500.0f, 500.0f)));
    if (std::fabs(tx) < 1.0f && std::fabs(ty) < 1.0f) return;  // skip near-origin

    float angle = aim_math::aim_angle(0.0f, 0.0f, tx, ty);
    Velocity v = aim_math::velocity_from_angle(angle, 400.0f);

    float len_t = std::sqrt(tx * tx + ty * ty);
    // Cross product ~0 → colinear; dot > 0 → same direction (not opposite).
    float cross = v.dx * ty - v.dy * tx;
    float dot = v.dx * tx + v.dy * ty;
    CHECK_THAT(cross / len_t, WithinAbs(0.0f, 1e-2));
    CHECK(dot > 0.0f);
    CHECK_THAT(std::sqrt(v.dx * v.dx + v.dy * v.dy), WithinAbs(400.0f, 1e-2));
}

TEST_CASE("Property: spawned enemies land on the arena spawn ring", "[Game][arena][property]") {
    float radius = GENERATE(take(NUM_OUTER_TESTS, random(120.0f, 400.0f)));
    unsigned int seed = GENERATE(take(NUM_INNER_TESTS, random(1u, 100000u)));

    GameConfig cfg;
    cfg.seed = seed;
    cfg.arena.center_x = 500.0f;
    cfg.arena.center_y = 350.0f;
    cfg.arena.spawn_radius = radius;
    // Upgrade Phase 2: the ring is centred on the player (here: none, so the
    // arena centre) and clamped inside arena.radius — keep the play circle
    // wider than the spawn ring so nothing is clamped.
    cfg.arena.radius = radius + 100.0f;
    cfg.enemy_types.push_back(EnemyType{});           // one default type
    WaveDef w; w.count = 3; w.delay = 0.0f; w.spawn_interval = 0.0f;
    cfg.waves.push_back(w);

    EntityManager em; ComponentStorage storage; Blackboard bb;
    bb.set<double>("delta_time", 1.0);   // large dt so a spawn happens this tick

    WaveSpawnerSystem spawner;
    spawner.set_config(&cfg);
    spawner.update(bb, em, storage);

    auto enemies = storage.entities_with_component<EnemyTag>();
    REQUIRE(enemies.size() >= 1);
    for (Entity e : enemies) {
        auto pos = storage.get_component<Position>(e);
        auto sz = storage.get_component<Size>(e);
        REQUIRE(pos.has_value());
        REQUIRE(sz.has_value());
        float cx = pos->get().x + sz->get().width * 0.5f;
        float cy = pos->get().y + sz->get().height * 0.5f;
        float d = std::sqrt(std::pow(cx - cfg.arena.center_x, 2) +
                            std::pow(cy - cfg.arena.center_y, 2));
        CHECK_THAT(d, WithinAbs(radius, 0.5f));
    }
}

TEST_CASE("Property: XP grants keep level monotone and xp non-negative", "[Game][arena][property]") {
    unsigned int seed = GENERATE(take(NUM_OUTER_TESTS, random(1u, 100000u)));
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> xp_dist(0, 40);

    EntityManager em; ComponentStorage storage; Blackboard bb;
    Entity p = em.create_entity();
    storage.add_component<PlayerTag>(p, PlayerTag{});
    storage.add_component<Experience>(p, Experience{0.0f, 1, 5.0f, 1.5f});
    ExperienceSystem sys;

    int prev_level = 1;
    for (int i = 0; i < NUM_INNER_TESTS + 3; ++i) {
        bb.set("pending_xp", xp_dist(rng));
        sys.update(storage, bb);
        auto& exp = storage.get_component<Experience>(p)->get();
        CHECK(exp.level >= prev_level);   // never decreases
        CHECK(exp.xp >= 0.0f);            // never negative
        prev_level = exp.level;
    }
}

TEST_CASE("Property: upgrade picker always returns an in-bounds index", "[Game][arena][property]") {
    unsigned int seed = GENERATE(take(NUM_OUTER_TESTS, random(1u, 100000u)));
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> size_dist(1, 6);
    std::uniform_real_distribution<float> weight_dist(0.0f, 5.0f);
    std::uniform_real_distribution<float> roll_dist(0.0f, 1.0f);

    for (int i = 0; i < NUM_INNER_TESTS; ++i) {
        int n = size_dist(rng);
        std::vector<Upgrade> pool;
        for (int k = 0; k < n; ++k) pool.push_back(Upgrade{"s", 1.0f, weight_dist(rng), "L"});
        size_t idx = UpgradeSystem::pick_index(pool, roll_dist(rng));
        CHECK(idx < pool.size());
    }
}
