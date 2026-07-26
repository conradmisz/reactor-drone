/**
 * Unit tests for v2 Phase 6 multi-arena logic: arena-select-by-wave
 * (active_arena_index) and obstacle push-out (push_circle_out_of_aabb).
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>

#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"

#include "game/arena_config.hpp"
#include "game/obstacles.hpp"
#include "game/collision_layers.hpp"
#include "game/player_components.hpp"
#include "game/enemy_components.hpp"
#include "game/tower_components.hpp"
#include "game/projectile_hit_system.hpp"
#include "game/player_damage_system.hpp"

using Catch::Matchers::WithinAbs;

TEST_CASE("active_arena_index picks the last arena activated at or before the wave",
          "[Game][arena6][select]") {
    std::vector<ArenaDef> arenas;
    ArenaDef core;    core.name = "Core";    core.first_wave = 1; arenas.push_back(core);
    ArenaDef foundry; foundry.name = "Foundry"; foundry.first_wave = 3; arenas.push_back(foundry);
    ArenaDef biolab;  biolab.name = "Bio-lab"; biolab.first_wave = 5; arenas.push_back(biolab);

    CHECK(active_arena_index(arenas, 0) == 0);  // before wave 1 clamps to first
    CHECK(active_arena_index(arenas, 1) == 0);
    CHECK(active_arena_index(arenas, 2) == 0);
    CHECK(active_arena_index(arenas, 3) == 1);
    CHECK(active_arena_index(arenas, 4) == 1);
    CHECK(active_arena_index(arenas, 5) == 2);
    CHECK(active_arena_index(arenas, 99) == 2);

    CHECK(active_arena_index({}, 4) == -1);     // no arenas configured
}

TEST_CASE("push_circle_out_of_aabb leaves a non-overlapping circle untouched",
          "[Game][arena6][obstacle]") {
    // Circle well to the left of the box: no change.
    Vec2 r = push_circle_out_of_aabb(0.0f, 50.0f, 10.0f, 100.0f, 0.0f, 40.0f, 100.0f);
    CHECK_THAT(r.x, WithinAbs(0.0f, 1e-4));
    CHECK_THAT(r.y, WithinAbs(50.0f, 1e-4));
}

TEST_CASE("push_circle_out_of_aabb ejects an overlapping circle to the nearest face",
          "[Game][arena6][obstacle]") {
    // Box [100,140]x[0,100]. Circle centre just inside the left face, r=10.
    Vec2 r = push_circle_out_of_aabb(105.0f, 50.0f, 10.0f, 100.0f, 0.0f, 40.0f, 100.0f);
    // Nearest face is the left (x=100); centre ejected to 100 - r = 90.
    CHECK_THAT(r.x, WithinAbs(90.0f, 1e-4));
    CHECK_THAT(r.y, WithinAbs(50.0f, 1e-4));

    // Circle overlapping the left edge from outside gets pushed clear of it.
    Vec2 s = push_circle_out_of_aabb(96.0f, 50.0f, 10.0f, 100.0f, 0.0f, 40.0f, 100.0f);
    CHECK_THAT(s.x, WithinAbs(90.0f, 1e-4));  // closest point x=100, push to 100-10
    CHECK_THAT(s.y, WithinAbs(50.0f, 1e-4));
}

TEST_CASE("ProjectileHitSystem destroys a shot that meets an obstacle, no damage",
          "[Game][arena6][obstacle]") {
    EntityManager em; ComponentStorage storage; Blackboard bb;

    Entity obstacle = em.create_entity();
    storage.add_component<Collider>(obstacle,
        Collider{40.0f, 40.0f, layers::OBSTACLE, layers::OBSTACLE_MASK});

    Entity proj = em.create_entity();
    storage.add_component<ProjectileTag>(proj, ProjectileTag{});
    storage.add_component<ProjectileData>(proj, ProjectileData{NO_TARGET, 500.0f, 20.0f});
    storage.add_component<CollidedWith>(proj, CollidedWith{{obstacle}});

    ProjectileHitSystem sys;
    sys.update(em, storage, bb);

    CHECK(storage.has_component<DestroyRequest>(proj));                 // shot stopped
    CHECK(storage.entities_with_component<DamageEvent>().empty());      // obstacle takes none
}

TEST_CASE("PlayerDamageSystem takes contact damage from a hazard (no EnemyTag)",
          "[Game][arena6][hazard]") {
    EntityManager em; ComponentStorage storage; Blackboard bb;
    bb.set<double>("delta_time", 0.016);
    bb.set<float>("player.invuln_window", 0.8f);

    // A static hazard patch: ContactDamage but NOT an enemy.
    Entity hazard = em.create_entity();
    storage.add_component<ContactDamage>(hazard, ContactDamage{9.0f, 0, 0});

    Entity player = em.create_entity();
    storage.add_component<PlayerTag>(player, PlayerTag{});
    storage.add_component<Health>(player, Health{100.0f, 100.0f});
    storage.add_component<CollidedWith>(player, CollidedWith{{hazard}});

    PlayerDamageSystem sys;
    sys.update(em, storage, bb);

    auto events = storage.entities_with_component<DamageEvent>();
    REQUIRE(events.size() == 1);
    CHECK(storage.get_component<DamageEvent>(events[0])->get().target_entity == player);
    CHECK_THAT(storage.get_component<DamageEvent>(events[0])->get().amount, WithinAbs(9.0f, 1e-4));
}
