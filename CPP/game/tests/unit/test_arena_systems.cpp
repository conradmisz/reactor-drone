/**
 * Unit tests for the Class-110 "Reactor Drone" arena systems: aim math, the
 * currency economy (loot drops, pickup collection), ring spawning,
 * collision-based projectile hits, and player contact damage with i-frames.
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
#include "game/enemy_death_system.hpp"
#include "game/pickup_system.hpp"
#include "game/wave_spawner_system.hpp"
#include "game/projectile_hit_system.hpp"
#include "game/player_damage_system.hpp"
#include "game/shop_system.hpp"
#include "game/shield_system.hpp"
#include "game/item_system.hpp"
#include "engine/ecs/destruction.hpp"

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

// v2 Gameplay Phase 2: the currency economy.

TEST_CASE("A dead enemy drops currency pickups carrying its type's value",
          "[Game][arena][economy]") {
    EntityManager em; ComponentStorage storage; Blackboard bb;
    Entity e = em.create_entity();
    storage.add_component<EnemyTag>(e, EnemyTag{});
    storage.add_component<Health>(e, Health{0.0f, 20.0f});
    storage.add_component<Position>(e, Position{200.0f, 300.0f});
    storage.add_component<Size>(e, Size{64.0f, 64.0f});
    storage.add_component<ContactDamage>(e, ContactDamage{8.0f, 15, 2});

    EconomyConfig ec;
    ec.min_drops = 2; ec.max_drops = 2;   // pin the roll so the count is exact
    ec.key_drop_chance = 0.0f;            // ... and no key
    EnemyDeathSystem sys;
    sys.set_economy(ec, 1234u);
    sys.update(storage, em, bb);

    CHECK(bb.get_or<int>("score", 0) == 15);
    auto drops = storage.entities_with_component<Pickup>();
    REQUIRE(drops.size() == 2);
    for (Entity d : drops) {
        const Pickup& pk = storage.get_component<Pickup>(d)->get();
        CHECK(pk.kind == static_cast<int>(PickupKind::Currency));
        CHECK(pk.value == 4);   // type value 2 + D193's flat base bonus of 2
        CHECK(storage.has_component<Lifetime>(d));   // uncollected loot expires
    }
}

TEST_CASE("PickupSystem credits the player on overlap and destroys the pickup",
          "[Game][arena][economy]") {
    EntityManager em; ComponentStorage storage; Blackboard bb;
    bb.set<double>("delta_time", 1.0 / 60.0);

    Entity p = em.create_entity();
    storage.add_component<PlayerTag>(p, PlayerTag{});
    storage.add_component<Position>(p, Position{100.0f, 100.0f});
    storage.add_component<Size>(p, Size{40.0f, 40.0f});   // centre (120,120), r=20
    storage.add_component<ShipState>(p, ShipState{});

    auto loot = [&](float x, float y, PickupKind kind, int value) {
        Entity e = em.create_entity();
        storage.add_component<Position>(e, Position{x, y});
        storage.add_component<Size>(e, Size{16.0f, 16.0f});
        storage.add_component<Pickup>(e, Pickup{static_cast<int>(kind), value, 400.0f});
        return e;
    };
    Entity on_top = loot(115.0f, 115.0f, PickupKind::Currency, 5);   // centre (123,123)
    Entity a_key  = loot(115.0f, 115.0f, PickupKind::Key, 1);
    Entity far    = loot(600.0f, 600.0f, PickupKind::Currency, 9);

    PickupSystem sys;                     // default economy; item_id = -1 so no magnet
    sys.update(storage, em, bb);

    const ShipState& ship = storage.get_component<ShipState>(p)->get();
    CHECK(ship.currency == 5);
    CHECK(ship.keys == 1);
    CHECK(storage.has_component<DestroyRequest>(on_top));
    CHECK(storage.has_component<DestroyRequest>(a_key));
    CHECK_FALSE(storage.has_component<DestroyRequest>(far));   // out of reach, untouched

    // The far pickup must not drift without the Magnet Core equipped.
    CHECK(storage.get_component<Position>(far)->get().x == 600.0f);
}

// R1: the whole point of the destruction.cpp line for a newly registered
// component. Without it the map entry outlives the entity and the next entity to
// be handed that recycled id silently inherits the dead one's loot.
TEST_CASE("Destroying a pickup clears it from a recycled entity id",
          "[Game][arena][economy]") {
    EntityManager em; ComponentStorage storage;

    Entity e = em.create_entity();
    storage.add_component<Pickup>(e, Pickup{static_cast<int>(PickupKind::Key), 7, 400.0f});
    storage.add_component<ShipState>(e, ShipState{});
    storage.get_component<ShipState>(e)->get().currency = 99;
    storage.add_component<DestroyRequest>(e, DestroyRequest{});
    destroy_marked_entities(em, storage);

    CHECK_FALSE(storage.has_component<Pickup>(e));
    CHECK_FALSE(storage.has_component<ShipState>(e));

    Entity recycled = em.create_entity();
    CHECK_FALSE(storage.has_component<Pickup>(recycled));
    CHECK_FALSE(storage.has_component<ShipState>(recycled));
}

TEST_CASE("WaveSpawnerSystem total_waves honors victory_wave", "[Game][arena][waves]") {
    GameConfig cfg;
    cfg.waves.resize(6);
    WaveSpawnerSystem s;
    cfg.victory_wave = 0; s.set_config(&cfg); CHECK(s.total_waves() == 6);
    cfg.victory_wave = 3; s.set_config(&cfg); CHECK(s.total_waves() == 3);
    cfg.victory_wave = 9; s.set_config(&cfg); CHECK(s.total_waves() == 6); // clamped to available
}

// v2 Gameplay Phase 1: timed waves + arena-clear gating.
static GameConfig two_wave_config() {
    GameConfig cfg;
    cfg.enemy_types.resize(1);            // no sidecar -> Color-rect fallback, no SDL needed
    cfg.enemy_types[0].size = 64.0f;
    cfg.enemy_types[0].health = 20.0f;
    cfg.enemy_types[0].speed = 100.0f;
    cfg.waves.resize(2);
    cfg.waves[0].delay = 0.0f;
    cfg.waves[0].duration = 1.0f;         // timed
    cfg.waves[0].spawn_interval = 0.1f;
    cfg.waves[0].hp_mult = 2.0f;
    cfg.waves[0].speed_mult = 0.5f;
    return cfg;
}

TEST_CASE("A timed wave spawns for its duration, then waits for a clear arena",
          "[Game][arena][waves]") {
    GameConfig cfg = two_wave_config();
    EntityManager em; ComponentStorage storage; Blackboard bb;
    WaveSpawnerSystem s;
    s.set_config(&cfg);

    bb.set<double>("delta_time", 0.05);
    for (int i = 0; i < 40; ++i) s.update(bb, em, storage);   // 2.0 s — twice the duration

    auto alive = storage.entities_with_component<EnemyTag>();
    // duration / spawn_interval spawns, give or take one frame of rounding.
    CHECK(alive.size() >= 8);
    CHECK(alive.size() <= 12);
    CHECK(s.current_wave_index() == 0);      // still wave 1: the arena is not clear
    CHECK(s.wave_just_cleared() == false);

    // Per-wave multipliers scale the shared enemy type.
    auto h = storage.get_component<Health>(alive.front());
    REQUIRE(h.has_value());
    CHECK_THAT(h->get().max_hp, WithinAbs(40.0f, 1e-3));        // 20 * hp_mult 2.0
    auto pf = storage.get_component<PathFollower>(alive.front());
    REQUIRE(pf.has_value());
    CHECK_THAT(pf->get().speed, WithinAbs(50.0f, 1e-3));     // 100 * speed_mult 0.5

    for (Entity e : alive) storage.remove_component<EnemyTag>(e);
    s.update(bb, em, storage);
    CHECK(s.current_wave_index() == 1);      // cleared -> advanced
    CHECK(s.wave_just_cleared() == true);    // one-shot edge the shop hooks onto
    s.update(bb, em, storage);
    CHECK(s.wave_just_cleared() == false);
}

TEST_CASE("A stalled wave force-kills stragglers instead of soft-locking",
          "[Game][arena][waves]") {
    GameConfig cfg = two_wave_config();
    cfg.wave_stall_timeout = 1.0f;
    EntityManager em; ComponentStorage storage; Blackboard bb;
    WaveSpawnerSystem s;
    s.set_config(&cfg);

    bb.set<double>("delta_time", 0.05);
    for (int i = 0; i < 60; ++i) s.update(bb, em, storage);  // 3 s: 1 s spawning, 2 s stalled

    for (Entity e : storage.entities_with_component<EnemyTag>()) {
        auto h = storage.get_component<Health>(e);
        REQUIRE(h.has_value());
        CHECK(h->get().current <= 0.0f);     // EnemyDeathSystem sweeps these next frame
    }
}

TEST_CASE("ProjectileHitSystem damages the enemy it overlaps and expires", "[Game][arena][hit]") {
    EntityManager em; ComponentStorage storage; Blackboard bb;
    Entity enemy = em.create_entity();
    storage.add_component<EnemyTag>(enemy, EnemyTag{});
    storage.add_component<Health>(enemy, Health{30.0f, 30.0f});

    Entity proj = em.create_entity();
    storage.add_component<ProjectileTag>(proj, ProjectileTag{});
    storage.add_component<ProjectileData>(proj, ProjectileData{NO_TARGET, 500.0f, 20.0f});
    storage.add_component<CollidedWith>(proj, CollidedWith{{enemy}});

    ProjectileHitSystem sys;
    sys.update(em, storage, bb);

    CHECK(storage.has_component<DestroyRequest>(proj));      // projectile consumed
    CHECK(storage.has_component<Flash>(enemy));              // v2: struck enemy flashes
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

// v2 Gameplay Phase 3: the shop.

static GameConfig shop_config() {
    GameConfig cfg;
    cfg.shop.price_growth = 2.0f;      // easy to check by eye: 50, 100, 200
    cfg.shop.shield_regen_delay = 3.0f;
    cfg.shop.upgrades = {
        {"Hull Plating", "hull", 50, 25.0f, 2},
        {"Shield Capacitor", "shield", 90, 30.0f, 5},
        {"Twin Barrel", "extra_shot", 220, 1.0f, 2},
    };
    return cfg;
}

TEST_CASE("Buying an upgrade charges an escalating price and applies the effect",
          "[Game][arena][shop]") {
    GameConfig cfg = shop_config();
    EntityManager em; ComponentStorage storage; Blackboard bb;

    Entity p = em.create_entity();
    storage.add_component<PlayerTag>(p, PlayerTag{});
    storage.add_component<Health>(p, Health{60.0f, 100.0f});
    storage.add_component<ShipState>(p, ShipState{});
    storage.get_component<ShipState>(p)->get().currency = 200;

    ShopSystem shop;
    shop.set_config(&cfg.shop);
    shop.open(storage, em, bb);
    REQUIRE(shop.is_open());

    CHECK(shop.price_for(0, 0) == 50);
    CHECK(shop.price_for(0, 1) == 100);

    shop.update(storage, bb, 1, false);                 // buy Hull Plating
    const ShipState& ship = storage.get_component<ShipState>(p)->get();
    const Health& hp = storage.get_component<Health>(p)->get();
    CHECK(ship.currency == 150);
    CHECK(ship.upg_counts[0] == 1);
    CHECK_THAT(hp.max_hp, WithinAbs(125.0f, 1e-4));
    CHECK_THAT(hp.current, WithinAbs(85.0f, 1e-4));      // the purchase repairs too

    shop.update(storage, bb, 1, false);                 // second one costs 100
    CHECK(ship.currency == 50);
    CHECK(ship.upg_counts[0] == 2);

    shop.update(storage, bb, 1, false);                 // max_stacks 2: refused, free
    CHECK(ship.currency == 50);
    CHECK(ship.upg_counts[0] == 2);

    shop.update(storage, bb, 3, false);                 // Twin Barrel costs 220: refused
    CHECK(ship.currency == 50);
    CHECK(bb.get_or<int>("ship.extra_shots", 0) == 0);

    // A shield purchase arrives charged, and the leave key ends the phase.
    storage.get_component<ShipState>(p)->get().currency = 90;
    shop.update(storage, bb, 2, false);
    CHECK_THAT(ship.shield, WithinAbs(30.0f, 1e-4));
    CHECK_THAT(ship.shield_max, WithinAbs(30.0f, 1e-4));
    CHECK(ship.shield_regen > 0.0f);
    CHECK(shop.update(storage, bb, 0, true));
}

TEST_CASE("Shields soak damage before hull and regen only after a quiet delay",
          "[Game][arena][shop]") {
    EntityManager em; ComponentStorage storage; Blackboard bb;
    bb.set<double>("delta_time", 0.1);
    bb.set<float>("player.invuln_window", 0.0f);        // hit every frame
    bb.set<float>("ship.shield_regen_delay", 3.0f);

    Entity enemy = em.create_entity();
    storage.add_component<EnemyTag>(enemy, EnemyTag{});
    storage.add_component<ContactDamage>(enemy, ContactDamage{12.0f, 10, 1});

    Entity p = em.create_entity();
    storage.add_component<PlayerTag>(p, PlayerTag{});
    storage.add_component<Health>(p, Health{100.0f, 100.0f});
    storage.add_component<CollidedWith>(p, CollidedWith{{enemy}});
    storage.add_component<ShipState>(p, ShipState{});
    ShipState& ship = storage.get_component<ShipState>(p)->get();
    ship.shield = ship.shield_max = 30.0f;
    ship.shield_regen = 6.0f;

    PlayerDamageSystem dmg;
    dmg.update(em, storage, bb);                        // 12 of 30 shield gone
    CHECK_THAT(ship.shield, WithinAbs(18.0f, 1e-4));
    CHECK(storage.entities_with_component<DamageEvent>().empty());   // hull untouched
    CHECK_THAT(ship.shield_delay, WithinAbs(3.0f, 1e-4));

    // Still inside the delay: no regen.
    tick_shields(storage, 0.1f);
    CHECK_THAT(ship.shield, WithinAbs(18.0f, 1e-4));

    // Past it: refills at shield_regen per second, capped at shield_max.
    ship.shield_delay = 0.0f;
    tick_shields(storage, 1.0f);
    CHECK_THAT(ship.shield, WithinAbs(24.0f, 1e-4));
    tick_shields(storage, 10.0f);
    CHECK_THAT(ship.shield, WithinAbs(30.0f, 1e-4));

    // Overflow damage past an empty shield reaches the hull.
    ship.shield = 5.0f;
    dmg.update(em, storage, bb);
    CHECK_THAT(ship.shield, WithinAbs(0.0f, 1e-4));
    auto events = storage.entities_with_component<DamageEvent>();
    REQUIRE(events.size() == 1);
    CHECK_THAT(storage.get_component<DamageEvent>(events[0])->get().amount, WithinAbs(7.0f, 1e-4));
}

// v2 Gameplay Phase 4: items, consumables, and the one timed buff.

static GameConfig gear_config() {
    GameConfig cfg;
    cfg.shop.repulsor_radius = 100.0f;
    cfg.shop.upgrades = { {"Hull Plating", "hull", 50, 25.0f, 2} };
    cfg.shop.items = {
        {"Magnet Core",      "magnet",   120, 0.0f,  0},
        {"Repulsor Field",   "repulsor", 120, 40.0f, 0},
        {"Reactive Plating", "reactive", 120, 25.0f, 0},
        {"Salvager",         "salvage",  120, 1.5f,  0},
    };
    cfg.shop.consumables = {
        {"Repair Kit",  "repair",      45, 60.0f, 0},
        {"Overdrive",   "overdrive",   45, 2.0f,  0, 8.0f},
        {"EMP Burst",   "emp",         45, 45.0f, 0},
        {"Phase Shift", "phase_shift", 45, 0.0f,  0, 3.0f},
    };
    return cfg;
}

TEST_CASE("The gear page equips one item and one consumable, replacing the slot",
          "[Game][arena][items]") {
    GameConfig cfg = gear_config();
    EntityManager em; ComponentStorage storage; Blackboard bb;
    bb.set<int>("window_width", 980); bb.set<int>("window_height", 660);

    Entity p = em.create_entity();
    storage.add_component<PlayerTag>(p, PlayerTag{});
    storage.add_component<ShipState>(p, ShipState{});
    ShipState& ship = storage.get_component<ShipState>(p)->get();
    ship.currency = 500;

    ShopSystem shop;
    shop.set_config(&cfg.shop);
    shop.open(storage, em, bb);

    // Page 0 is upgrades: digit 1 must not equip anything.
    shop.update(storage, bb, 1, false);
    CHECK(ship.item_id == -1);
    CHECK(ship.upg_counts[0] == 1);

    shop.update(storage, bb, 0, false, /*toggle_page=*/true);   // -> gear page
    shop.update(storage, bb, 4, false);                          // row 4 = Salvager
    CHECK(ship.item_id == item_ids::SALVAGER);
    CHECK_THAT(bb.get_or<float>("ship.item_amount", 0.0f), WithinAbs(1.5f, 1e-4));
    CHECK(ship.currency == 500 - 50 - 120);

    // Rows 5-8 are the consumables; equipping an item does not touch that slot.
    shop.update(storage, bb, 6, false);                          // row 6 = Overdrive
    CHECK(ship.item_id == item_ids::SALVAGER);
    CHECK(ship.consumable_id == consumable_ids::OVERDRIVE);
    CHECK(ship.currency == 500 - 50 - 120 - 45);

    // One slot: a second item replaces the first, with no refund.
    shop.update(storage, bb, 1, false);                          // row 1 = Magnet Core
    CHECK(ship.item_id == item_ids::MAGNET_CORE);
    CHECK(ship.item_id == PickupSystem::ITEM_MAGNET_CORE);       // the id PickupSystem gates on
    CHECK(ship.currency == 500 - 50 - 120 - 45 - 120);

    // Broke: the slot keeps what it had.
    ship.currency = 10;
    shop.update(storage, bb, 2, false);
    CHECK(ship.item_id == item_ids::MAGNET_CORE);
    CHECK(ship.currency == 10);
}

