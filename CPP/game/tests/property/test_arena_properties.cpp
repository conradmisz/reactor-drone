/**
 * Property-based tests for the Class-110 "Reactor Drone" arena systems.
 *
 * Uses GENERATE with the mandatory iteration counts. Properties covered:
 *  - aim round-trip: a velocity built from aim_angle points at the target;
 *  - ring spawn: every spawned enemy sits on the arena's spawn ring;
 *  - loot drops: a kill always drops between min_drops and max_drops pickups,
 *    and the same seed always drops the same ones (R2).
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <algorithm>
#include <cmath>
#include <tuple>

#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"

#include "game/aim_math.hpp"
#include "game/player_components.hpp"
#include "game/enemy_components.hpp"
#include "game/arena_config.hpp"
#include "game/enemy_death_system.hpp"
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

TEST_CASE("Property: a kill drops min..max pickups, and the same seed drops the same ones",
          "[Game][arena][property][economy]") {
    unsigned int seed = GENERATE(take(NUM_OUTER_TESTS, random(1u, 100000u)));
    int max_drops = GENERATE(take(NUM_INNER_TESTS, random(1, 5)));

    EconomyConfig ec;
    ec.min_drops = 1;
    ec.max_drops = max_drops;
    ec.key_drop_chance = 0.5f;   // exercise the key branch often

    // Run the same kill twice from the same seed; the drops must match exactly.
    auto run = [&]() {
        EntityManager em; ComponentStorage storage; Blackboard bb;
        Entity e = em.create_entity();
        storage.add_component<EnemyTag>(e, EnemyTag{});
        storage.add_component<Health>(e, Health{0.0f, 20.0f});
        storage.add_component<Position>(e, Position{100.0f, 100.0f});
        storage.add_component<Size>(e, Size{64.0f, 64.0f});
        storage.add_component<ContactDamage>(e, ContactDamage{8.0f, 10, 3});

        EnemyDeathSystem sys;
        sys.set_economy(ec, seed);
        sys.update(storage, em, bb);

        std::vector<std::tuple<int, int, float, float>> drops;
        for (Entity p : storage.entities_with_component<Pickup>()) {
            const Pickup& pk = storage.get_component<Pickup>(p)->get();
            const Position& po = storage.get_component<Position>(p)->get();
            drops.emplace_back(pk.kind, pk.value, po.x, po.y);
        }
        std::sort(drops.begin(), drops.end());
        return drops;
    };

    auto a = run();
    auto b = run();
    CHECK(a == b);                                  // R2: seeded drops are reproducible

    int currency = 0, keys = 0;
    for (const auto& [kind, value, x, y] : a) {
        (void)x; (void)y;
        if (kind == static_cast<int>(PickupKind::Key)) keys += 1;
        // D232 item 2: kills may also roll a health orb — not currency.
        else if (kind == static_cast<int>(PickupKind::Health)) { CHECK(value == 25); }
        // the type's currency (3, from ContactDamage above) plus the flat
        // CREDIT_BASE_BONUS of 2
        else { currency += 1; CHECK(value == 3 + 2); }
    }
    CHECK(currency >= ec.min_drops);
    CHECK(currency <= ec.max_drops);
    CHECK(keys <= 1);
}
