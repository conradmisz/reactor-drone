/**
 * test_dash.cpp — the thruster dash (#5, D57).
 *
 * The dash is a burst that lasts several frames while overlapping several
 * bodies, which is exactly the shape that turns into an accidental blender: nine
 * frames x N enemies x cfg.damage. The three rules that stop that — cooldown
 * gating, each enemy damaged once, and at most one hit taken per dash — are all
 * frame-count-sensitive and all silent when broken, so they are pinned here.
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/project_paths.hpp"
#include "game/arena_config.hpp"
#include "game/dash_system.hpp"
#include "game/enemy_components.hpp"
#include "game/player_components.hpp"
#include "game/tower_components.hpp"

using Catch::Matchers::WithinAbs;

namespace {

constexpr float DT = 1.0f / 60.0f;

DashConfig test_cfg() {
    DashConfig c;
    c.speed = 900.0f;
    c.duration = 0.15f;
    c.cooldown = 2.5f;
    c.damage = 30.0f;
    return c;
}

Entity make_player(ComponentStorage& cs, EntityManager& em, float vx = 100.0f) {
    Entity p = em.create_entity();
    cs.add_component<Position>(p, Position{0.0f, 0.0f});
    cs.add_component<Size>(p, Size{40.0f, 40.0f});
    cs.add_component<Velocity>(p, Velocity{vx, 0.0f});
    cs.add_component<Rotation>(p, Rotation{0.0f, 0.0f, false});
    cs.add_component<PlayerTag>(p, PlayerTag{});
    cs.add_component<ShipState>(p, ShipState{});
    return p;
}

Entity make_enemy(ComponentStorage& cs, EntityManager& em, float x, float y) {
    Entity e = em.create_entity();
    cs.add_component<Position>(e, Position{x, y});
    cs.add_component<Size>(e, Size{30.0f, 30.0f});
    cs.add_component<EnemyTag>(e, EnemyTag{});
    cs.add_component<Health>(e, Health{100.0f, 100.0f, 1.0f});
    return e;
}

/// Total damage queued against `target` in DamageEvent entities so far.
float queued_damage(const ComponentStorage& cs, Entity target) {
    float total = 0.0f;
    for (Entity e : cs.entities_with_component<DamageEvent>()) {
        const auto& ev = cs.get_component<DamageEvent>(e)->get();
        if (ev.target_entity == target) total += ev.amount;
    }
    return total;
}

int damage_event_count(const ComponentStorage& cs, Entity target) {
    int n = 0;
    for (Entity e : cs.entities_with_component<DamageEvent>()) {
        if (cs.get_component<DamageEvent>(e)->get().target_entity == target) ++n;
    }
    return n;
}

}  // namespace

TEST_CASE("the dash overrides movement for its duration and then stops",
          "[Game][dash]") {
    ComponentStorage cs;
    EntityManager em;
    Blackboard bb;
    DashState st;
    const DashConfig cfg = test_cfg();
    Entity p = make_player(cs, em, /*vx=*/100.0f);

    tick_dash(cs, em, bb, cfg, st, /*key_down=*/true, DT);
    CHECK_THAT(cs.get_component<Velocity>(p)->get().dx, WithinAbs(cfg.speed, 1e-3f));
    CHECK(cs.get_component<ShipState>(p)->get().dash_timer > 0.0f);

    // 0.15s at 60Hz is 9 frames; run well past it with the key released.
    for (int i = 0; i < 20; ++i) {
        cs.get_component<Velocity>(p)->get() = Velocity{100.0f, 0.0f};
        tick_dash(cs, em, bb, cfg, st, /*key_down=*/false, DT);
    }
    CHECK_THAT(cs.get_component<ShipState>(p)->get().dash_timer, WithinAbs(0.0f, 1e-6f));
    CHECK_THAT(cs.get_component<Velocity>(p)->get().dx, WithinAbs(100.0f, 1e-3f));
}

TEST_CASE("the dash is gated by its cooldown", "[Game][dash][cooldown]") {
    ComponentStorage cs;
    EntityManager em;
    Blackboard bb;
    DashState st;
    const DashConfig cfg = test_cfg();
    Entity p = make_player(cs, em);
    ShipState& ship = cs.get_component<ShipState>(p)->get();

    tick_dash(cs, em, bb, cfg, st, true, DT);
    CHECK_THAT(ship.dash_cd, WithinAbs(cfg.cooldown, 1e-4f));

    // Holding the key through the whole cooldown must not re-trigger. Count how
    // many times the burst restarts across 2.4s (< cooldown).
    int retriggers = 0;
    for (int i = 0; i < 144; ++i) {
        const float before = ship.dash_timer;
        tick_dash(cs, em, bb, cfg, st, true, DT);
        if (ship.dash_timer > before) ++retriggers;
    }
    CHECK(retriggers == 0);

    // Past the cooldown it fires again — held, not edge-triggered (D57).
    bool refired = false;
    for (int i = 0; i < 20 && !refired; ++i) {
        const float before = ship.dash_timer;
        tick_dash(cs, em, bb, cfg, st, true, DT);
        refired = ship.dash_timer > before;
    }
    CHECK(refired);
}