TEST_CASE("Magnet Core pulls loot in and Salvager pays more for it",
          "[Game][arena][items]") {
    EntityManager em; ComponentStorage storage; Blackboard bb;
    bb.set<double>("delta_time", 0.1);

    Entity p = em.create_entity();
    storage.add_component<PlayerTag>(p, PlayerTag{});
    storage.add_component<Position>(p, Position{100.0f, 100.0f});
    storage.add_component<Size>(p, Size{40.0f, 40.0f});     // centre (120,120), r=20
    storage.add_component<ShipState>(p, ShipState{});
    ShipState& ship = storage.get_component<ShipState>(p)->get();

    Entity loot = em.create_entity();
    storage.add_component<Position>(loot, Position{212.0f, 112.0f});   // centre (220,120)
    storage.add_component<Size>(loot, Size{16.0f, 16.0f});
    storage.add_component<Pickup>(loot, Pickup{static_cast<int>(PickupKind::Currency), 1, 300.0f});

    PickupSystem sys;                       // default economy: magnet radius 220
    sys.update(storage, em, bb);
    CHECK_THAT(storage.get_component<Position>(loot)->get().x, WithinAbs(212.0f, 1e-4));

    // Equipped: 300 px/s for 0.1 s = 30 px closer, and never past the ship.
    ship.item_id = item_ids::MAGNET_CORE;
    sys.update(storage, em, bb);
    CHECK_THAT(storage.get_component<Position>(loot)->get().x, WithinAbs(182.0f, 1e-3));
    CHECK_FALSE(storage.has_component<DestroyRequest>(loot));
    for (int i = 0; i < 10 && !storage.has_component<DestroyRequest>(loot); ++i)
        sys.update(storage, em, bb);
    CHECK(storage.has_component<DestroyRequest>(loot));
    CHECK(ship.currency == 1);              // no Salvager: face value

    // Salvager: x1.5 with a floor of +1, so the 1-credit common drop still gains.
    ship.item_id = item_ids::SALVAGER;
    bb.set<float>("ship.item_amount", 1.5f);
    auto drop = [&](int value) {
        Entity e = em.create_entity();
        storage.add_component<Position>(e, Position{115.0f, 115.0f});
        storage.add_component<Size>(e, Size{16.0f, 16.0f});
        storage.add_component<Pickup>(e, Pickup{static_cast<int>(PickupKind::Currency), value, 0.0f});
    };
    drop(1); drop(4);
    sys.update(storage, em, bb);
    CHECK(ship.currency == 1 + 2 + 6);
}

