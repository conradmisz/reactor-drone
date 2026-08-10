/**
 * test_weapon_items.cpp — Long Barrel (range) and Ricochet Coils (bounce), D97-D99.
 *
 * The reflection math is a pure helper (bullet_bounce.hpp) so it tests without a
 * window; the ricochet budget is tested through the real ProjectileHitSystem
 * against a hand-built obstacle and arena, and the two catalogue rows are tested
 * through ShopSystem's ordinary buy path so "levels correctly" means the same
 * stacking + price escalation every other upgrade row gets.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/components.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "game/arena_config.hpp"
#include "game/bullet_bounce.hpp"
#include "game/collision_layers.hpp"
#include "game/player_components.hpp"
#include "game/projectile_hit_system.hpp"
#include "game/shop_system.hpp"
#include "game/tower_components.hpp"

using Catch::Matchers::WithinAbs;

namespace {

constexpr float PR = 6.0f;   // projectile radius, as PlayerFireSystem spawns it

ShopConfig weapon_catalogue() {
    ShopConfig cfg;
    cfg.price_growth = 1.5f;
    cfg.upgrades = {
        ShopUpgradeDef{"Long Barrel",    "range",   80, 0.30f, 3, 0.0f},
        ShopUpgradeDef{"Ricochet Coils", "bounce", 180, 1.00f, 3, 0.0f},
    };
    return cfg;
}

/// A world with one player carrying the stock weapon, ready for ShopSystem.
struct ShopWorld {
    EntityManager em;
    ComponentStorage cs;
    Blackboard bb;
    Entity player = 0;

    ShopWorld() {
        player = em.create_entity();
        cs.add_component<PlayerTag>(player, PlayerTag{});
        cs.add_component<ShipState>(player, ShipState{});
        cs.add_component<WeaponStats>(player, WeaponStats{});
    }
    ShipState& ship() { return cs.get_component<ShipState>(player)->get(); }
    WeaponStats& weapon() { return cs.get_component<WeaponStats>(player)->get(); }
};

/// A world with one obstacle and one projectile, wired the way the game wires them.
struct HitWorld {
    EntityManager em;
    ComponentStorage cs;
    Blackboard bb;
    ArenaConfig arena;
    ProjectileHitSystem sys;

    HitWorld() {
        arena.center_x = 0.0f;
        arena.center_y = 0.0f;
        arena.radius = 500.0f;
        sys.set_arena(&arena);
    }

    Entity obstacle(float x, float y, float w, float h) {
        Entity e = em.create_entity();
        cs.add_component<Position>(e, Position{x, y});
        cs.add_component<Size>(e, Size{w, h});
        cs.add_component<Collider>(e, Collider{w, h, layers::OBSTACLE, layers::OBSTACLE_MASK});
        return e;
    }

    Entity shot(float cx, float cy, float vx, float vy, int bounces) {
        Entity e = em.create_entity();
        cs.add_component<Position>(e, Position{cx - PR, cy - PR});
        cs.add_component<Size>(e, Size{PR * 2.0f, PR * 2.0f});
        cs.add_component<Velocity>(e, Velocity{vx, vy});
        cs.add_component<ProjectileTag>(e, ProjectileTag{});
        cs.add_component<ProjectileData>(e, ProjectileData{NO_TARGET, 500.0f, 20.0f, bounces});
        return e;
    }

    void collide(Entity shot_e, Entity other) {
        CollidedWith c;
        c.entities.push_back(other);
        cs.add_component<CollidedWith>(shot_e, c);
    }
    void clear_collisions(Entity shot_e) {
        cs.remove_component<CollidedWith>(shot_e);
    }

    void tick() { sys.update(em, cs, bb); }

    bool dead(Entity e) { return cs.has_component<DestroyRequest>(e); }
    Position& pos(Entity e) { return cs.get_component<Position>(e)->get(); }
    Velocity& vel(Entity e) { return cs.get_component<Velocity>(e)->get(); }
    int bounces_left(Entity e) { return cs.get_component<ProjectileData>(e)->get().bounces; }
};

}  // namespace

// ---------------------------------------------------------------------------
// D97 — Long Barrel moves projectile_lifetime, so range = speed * lifetime.
// ---------------------------------------------------------------------------

TEST_CASE("Long Barrel extends travel distance by speed * added lifetime", "[Game][weapon_items][range]") {
    ShopWorld w;
    ShopConfig cfg = weapon_catalogue();
    ShopSystem shop;
    shop.set_config(&cfg);
    shop.open(w.cs, w.em, w.bb);   // update() only acts on an open shop
    w.ship().currency = 5000;

    const float speed0 = w.weapon().projectile_speed;
    const float range0 = speed0 * w.weapon().projectile_lifetime;

    shop.update(w.cs, w.bb, 1, false, false);   // buy row 1 = Long Barrel

    // Speed is untouched — only where the shot expires moved (D97).
    REQUIRE_THAT(w.weapon().projectile_speed, WithinAbs(speed0, 1e-4f));
    REQUIRE_THAT(w.weapon().projectile_lifetime - 1.2f, WithinAbs(0.30f, 1e-4f));
    const float range1 = w.weapon().projectile_speed * w.weapon().projectile_lifetime;
    REQUIRE_THAT(range1 - range0, WithinAbs(speed0 * 0.30f, 1e-2f));
}

TEST_CASE("Long Barrel levels by stacking and stops at max_stacks", "[Game][weapon_items][range]") {
    ShopWorld w;
    ShopConfig cfg = weapon_catalogue();
    ShopSystem shop;
    shop.set_config(&cfg);
    shop.open(w.cs, w.em, w.bb);   // update() only acts on an open shop
    w.ship().currency = 5000;

    for (int i = 0; i < 5; ++i) shop.update(w.cs, w.bb, 1, false, false);

    REQUIRE(w.ship().upg_counts[0] == 3);                       // max_stacks honoured
    REQUIRE_THAT(w.weapon().projectile_lifetime - 1.2f, WithinAbs(0.90f, 1e-4f));
    // Escalating price, same curve as every other upgrade row.
    REQUIRE(shop.price_for(0, 0) == 80);
    REQUIRE(shop.price_for(0, 1) == 120);
    REQUIRE(shop.price_for(0, 2) == 180);
}

// ---------------------------------------------------------------------------
// D98 — Ricochet Coils. Catalogue level -> Blackboard count -> per-shot budget.
// ---------------------------------------------------------------------------

TEST_CASE("Ricochet Coils levels the bounce count on the Blackboard", "[Game][weapon_items][bounce]") {
    ShopWorld w;
    ShopConfig cfg = weapon_catalogue();
    ShopSystem shop;
    shop.set_config(&cfg);
    shop.open(w.cs, w.em, w.bb);   // update() only acts on an open shop
    w.ship().currency = 5000;

    REQUIRE(w.bb.get_or<int>("ship.bounces", 0) == 0);
    shop.update(w.cs, w.bb, 2, false, false);
    REQUIRE(w.bb.get_or<int>("ship.bounces", 0) == 1);
    shop.update(w.cs, w.bb, 2, false, false);
    shop.update(w.cs, w.bb, 2, false, false);
    REQUIRE(w.bb.get_or<int>("ship.bounces", 0) == 3);
    shop.update(w.cs, w.bb, 2, false, false);   // maxed
    REQUIRE(w.bb.get_or<int>("ship.bounces", 0) == 3);
    REQUIRE(w.ship().upg_counts[1] == 3);
}

TEST_CASE("A shot with no bounces still stops dead on an obstacle", "[Game][weapon_items][bounce]") {
    HitWorld w;
    Entity ob = w.obstacle(0.0f, -50.0f, 20.0f, 100.0f);
    Entity s = w.shot(-8.0f, 0.0f, 500.0f, 0.0f, 0);
    w.collide(s, ob);
    w.tick();
    REQUIRE(w.dead(s));
}

TEST_CASE("A bouncing shot reflects off an obstacle instead of dying", "[Game][weapon_items][bounce]") {
    HitWorld w;
    Entity ob = w.obstacle(0.0f, -50.0f, 20.0f, 100.0f);   // wall face at x = 0
    Entity s = w.shot(-3.0f, 0.0f, 500.0f, 0.0f, 2);       // penetrating, moving +x
    w.collide(s, ob);
    w.tick();

    REQUIRE_FALSE(w.dead(s));
    REQUIRE(w.bounces_left(s) == 1);
    REQUIRE_THAT(w.vel(s).dx, WithinAbs(-500.0f, 1e-3f));   // mirrored in x
    REQUIRE_THAT(w.vel(s).dy, WithinAbs(0.0f, 1e-3f));      // y untouched
}

TEST_CASE("A bounced shot is clear of the surface it left", "[Game][weapon_items][bounce]") {
    HitWorld w;
    Entity ob = w.obstacle(0.0f, -50.0f, 20.0f, 100.0f);
    Entity s = w.shot(-3.0f, 0.0f, 500.0f, 0.0f, 3);
    w.collide(s, ob);
    w.tick();

    // Centre is strictly outside the face by more than the radius, so a second
    // broadphase pass on the same frame reports no overlap at all.
    const float cx = w.pos(s).x + PR;
    REQUIRE(cx < -PR);

    // Prove it: feed the same collision again without moving the shot. The
    // reflection helper refuses (no penetration), so no bounce is spent...
    w.tick();
    REQUIRE(w.bounces_left(s) == 2);
    REQUIRE_FALSE(w.dead(s));
}

TEST_CASE("The bounce budget is finite and the shot dies when it runs out",
          "[Game][weapon_items][bounce]") {
    HitWorld w;
    Entity ob = w.obstacle(0.0f, -50.0f, 20.0f, 100.0f);
    Entity s = w.shot(-3.0f, 0.0f, 500.0f, 0.0f, 2);

    for (int i = 0; i < 2; ++i) {
        // Re-penetrate the wall each iteration: the shot is "flown back into" it.
        w.pos(s).x = -3.0f - PR;
        w.pos(s).y = -PR;
        w.vel(s).dx = 500.0f;
        w.clear_collisions(s);
        w.collide(s, ob);
        w.tick();
        REQUIRE_FALSE(w.dead(s));
    }
    REQUIRE(w.bounces_left(s) == 0);

    w.pos(s).x = -3.0f - PR;
    w.vel(s).dx = 500.0f;
    w.clear_collisions(s);
    w.collide(s, ob);
    w.tick();
    REQUIRE(w.dead(s));   // budget spent: the wall stops it dead again
}

TEST_CASE("A ricochet reflects off the arena ring, which is not a collider",
          "[Game][weapon_items][bounce]") {
    HitWorld w;
    // Ring radius 500 about the origin; the shot is just past the limit heading out.
    Entity s = w.shot(499.0f, 0.0f, 500.0f, 0.0f, 1);
    w.tick();

    REQUIRE_FALSE(w.dead(s));
    REQUIRE(w.bounces_left(s) == 0);
    REQUIRE_THAT(w.vel(s).dx, WithinAbs(-500.0f, 1e-3f));
    REQUIRE((w.pos(s).x + PR) < 500.0f - PR);   // placed back inside the ring

    // With the budget spent it flies out and expires on Lifetime, as before.
    w.pos(s).x = 499.0f - PR;
    w.vel(s).dx = 500.0f;
    w.tick();
    REQUIRE_THAT(w.vel(s).dx, WithinAbs(500.0f, 1e-3f));
}

TEST_CASE("An enemy outranks a wall in the same frame", "[Game][weapon_items][bounce]") {
    HitWorld w;
    Entity ob = w.obstacle(0.0f, -50.0f, 20.0f, 100.0f);
    Entity enemy = w.em.create_entity();
    w.cs.add_component<EnemyTag>(enemy, EnemyTag{});
    w.cs.add_component<Position>(enemy, Position{0.0f, 0.0f});

    Entity s = w.shot(-3.0f, 0.0f, 500.0f, 0.0f, 3);
    CollidedWith c;
    c.entities.push_back(ob);      // wall listed FIRST
    c.entities.push_back(enemy);
    w.cs.add_component<CollidedWith>(s, c);
    w.tick();

    REQUIRE(w.dead(s));            // the hit landed, the ricochet did not eat it
    REQUIRE(w.bounces_left(s) == 3);
    bool damaged = false;
    for (Entity e : w.cs.entities_with_component<DamageEvent>())
        if (w.cs.get_component<DamageEvent>(e)->get().target_entity == enemy) damaged = true;
    REQUIRE(damaged);
}

// ---------------------------------------------------------------------------
// D99 — the pure reflection helper.
// ---------------------------------------------------------------------------

TEST_CASE("bounce::off_aabb mirrors through the outward face normal", "[Game][weapon_items][bounce_math]") {
    bounce::Result r;

    // Head-on into the left face of a box at x in [0,20].
    REQUIRE(bounce::off_aabb(-3.0f, 5.0f, PR, 100.0f, 0.0f, 0.0f, 0.0f, 20.0f, 20.0f, r));
    REQUIRE_THAT(r.vx, WithinAbs(-100.0f, 1e-3f));
    REQUIRE_THAT(r.vy, WithinAbs(0.0f, 1e-3f));

    // Diagonal into the same face keeps the tangential component.
    REQUIRE(bounce::off_aabb(-3.0f, 5.0f, PR, 100.0f, 60.0f, 0.0f, 0.0f, 20.0f, 20.0f, r));
    REQUIRE_THAT(r.vx, WithinAbs(-100.0f, 1e-3f));
    REQUIRE_THAT(r.vy, WithinAbs(60.0f, 1e-3f));

    // Speed is conserved: a ricochet must not accelerate or stall the shot.
    const float sp = std::sqrt(r.vx * r.vx + r.vy * r.vy);
    REQUIRE_THAT(sp, WithinAbs(std::sqrt(100.0f * 100.0f + 60.0f * 60.0f), 1e-2f));
}

TEST_CASE("bounce::off_aabb refuses a clear circle and one already leaving",
          "[Game][weapon_items][bounce_math]") {
    bounce::Result r;
    REQUIRE_FALSE(bounce::off_aabb(-50.0f, 5.0f, PR, 100.0f, 0.0f, 0.0f, 0.0f, 20.0f, 20.0f, r));
    // Penetrating but travelling away from the face — a clipped corner, not a hit.
    REQUIRE_FALSE(bounce::off_aabb(-3.0f, 5.0f, PR, -100.0f, 0.0f, 0.0f, 0.0f, 20.0f, 20.0f, r));
}

TEST_CASE("bounce::inside_circle reflects inward off the ring", "[Game][weapon_items][bounce_math]") {
    bounce::Result r;
    REQUIRE_FALSE(bounce::inside_circle(0.0f, 0.0f, PR, 100.0f, 0.0f, 0.0f, 0.0f, 500.0f, r));
    REQUIRE(bounce::inside_circle(499.0f, 0.0f, PR, 100.0f, 0.0f, 0.0f, 0.0f, 500.0f, r));
    REQUIRE_THAT(r.vx, WithinAbs(-100.0f, 1e-3f));
    REQUIRE(std::sqrt(r.cx * r.cx + r.cy * r.cy) < 500.0f - PR);
    // Already heading back in: nothing to do.
    REQUIRE_FALSE(bounce::inside_circle(499.0f, 0.0f, PR, -100.0f, 0.0f, 0.0f, 0.0f, 500.0f, r));
}
