/**
 * Unit tests for the Class-110 "Reactor Drone" arena systems: aim math, the
 * weighted upgrade picker, XP/leveling, ring spawning, collision-based projectile
 * hits, and player contact damage with i-frames.
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>

#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"

#include "game/aim_math.hpp"
#include "game/player_components.hpp"
#include "game/enemy_components.hpp"
#include "game/tower_components.hpp"
#include "game/arena_config.hpp"
#include "game/upgrade_system.hpp"
#include "game/experience_system.hpp"
#include "game/wave_spawner_system.hpp"
#include "game/projectile_hit_system.hpp"
#include "game/player_damage_system.hpp"

using Catch::Matchers::WithinAbs;
static constexpr float PI = 3.14159265358979323846f;

TEST_CASE("aim_angle points in the cardinal directions", "[Game][arena][aim]") {
    CHECK_THAT(aim_math::aim_angle(0, 0, 10, 0), WithinAbs(0.0f, 1e-5));
    CHECK_THAT(aim_math::aim_angle(0, 0, 0, 10), WithinAbs(PI / 2, 1e-5));
    CHECK_THAT(std::fabs(aim_math::aim_angle(0, 0, -10, 0)), WithinAbs(PI, 1e-5));
    CHECK_THAT(aim_math::aim_angle(0, 0, 0, -10), WithinAbs(-PI / 2, 1e-5));
    // Coincident points yield a defined 0 (no direction).
    CHECK(aim_math::aim_angle(5, 5, 5, 5) == 0.0f);
}

TEST_CASE("velocity_from_angle has the requested speed and direction", "[Game][arena][aim]") {
    Velocity v = aim_math::velocity_from_angle(PI / 2, 300.0f);
    CHECK_THAT(v.dx, WithinAbs(0.0f, 1e-3));
    CHECK_THAT(v.dy, WithinAbs(300.0f, 1e-3));
    float speed = std::sqrt(v.dx * v.dx + v.dy * v.dy);
    CHECK_THAT(speed, WithinAbs(300.0f, 1e-3));
}

TEST_CASE("UpgradeSystem::pick_index respects weights and bounds", "[Game][arena][upgrade]") {
    std::vector<Upgrade> pool = {
        {"a", 1.0f, 1.0f, "A"},
        {"b", 1.0f, 3.0f, "B"},  // 75% of the weight
    };
    CHECK(UpgradeSystem::pick_index(pool, 0.0f) == 0);   // first bucket
    CHECK(UpgradeSystem::pick_index(pool, 0.10f) == 0);  // still in A (0..0.25)
    CHECK(UpgradeSystem::pick_index(pool, 0.30f) == 1);  // into B
    CHECK(UpgradeSystem::pick_index(pool, 0.999f) == 1); // clamps inside last bucket

    CHECK(UpgradeSystem::pick_index({}, 0.5f) == 0);     // empty pool is safe
}

TEST_CASE("ExperienceSystem grants XP and levels up", "[Game][arena][xp]") {
    EntityManager em; ComponentStorage storage; Blackboard bb;
    Entity p = em.create_entity();
    storage.add_component<PlayerTag>(p, PlayerTag{});
    storage.add_component<Experience>(p, Experience{0.0f, 1, 5.0f, 2.0f});

    ExperienceSystem sys;
    bb.set("pending_xp", 4);
    sys.update(storage, bb);
    CHECK(storage.get_component<Experience>(p)->get().level == 1);      // 4 < 5, no level
    CHECK(bb.get_or<int>("pending_upgrades", 0) == 0);

    bb.set("pending_xp", 2);   // total 6 >= 5 → level 2, threshold → 10
    sys.update(storage, bb);
    auto& exp = storage.get_component<Experience>(p)->get();
    CHECK(exp.level == 2);
    CHECK(bb.get_or<int>("pending_upgrades", 0) == 1);
    CHECK_THAT(exp.xp, WithinAbs(1.0f, 1e-4));      // 6 - 5 carried over
    CHECK_THAT(exp.threshold, WithinAbs(10.0f, 1e-4));
    CHECK(bb.get_or<int>("pending_xp", -1) == 0);   // consumed
}

TEST_CASE("WaveSpawnerSystem total_waves honors victory_wave", "[Game][arena][waves]") {
    GameConfig cfg;
    cfg.waves.resize(6);
    WaveSpawnerSystem s;
    cfg.victory_wave = 0; s.set_config(&cfg); CHECK(s.total_waves() == 6);
    cfg.victory_wave = 3; s.set_config(&cfg); CHECK(s.total_waves() == 3);
    cfg.victory_wave = 9; s.set_config(&cfg); CHECK(s.total_waves() == 6); // clamped to available
}

TEST_CASE("ProjectileHitSystem damages the enemy it overlaps and expires", "[Game][arena][hit]") {
    EntityManager em; ComponentStorage storage;
    Entity enemy = em.create_entity();
    storage.add_component<EnemyTag>(enemy, EnemyTag{});
    storage.add_component<Health>(enemy, Health{30.0f, 30.0f});

    Entity proj = em.create_entity();
    storage.add_component<ProjectileTag>(proj, ProjectileTag{});
    storage.add_component<ProjectileData>(proj, ProjectileData{NO_TARGET, 500.0f, 20.0f});
    storage.add_component<CollidedWith>(proj, CollidedWith{{enemy}});

    ProjectileHitSystem sys;
    sys.update(em, storage);

    CHECK(storage.has_component<DestroyRequest>(proj));      // projectile consumed
    auto events = storage.entities_with_component<DamageEvent>();
    REQUIRE(events.size() == 1);
    CHECK(storage.get_component<DamageEvent>(events[0])->get().target_entity == enemy);
    CHECK_THAT(storage.get_component<DamageEvent>(events[0])->get().amount, WithinAbs(20.0f, 1e-4));
}

TEST_CASE("PlayerDamageSystem applies contact damage once per i-frame window",
          "[Game][arena][contact]") {
    EntityManager em; ComponentStorage storage; Blackboard bb;
    bb.set<double>("delta_time", 0.016);
    bb.set<float>("player.invuln_window", 0.8f);

    Entity enemy = em.create_entity();
    storage.add_component<EnemyTag>(enemy, EnemyTag{});
    storage.add_component<ContactDamage>(enemy, ContactDamage{12.0f, 10, 1});

    Entity player = em.create_entity();
    storage.add_component<PlayerTag>(player, PlayerTag{});
    storage.add_component<Health>(player, Health{100.0f, 100.0f});
    storage.add_component<CollidedWith>(player, CollidedWith{{enemy}});

    PlayerDamageSystem sys;
    sys.update(em, storage, bb);
    CHECK(storage.entities_with_component<DamageEvent>().size() == 1);
    CHECK(bb.get_or<float>("player.iframes", 0.0f) > 0.0f);

    // Immediately again while invulnerable → no new damage event.
    sys.update(em, storage, bb);
    CHECK(storage.entities_with_component<DamageEvent>().size() == 1);
}