TEST_CASE("Repulsor Field shoves enemies to the rim and no further",
          "[Game][arena][items]") {
    EntityManager em; ComponentStorage storage; Blackboard bb;

    Entity p = em.create_entity();
    storage.add_component<PlayerTag>(p, PlayerTag{});
    storage.add_component<Position>(p, Position{100.0f, 100.0f});
    storage.add_component<Size>(p, Size{40.0f, 40.0f});      // centre (120,120)
    storage.add_component<ShipState>(p, ShipState{});
    ShipState& ship = storage.get_component<ShipState>(p)->get();

    Entity e = em.create_entity();
    storage.add_component<EnemyTag>(e, EnemyTag{});
    storage.add_component<Position>(e, Position{140.0f, 88.0f});   // centre (172,120): 52 away
    storage.add_component<Size>(e, Size{64.0f, 64.0f});

    bb.set<float>("ship.item_amount", 40.0f);
    items::repulse_enemies(storage, bb, 100.0f, 0.5f);             // no item equipped
    CHECK_THAT(storage.get_component<Position>(e)->get().x, WithinAbs(140.0f, 1e-4));

    ship.item_id = item_ids::REPULSOR_FIELD;
    items::repulse_enemies(storage, bb, 100.0f, 0.5f);             // 20 px outward
    CHECK_THAT(storage.get_component<Position>(e)->get().x, WithinAbs(160.0f, 1e-3));

    // Clamped at the rim: many seconds of push never exceed the radius.
    for (int i = 0; i < 20; ++i) items::repulse_enemies(storage, bb, 100.0f, 0.5f);
    CHECK_THAT(storage.get_component<Position>(e)->get().x, WithinAbs(188.0f, 1e-3));
}

