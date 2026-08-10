/**
 * test_moon_muzzle.cpp — iteration 5 item #3 (D109): a moon shooter fires from
 * the crescent's MOUTH and turns to face what it shoots.
 *
 * Two claims that only fail visually, and so cannot be seen from a build log:
 *   - the muzzle is ahead of the entity's centre, never behind it (a negative
 *     offset is the original bug wearing a different sign);
 *   - the entity gains a Rotation equal to its aim, so the mouth is on the side
 *     the shot leaves from — without it the sprite points right forever.
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "game/arena_config.hpp"
#include "game/enemy_components.hpp"
#include "game/enemy_fire_system.hpp"
#include "game/player_components.hpp"

#include <cmath>

using Catch::Matchers::WithinAbs;

namespace {

struct Fixture {
    EntityManager em;
    ComponentStorage cs;
    Blackboard bb;
    GameConfig cfg;
    EnemyFireSystem sys;

    Fixture() {
        bb.set<double>("delta_time", 0.25);
        EnemyType moon;
        moon.name = "moon_1";
        moon.behavior = "shooter";
        moon.behavior_tier = 1;
        moon.fire_interval = 1.0f;
        moon.shot_speed = 200.0f;
        moon.shot_damage = 11.0f;
        cfg.enemy_types.push_back(moon);
        sys.set_config(&cfg);
    }

    void player(float x, float y) {
        Entity p = em.create_entity();
        cs.add_component<Position>(p, Position{x, y});
        cs.add_component<Size>(p, Size{40.0f, 40.0f});
        cs.add_component<PlayerTag>(p, PlayerTag{});
        cs.add_component<Health>(p, Health{100.0f, 100.0f});
        cs.add_component<ShipState>(p, ShipState{});
    }

    Entity shooter(float x, float y, int tier) {
        Entity e = em.create_entity();
        cs.add_component<Position>(e, Position{x, y});
        cs.add_component<Size>(e, Size{60.0f, 60.0f});
        cs.add_component<EnemyTag>(e, EnemyTag{});
        cs.add_component<EnemyBehavior>(e,
            EnemyBehavior{behavior_kinds::SHOOTER, tier, 0.0f, 1.0f, 0.0f});
        return e;
    }
};

}  // namespace

TEST_CASE("the moon muzzle is ahead of the centre and deepens with tier",
          "[Game][moonmuzzle]") {
    // The bug was a shot leaving the shadowed (back) lobe: any value <= 0 is
    // that bug. Half the sprite would be outside the art entirely.
    for (int tier : {1, 2, 3}) {
        const float f = enemy_fire::moon_muzzle_frac(tier);
        CHECK(f > 0.0f);
        CHECK(f < 0.5f);
    }
    CHECK(enemy_fire::moon_muzzle_frac(1) < enemy_fire::moon_muzzle_frac(2));
    CHECK(enemy_fire::moon_muzzle_frac(2) < enemy_fire::moon_muzzle_frac(3));
    // Unknown tiers fall back to tier 1 rather than to zero.
    CHECK(enemy_fire::moon_muzzle_frac(0) == enemy_fire::moon_muzzle_frac(1));
}

TEST_CASE("a shot spawns at the mouth, offset along the aim", "[Game][moonmuzzle]") {
    Fixture w;
    // Drone due WEST: 40px at (-1000,10) -> centre (-980,30); shooter 60px at
    // (0,0) -> centre (30,30). The muzzle must therefore move -x, which is the
    // case the old centre-spawn could never distinguish.
    w.player(-1000.0f, 10.0f);
    w.shooter(0.0f, 0.0f, 1);
    w.sys.update(w.cs, w.em, w.bb);

    auto shots = w.cs.entities_with_component<EnemyShot>();
    REQUIRE(shots.size() == 1);
    auto pos = w.cs.get_component<Position>(shots[0]);
    auto size = w.cs.get_component<Size>(shots[0]);
    REQUIRE(pos.has_value());
    REQUIRE(size.has_value());
    const float sx = pos->get().x + size->get().width * 0.5f;
    const float sy = pos->get().y + size->get().height * 0.5f;

    const float expect = 30.0f - enemy_fire::moon_muzzle_frac(1) * 60.0f;
    CHECK_THAT(sx, WithinAbs(expect, 0.01f));
    CHECK_THAT(sy, WithinAbs(30.0f, 0.01f));
    CHECK(sx < 30.0f);   // in front of the mouth, not behind the moon
}

TEST_CASE("a shooter turns to face its target with pure rotation",
          "[Game][moonmuzzle]") {
    Fixture w;
    // Centres aligned on x: drone 40px at (10,1000) -> (30,1020); shooter
    // 60px at (0,0) -> (30,30). Due north, so the aim is exactly +pi/2.
    w.player(10.0f, 1000.0f);
    Entity e = w.shooter(0.0f, 0.0f, 1);
    CHECK_FALSE(w.cs.has_component<Rotation>(e));

    w.sys.update(w.cs, w.em, w.bb);
    auto rot = w.cs.get_component<Rotation>(e);
    REQUIRE(rot.has_value());
    CHECK_THAT(rot->get().angle, WithinAbs(3.14159265f * 0.5f, 0.01f));
    // Symmetric art: the renderer's face-left flip heuristic must stay off, or
    // the crescent mirrors instead of turning.
    CHECK_FALSE(rot->get().flip_when_left);

    // A second tick reuses the component rather than stacking another one.
    w.sys.update(w.cs, w.em, w.bb);
    CHECK(w.cs.entities_with_component<Rotation>().size() == 1);
}