TEST_CASE("each enemy the dash passes through takes damage exactly once",
          "[Game][dash][damage]") {
    ComponentStorage cs;
    EntityManager em;
    Blackboard bb;
    DashState st;
    const DashConfig cfg = test_cfg();
    make_player(cs, em);
    // Two bodies parked on top of the drone for the whole burst.
    Entity a = make_enemy(cs, em, 5.0f, 5.0f);
    Entity b = make_enemy(cs, em, 10.0f, 5.0f);

    for (int i = 0; i < 12; ++i) tick_dash(cs, em, bb, cfg, st, i == 0, DT);

    CHECK(damage_event_count(cs, a) == 1);
    CHECK(damage_event_count(cs, b) == 1);
    CHECK_THAT(queued_damage(cs, a), WithinAbs(cfg.damage, 1e-4f));
    CHECK_THAT(queued_damage(cs, b), WithinAbs(cfg.damage, 1e-4f));
}

TEST_CASE("a fresh dash can damage an enemy the previous one already hit",
          "[Game][dash][damage]") {
    ComponentStorage cs;
    EntityManager em;
    Blackboard bb;
    DashState st;
    DashConfig cfg = test_cfg();
    cfg.cooldown = 0.1f;   // so a second dash is available inside the test
    make_player(cs, em);
    Entity a = make_enemy(cs, em, 5.0f, 5.0f);

    for (int i = 0; i < 12; ++i) tick_dash(cs, em, bb, cfg, st, i == 0, DT);
    REQUIRE(damage_event_count(cs, a) == 1);
    for (int i = 0; i < 12; ++i) tick_dash(cs, em, bb, cfg, st, i == 0, DT);
    CHECK(damage_event_count(cs, a) == 2);
}

TEST_CASE("one dash costs the player at most one hit", "[Game][dash][iframes]") {
    ComponentStorage cs;
    EntityManager em;
    Blackboard bb;
    DashState st;
    const DashConfig cfg = test_cfg();
    make_player(cs, em);
    for (int i = 0; i < 5; ++i) make_enemy(cs, em, 5.0f + static_cast<float>(i), 5.0f);

    // Frame 1: first contact. The hit is deliberately let through — i-frames are
    // still zero so PlayerDamageSystem resolves it normally this frame.
    tick_dash(cs, em, bb, cfg, st, true, DT);
    CHECK_THAT(bb.get_or<float>("player.iframes", 0.0f), WithinAbs(0.0f, 1e-6f));

    // Every later frame of the burst holds i-frames past the end of the dash, so
    // the other four bodies cannot land a second hit.
    for (int i = 0; i < 8; ++i) {
        tick_dash(cs, em, bb, cfg, st, false, DT);
        CHECK(bb.get_or<float>("player.iframes", 0.0f) > 0.0f);
    }
}

TEST_CASE("a dash through empty space never touches the i-frame timer",
          "[Game][dash][iframes]") {
    ComponentStorage cs;
    EntityManager em;
    Blackboard bb;
    DashState st;
    const DashConfig cfg = test_cfg();
    make_player(cs, em);
    make_enemy(cs, em, 4000.0f, 4000.0f);   // far away

    for (int i = 0; i < 12; ++i) tick_dash(cs, em, bb, cfg, st, i == 0, DT);
    CHECK_THAT(bb.get_or<float>("player.iframes", 0.0f), WithinAbs(0.0f, 1e-6f));
    CHECK(cs.entities_with_component<DamageEvent>().empty());
}

TEST_CASE("a standing-still dash goes where the drone is aiming",
          "[Game][dash]") {
    ComponentStorage cs;
    EntityManager em;
    Blackboard bb;
    DashState st;
    const DashConfig cfg = test_cfg();
    Entity p = make_player(cs, em, /*vx=*/0.0f);
    cs.get_component<Velocity>(p)->get() = Velocity{0.0f, 0.0f};
    cs.get_component<Rotation>(p)->get().angle = 1.5707963f;   // straight up

    tick_dash(cs, em, bb, cfg, st, true, DT);
    const Velocity& v = cs.get_component<Velocity>(p)->get();
    CHECK_THAT(v.dx, WithinAbs(0.0f, 1e-2f));
    CHECK_THAT(v.dy, WithinAbs(cfg.speed, 1e-2f));
}

TEST_CASE("the shipped GameData dash block is live and sane", "[Game][dash][config]") {
    GameConfig cfg = load_arena_config(project_paths::assets_dir() + "/GameData.json");
    CHECK(cfg.dash.duration > 0.0f);
    CHECK(cfg.dash.cooldown > cfg.dash.duration);   // or it is a permanent speed buff
    CHECK(cfg.dash.speed > 0.0f);
}

TEST_CASE("a triggered dash bumps the tm.dashes counter", "[Game][dash][telemetry]") {
    ComponentStorage cs;
    EntityManager em;
    Blackboard bb;
    DashState st;
    const DashConfig cfg = test_cfg();
    make_player(cs, em);

    REQUIRE(bb.get_or<double>("tm.dashes", 0.0) == 0.0);
    tick_dash(cs, em, bb, cfg, st, /*key_down=*/true, DT);
    REQUIRE(bb.get_or<double>("tm.dashes", 0.0) == 1.0);

    // Held through the cooldown: no re-trigger, so no double count.
    for (int i = 0; i < 60; ++i) tick_dash(cs, em, bb, cfg, st, true, DT);
    REQUIRE(bb.get_or<double>("tm.dashes", 0.0) == 1.0);
}