TEST_CASE("Reactive Plating hits the attacker back, shield or no shield",
          "[Game][arena][items]") {
    EntityManager em; ComponentStorage storage; Blackboard bb;
    bb.set<double>("delta_time", 0.016);
    bb.set<float>("player.invuln_window", 0.8f);
    bb.set<float>("ship.item_amount", 25.0f);

    Entity enemy = em.create_entity();
    storage.add_component<EnemyTag>(enemy, EnemyTag{});
    storage.add_component<Health>(enemy, Health{20.0f, 20.0f});
    storage.add_component<ContactDamage>(enemy, ContactDamage{12.0f, 10, 1});

    Entity p = em.create_entity();
    storage.add_component<PlayerTag>(p, PlayerTag{});
    storage.add_component<Health>(p, Health{100.0f, 100.0f});
    storage.add_component<CollidedWith>(p, CollidedWith{{enemy}});
    storage.add_component<ShipState>(p, ShipState{});
    ShipState& ship = storage.get_component<ShipState>(p)->get();
    ship.item_id = item_ids::REACTIVE_PLATING;
    ship.shield = ship.shield_max = 50.0f;    // the hit is fully absorbed

    PlayerDamageSystem dmg;
    dmg.update(em, storage, bb);

    auto events = storage.entities_with_component<DamageEvent>();
    REQUIRE(events.size() == 1);              // no hull event: only the reflect
    CHECK(storage.get_component<DamageEvent>(events[0])->get().target_entity == enemy);
    CHECK_THAT(storage.get_component<DamageEvent>(events[0])->get().amount, WithinAbs(25.0f, 1e-4));
}

