/**
 * test_enemy_fire.cpp — enemy projectiles and the three moon shooters (D66/D67).
 *
 * The load-bearing claims of Phase 6, each of which fails silently in a window:
 *   - a shooter fires on its cooldown and NOT before (a timer reset in the wrong
 *     place is a wave that opens with a volley);
 *   - tier 2's tracking is clamped, so a moving drone can out-turn it;
 *   - a shot dies on the thing it hits, and the tier-3 laser is the one that does
 *     not (that asymmetry IS the tier-3 upgrade);
 *   - the drone loses health through the EXISTING damage path, with no second
 *     damage system — the whole reason Phase 6 is small;
 *   - the moon types reach the spawn stream at waves 3 / 15 / 30 without any
 *     wave's `types` list mentioning them.
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/destruction.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/project_paths.hpp"
#include "game/arena_config.hpp"
#include "game/collision_layers.hpp"
#include "game/damage_apply_system.hpp"
#include "game/enemy_components.hpp"
#include "game/enemy_fire_system.hpp"
#include "game/player_components.hpp"
#include "game/player_damage_system.hpp"
#include "game/wave_spawner_system.hpp"

#include <cmath>

using Catch::Matchers::WithinAbs;

namespace {

constexpr float PI = 3.14159265358979f;

struct World {
    EntityManager em;
    ComponentStorage cs;
    Blackboard bb;
    GameConfig cfg;
    EnemyFireSystem sys;

    World() {
        // 0.25 is exact in binary: a cooldown test built on 0.1 would be deciding
        // its own boundary by float error rather than by the rule under test.
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

    Entity player(float x, float y) {
        Entity p = em.create_entity();
        cs.add_component<Position>(p, Position{x, y});
        cs.add_component<Size>(p, Size{40.0f, 40.0f});
        cs.add_component<PlayerTag>(p, PlayerTag{});
        cs.add_component<Health>(p, Health{100.0f, 100.0f});
        cs.add_component<ShipState>(p, ShipState{});
        return p;
    }

    Entity shooter(float x, float y, int tier, float cooldown) {
        Entity e = em.create_entity();
        cs.add_component<Position>(e, Position{x, y});
        cs.add_component<Size>(e, Size{60.0f, 60.0f});
        cs.add_component<EnemyTag>(e, EnemyTag{});
        cs.add_component<EnemyBehavior>(e,
            EnemyBehavior{behavior_kinds::SHOOTER, tier, cooldown, cooldown, 0.0f});
        return e;
    }

    size_t shots() const { return cs.entities_with_component<EnemyShot>().size(); }
};

}  // namespace

TEST_CASE("a shooter fires on its cooldown and not before", "[Game][enemyfire]") {
    World w;
    w.player(1000.0f, 0.0f);
    w.shooter(0.0f, 0.0f, 1, 1.0f);

    // 3 ticks of 0.25s = 0.75s: still armed, nothing fired.
    for (int i = 0; i < 3; ++i) w.sys.update(w.cs, w.em, w.bb);
    CHECK(w.shots() == 0);

    w.sys.update(w.cs, w.em, w.bb);   // 1.0s
    CHECK(w.shots() == 1);

    // ...and then it re-arms rather than firing every frame.
    for (int i = 0; i < 3; ++i) w.sys.update(w.cs, w.em, w.bb);
    CHECK(w.shots() == 1);
    w.sys.update(w.cs, w.em, w.bb);
    CHECK(w.shots() == 2);
}

TEST_CASE("a fired shot carries the damage payload and the enemy-shot layer",
          "[Game][enemyfire]") {
    World w;
    // Centres level: drone 40px at (1000,10) -> (1020,30); shooter 60px at
    // (0,0) -> (30,30). Due east, so the shot's dy must be exactly zero.
    w.player(1000.0f, 10.0f);
    w.shooter(0.0f, 0.0f, 1, 0.0f);
    w.sys.update(w.cs, w.em, w.bb);

    auto shots = w.cs.entities_with_component<EnemyShot>();
    REQUIRE(shots.size() == 1);
    const Entity s = shots[0];

    auto cd = w.cs.get_component<ContactDamage>(s);
    REQUIRE(cd.has_value());
    CHECK_THAT(cd->get().amount, WithinAbs(11.0f, 1e-4f));
    // A shot is not a kill: it must never pay score or currency.
    CHECK(cd->get().score == 0);
    CHECK(cd->get().currency == 0);

    auto col = w.cs.get_component<Collider>(s);
    REQUIRE(col.has_value());
    CHECK(col->get().layer == layers::ENEMY_SHOT);
    CHECK(w.cs.has_component<Lifetime>(s));

    // Fired at the drone, which is due east.
    auto v = w.cs.get_component<Velocity>(s);
    REQUIRE(v.has_value());
    CHECK(v->get().dx > 0.0f);
    CHECK_THAT(v->get().dy, WithinAbs(0.0f, 1e-3f));
}

TEST_CASE("tracking is clamped to a turn rate", "[Game][enemyfire]") {
    // Pure: the tier-2 shooter's whole counterplay is that it cannot snap around.
    CHECK_THAT(enemy_fire::turn_toward(0.0f, PI * 0.5f, 0.1f), WithinAbs(0.1f, 1e-5f));
    CHECK_THAT(enemy_fire::turn_toward(0.0f, -PI * 0.5f, 0.1f), WithinAbs(-0.1f, 1e-5f));
    // Short way round: 3.0 -> -3.0 is +0.283 across the wrap, NOT -6.0 the long
    // way. Getting this wrong is a shooter that spins the wrong way past pi.
    CHECK_THAT(enemy_fire::turn_toward(3.0f, -3.0f, 1.0f),
               WithinAbs(3.0f + (6.28318530718f - 6.0f), 1e-4f));
    // Never overshoots when the target is inside the clamp.
    CHECK_THAT(enemy_fire::turn_toward(0.0f, 0.05f, 1.0f), WithinAbs(0.05f, 1e-5f));
    // Only tier 2 tracks.
    CHECK(enemy_fire::shot_spec(2).turn_rate > 0.0f);
    CHECK_THAT(enemy_fire::shot_spec(1).turn_rate, WithinAbs(0.0f, 1e-6f));
}

TEST_CASE("a shot dies on contact and the tier-3 laser pierces", "[Game][enemyfire]") {
    World w;
    Entity wall = w.em.create_entity();

    Entity plain = enemy_fire::spawn_shot(w.cs, w.em, 0, 0, 0.0f, 100.0f, 5.0f, 1);
    Entity laser = enemy_fire::spawn_shot(w.cs, w.em, 0, 0, 0.0f, 100.0f, 5.0f, 3);
    CHECK(enemy_fire::shot_spec(3).pierce);
    CHECK_FALSE(enemy_fire::shot_spec(1).pierce);

    w.cs.add_component<CollidedWith>(plain, CollidedWith{{wall}});
    w.cs.add_component<CollidedWith>(laser, CollidedWith{{wall}});
    w.sys.update(w.cs, w.em, w.bb);

    CHECK(w.cs.has_component<DestroyRequest>(plain));
    CHECK_FALSE(w.cs.has_component<DestroyRequest>(laser));
}

TEST_CASE("the drone loses health through the existing damage path",
          "[Game][enemyfire]") {
    // No new damage system: PlayerDamageSystem reacts to ContactDamage in
    // CollidedWith, DamageApplySystem lands it. If this ever needs a third
    // system, Phase 6 was built wrong.
    World w;
    Entity p = w.player(0.0f, 0.0f);
    Entity shot = enemy_fire::spawn_shot(w.cs, w.em, 10.0f, 10.0f, 0.0f, 0.0f, 17.0f, 1);
    w.cs.add_component<CollidedWith>(p, CollidedWith{{shot}});

    PlayerDamageSystem pd;
    DamageApplySystem da;
    pd.update(w.em, w.cs, w.bb);
    da.update(w.em, w.cs);

    auto h = w.cs.get_component<Health>(p);
    REQUIRE(h.has_value());
    CHECK_THAT(h->get().current, WithinAbs(83.0f, 1e-3f));
}

TEST_CASE("the shipped moon types unlock at waves 3 / 15 / 30 without a wave roster",
          "[Game][enemyfire][config]") {
    GameConfig cfg = load_arena_config(project_paths::assets_dir() + "/GameData.json");

    int found = 0;
    for (const EnemyType& t : cfg.enemy_types) {
        if (enemy_fire::behavior_kind_for(t.behavior) != behavior_kinds::SHOOTER) continue;
        ++found;
        CHECK(t.first_wave > 0);
        CHECK(t.shot_damage > 0.0f);
    }
    CHECK(found == 3);

    CHECK(unlocked_injections(cfg.enemy_types, 1).empty());
    CHECK(unlocked_injections(cfg.enemy_types, 3).size() == 1);
    CHECK(unlocked_injections(cfg.enemy_types, 15).size() == 2);
    CHECK(unlocked_injections(cfg.enemy_types, 30).size() == 3);

    // No wave roster names them: that is the point of the injection cadence.
    const int first_moon = unlocked_injections(cfg.enemy_types, 50).front();
    for (const WaveDef& wv : cfg.waves)
        for (int t : wv.types) CHECK(t < first_moon);

    // The injection cadence itself is live.
    CHECK(cfg.specialty.moon_every_n_spawns > 0);
}

TEST_CASE("hard mode pulls the moon unlocks forward too", "[Game][enemyfire][config]") {
    // type_lookahead unlocks enemy types earlier. The moons are not in any wave's
    // roster, so without the extra step in apply_difficulty they would be the one
    // part of the roster Hard never accelerates.
    GameConfig cfg = load_arena_config(project_paths::assets_dir() + "/GameData.json");
    REQUIRE(cfg.difficulties.size() >= 2);
    const DifficultyDef& hard = cfg.difficulties[1];
    REQUIRE(hard.type_lookahead > 0);

    const int before = unlocked_injections(cfg.enemy_types, 3).front();
    apply_difficulty(cfg, hard);
    CHECK(cfg.enemy_types[static_cast<size_t>(before)].first_wave ==
          3 - hard.type_lookahead);
    CHECK(unlocked_injections(cfg.enemy_types, 3).size() == 1);
}