TEST_CASE("Consumables fire once and Overdrive expires on its own",
          "[Game][arena][consumables]") {
    GameConfig cfg = gear_config();
    EntityManager em; ComponentStorage storage; Blackboard bb;

    Entity p = em.create_entity();
    storage.add_component<PlayerTag>(p, PlayerTag{});
    storage.add_component<Health>(p, Health{40.0f, 100.0f});
    storage.add_component<ShipState>(p, ShipState{});
    ShipState& ship = storage.get_component<ShipState>(p)->get();

    // Empty slot: nothing happens.
    CHECK_FALSE(items::use_consumable(storage, em, bb, cfg.shop));

    ship.consumable_id = consumable_ids::REPAIR_KIT;
    CHECK(items::use_consumable(storage, em, bb, cfg.shop));
    CHECK_THAT(storage.get_component<Health>(p)->get().current, WithinAbs(100.0f, 1e-4));  // capped
    CHECK(ship.consumable_id == -1);          // one use, then the slot is empty
    CHECK_FALSE(items::use_consumable(storage, em, bb, cfg.shop));

    ship.consumable_id = consumable_ids::OVERDRIVE;
    CHECK(items::use_consumable(storage, em, bb, cfg.shop));
    CHECK(ship.buff_id == consumable_ids::OVERDRIVE);
    CHECK_THAT(ship.buff_timer, WithinAbs(8.0f, 1e-4));
    CHECK_THAT(bb.get_or<float>("ship.buff_mult", 0.0f), WithinAbs(2.0f, 1e-4));
    tick_buff(storage, 7.0f);
    CHECK(ship.buff_id == consumable_ids::OVERDRIVE);
    tick_buff(storage, 2.0f);
    CHECK(ship.buff_id == -1);                // expired; no fire_rate to restore (D34)
    CHECK_THAT(ship.buff_timer, WithinAbs(0.0f, 1e-4));

    ship.consumable_id = consumable_ids::PHASE_SHIFT;
    CHECK(items::use_consumable(storage, em, bb, cfg.shop));
    CHECK_THAT(bb.get_or<float>("player.iframes", 0.0f), WithinAbs(3.0f, 1e-4));

    // EMP damages every living enemy, once each.
    for (int i = 0; i < 3; ++i) {
        Entity e = em.create_entity();
        storage.add_component<EnemyTag>(e, EnemyTag{});
        storage.add_component<Health>(e, Health{i == 2 ? 0.0f : 30.0f, 30.0f});  // one corpse
    }
    ship.consumable_id = consumable_ids::EMP_BURST;
    CHECK(items::use_consumable(storage, em, bb, cfg.shop));
    auto events = storage.entities_with_component<DamageEvent>();
    REQUIRE(events.size() == 2);              // the dead one is skipped
    CHECK_THAT(storage.get_component<DamageEvent>(events[0])->get().amount, WithinAbs(45.0f, 1e-4));
}
