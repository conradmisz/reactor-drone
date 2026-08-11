/**
 * test_lane_n.cpp — iteration 5, Lane N: controls & bombs (D120-D123).
 *
 * What fails silently without these:
 *   - the dash cooldown quietly staying at the old 2.5s after the rebind;
 *   - a mine that a shot passes straight through (the "destroyable" that isn't),
 *     or one that pops before it has armed;
 *   - a blast that is still the old 150px box;
 *   - an upgrade look whose tier 0 does NOT match the emitter main.cpp spawns,
 *     which would change a fresh drone's exhaust on its first playing frame.
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/project_paths.hpp"
#include "game/arena_config.hpp"
#include "game/collision_layers.hpp"
#include "game/enemy_components.hpp"
#include "game/player_components.hpp"
#include "game/dash_system.hpp"
#include "game/player_fire_system.hpp"
#include "game/specialty_system.hpp"
#include "game/tower_components.hpp"
#include "game/upgrade_visuals.hpp"

#include <string>

using Catch::Matchers::WithinAbs;

namespace {

/// A tier-0 mine, exactly as SpecialtySystem deploys one.
Entity deploy_mine(EntityManager& em, ComponentStorage& cs, float x, float y,
                   float arm = specialty::MINE_ARM_DELAY) {
    Entity m = em.create_entity();
    cs.add_component<Position>(m, Position{x, y});
    cs.add_component<Size>(m, Size{specialty::MINE_SIZE, specialty::MINE_SIZE});
    cs.add_component<EnemyBehavior>(m,
        EnemyBehavior{behavior_kinds::MINER, 0, arm, 22.0f, specialty::MINE_TRIGGER});
    return m;
}

Entity shot(EntityManager& em, ComponentStorage& cs, float x, float y) {
    Entity s = em.create_entity();
    cs.add_component<Position>(s, Position{x, y});
    cs.add_component<Size>(s, Size{8.0f, 8.0f});
    cs.add_component<ProjectileTag>(s, ProjectileTag{});
    return s;
}

size_t hazards(const ComponentStorage& cs) {
    size_t n = 0;
    for (Entity e : cs.entities_with_component<ContactDamage>()) {
        auto c = cs.get_component<Collider>(e);
        if (c.has_value() && c->get().layer == layers::HAZARD) ++n;
    }
    return n;
}

}  // namespace

TEST_CASE("the dash is on a 10s cooldown", "[Game][laneN][dash]") {
    GameConfig cfg = load_arena_config(project_paths::assets_dir() + "/GameData.json");
    CHECK_THAT(cfg.dash.cooldown, WithinAbs(10.0f, 1e-5f));
    // Still a short burst — the rebind moved the key and the wait, not the dash.
    CHECK_THAT(cfg.dash.duration, WithinAbs(0.15f, 1e-5f));
}

TEST_CASE("an armed mine detonates when it is shot, and eats the shot",
          "[Game][laneN][specialty]") {
    EntityManager em; ComponentStorage cs; Blackboard bb;
    bb.set<double>("delta_time", 1.0);
    SpecialtySystem sys;
    GameConfig cfg; sys.set_config(&cfg);

    Entity mine = deploy_mine(em, cs, 500.0f, 500.0f);
    Entity bullet = shot(em, cs, 500.0f + specialty::MINE_SIZE * 0.5f, 500.0f);

    // Frame 1: still arming. A shot on top of it must do nothing at all.
    sys.update(cs, em, bb);
    CHECK_FALSE(cs.has_component<DestroyRequest>(mine));
    CHECK_FALSE(cs.has_component<DestroyRequest>(bullet));
    CHECK(hazards(cs) == 0);

    // Frame 2: armed. No player in the world, so proximity cannot be what fires it.
    sys.update(cs, em, bb);
    CHECK(cs.has_component<DestroyRequest>(mine));
    CHECK(cs.has_component<DestroyRequest>(bullet));
    CHECK(hazards(cs) == 1);
}

TEST_CASE("a shot that misses leaves the mine armed", "[Game][laneN][specialty]") {
    EntityManager em; ComponentStorage cs; Blackboard bb;
    bb.set<double>("delta_time", 1.0);
    SpecialtySystem sys;
    GameConfig cfg; sys.set_config(&cfg);

    Entity mine = deploy_mine(em, cs, 500.0f, 500.0f);
    shot(em, cs, 900.0f, 900.0f);
    sys.update(cs, em, bb);
    sys.update(cs, em, bb);
    CHECK_FALSE(cs.has_component<DestroyRequest>(mine));
    CHECK(hazards(cs) == 0);
}

TEST_CASE("the mine blast reaches less far than it used to", "[Game][laneN][specialty]") {
    // 150 -> 100 (D121): a box, so the reach from the mine's centre is the half.
    CHECK_THAT(specialty::MINE_BLAST_SIZE, WithinAbs(100.0f, 1e-5f));
    CHECK(specialty::MINE_BLAST_SIZE * 0.5f < specialty::MINE_TRIGGER);
}

TEST_CASE("the upgrade look ramps with purchases and starts at the stock plume",
          "[Game][laneN][upgrades]") {
    ShipState s;
    CHECK(upgrade_visuals::tier(s) == 0);

    // Tier 0 must be exactly the emitter main.cpp spawns, or a fresh drone would
    // change look on frame one.
    const upgrade_visuals::Look base = upgrade_visuals::look_for(0);
    CHECK(base.start_r == 90);
    CHECK(base.start_g == 220);
    CHECK(base.start_b == 255);
    CHECK(base.end_r == 30);
    CHECK(base.end_g == 80);
    CHECK(base.end_b == 160);
    CHECK_THAT(base.emission_rate, WithinAbs(34.0f, 1e-4f));
    CHECK_THAT(base.start_size, WithinAbs(5.0f, 1e-4f));
    CHECK_THAT(base.particle_lifetime, WithinAbs(0.4f, 1e-4f));

    // Purchases spread over rows still count as purchases.
    s.upg_counts[0] = 2; s.upg_counts[3] = 1;
    CHECK(upgrade_visuals::tier(s) == 1);
    for (int i = 0; i < 8; ++i) s.upg_counts[i] = 9;
    CHECK(upgrade_visuals::tier(s) == upgrade_visuals::MAX_TIER);   // capped

    // Monotone hotter/bigger/denser, and the top tier stays inside the budget
    // ENGINE.md §5 cares about.
    float rate = -1.0f, size = -1.0f;
    for (int t = 0; t <= upgrade_visuals::MAX_TIER; ++t) {
        const upgrade_visuals::Look l = upgrade_visuals::look_for(t);
        CHECK(l.emission_rate > rate);
        CHECK(l.start_size > size);
        rate = l.emission_rate; size = l.start_size;
    }
    CHECK(rate * upgrade_visuals::look_for(upgrade_visuals::MAX_TIER).particle_lifetime < 100.0f);
}

TEST_CASE("the upgrade look leaves the emission rate alone mid-dash",
          "[Game][laneN][upgrades]") {
    EntityManager em; ComponentStorage cs;
    Entity p = em.create_entity();
    ParticleEmitter thruster;
    thruster.emission_rate = 240.0f;      // DASH_EMISSION_RATE: a burst is running
    cs.add_component<ParticleEmitter>(p, thruster);

    ShipState s;
    s.upg_counts[0] = 12;                 // top tier
    s.dash_timer = 0.1f;
    upgrade_visuals::apply_to_player(cs, p, s);
    auto em_c = cs.get_component<ParticleEmitter>(p);
    REQUIRE(em_c.has_value());
    CHECK_THAT(em_c->get().emission_rate, WithinAbs(240.0f, 1e-4f));   // dash keeps it
    CHECK(em_c->get().start_r == 255);                                 // colour still ramps

    s.dash_timer = 0.0f;
    upgrade_visuals::apply_to_player(cs, p, s);
    CHECK_THAT(em_c->get().emission_rate,
               WithinAbs(upgrade_visuals::look_for(upgrade_visuals::MAX_TIER).emission_rate,
                         1e-4f));
}

// ---------------------------------------------------------------------------
// D192 — the playtest batch. Three branches that fail silently otherwise: a
// blast that only hurts the drone (#4), a dash stack that never refills or that
// refills for free (#10), and a battery that never locks out (#9).
// ---------------------------------------------------------------------------

TEST_CASE("a mine blast damages enemies standing in it (#4)",
          "[Game][d192][specialty]") {
    EntityManager em; ComponentStorage cs; Blackboard bb;
    bb.set<double>("delta_time", 1.0);
    SpecialtySystem sys;
    GameConfig cfg; sys.set_config(&cfg);

    Entity mine = deploy_mine(em, cs, 500.0f, 500.0f);
    shot(em, cs, 500.0f + specialty::MINE_SIZE * 0.5f, 500.0f);

    // Inside the blast radius, and one well outside it.
    auto enemy = [&](float x, float y) {
        Entity e = em.create_entity();
        cs.add_component<Position>(e, Position{x, y});
        cs.add_component<Size>(e, Size{20.0f, 20.0f});
        cs.add_component<EnemyTag>(e, EnemyTag{});
        return e;
    };
    const Entity near = enemy(510.0f, 505.0f);
    const Entity far = enemy(1500.0f, 1500.0f);

    sys.update(cs, em, bb);   // arming
    sys.update(cs, em, bb);   // detonation
    CHECK(cs.has_component<DestroyRequest>(mine));

    int hit_near = 0, hit_far = 0;
    for (Entity e : cs.entities_with_component<DamageEvent>()) {
        auto d = cs.get_component<DamageEvent>(e);
        if (!d.has_value()) continue;
        if (d->get().target_entity == near) ++hit_near;
        if (d->get().target_entity == far) ++hit_far;
    }
    CHECK(hit_near == 1);
    CHECK(hit_far == 0);
}

TEST_CASE("dash charges spend and refill one at a time (#10)",
          "[Game][d192][dash]") {
    EntityManager em; ComponentStorage cs; Blackboard bb;
    DashConfig cfg;               // duration 0.15, cooldown 2.5 by default
    DashState state;

    Entity p = em.create_entity();
    cs.add_component<PlayerTag>(p, PlayerTag{});
    cs.add_component<Position>(p, Position{0.0f, 0.0f});
    cs.add_component<Size>(p, Size{40.0f, 40.0f});
    cs.add_component<Velocity>(p, Velocity{100.0f, 0.0f});
    ShipState st{};
    st.dash_max = 2;              // one boss already killed
    st.dash_charges = 2;
    cs.add_component<ShipState>(p, st);
    auto ship = [&]() -> ShipState& { return cs.get_component<ShipState>(p)->get(); };

    // Two bursts back to back: both must fire, because the stack holds two.
    tick_dash(cs, em, bb, cfg, state, true, 0.016f);
    CHECK(ship().dash_charges == 1);
    while (ship().dash_timer > 0.0f)
        tick_dash(cs, em, bb, cfg, state, false, 0.016f);
    tick_dash(cs, em, bb, cfg, state, true, 0.016f);
    CHECK(ship().dash_charges == 0);

    // A third is refused, and the stack comes back one charge per cooldown —
    // never past dash_max.
    tick_dash(cs, em, bb, cfg, state, true, 0.016f);
    CHECK(ship().dash_charges == 0);
    for (int i = 0; i < 160; ++i) tick_dash(cs, em, bb, cfg, state, false, 0.016f);  // one cooldown (2.5s)
    CHECK(ship().dash_charges == 1);
    for (int i = 0; i < 160; ++i) tick_dash(cs, em, bb, cfg, state, false, 0.016f);  // one cooldown (2.5s)
    CHECK(ship().dash_charges == 2);
    for (int i = 0; i < 160; ++i) tick_dash(cs, em, bb, cfg, state, false, 0.016f);  // one cooldown (2.5s)
    CHECK(ship().dash_charges == 2);
}

TEST_CASE("the battery empties, locks the trigger, and only unlocks at full (#9)",
          "[Game][d192][battery]") {
    EntityManager em; ComponentStorage cs; Blackboard bb;
    bb.set<double>("delta_time", 0.1);
    bb.set<bool>("mouse.held", true);
    bb.set<float>("battery.drain_per_s", 1.0f / 12.0f);
    bb.set<float>("battery.charge_per_s", 1.0f / 3.0f);

    Entity p = em.create_entity();
    cs.add_component<PlayerTag>(p, PlayerTag{});
    cs.add_component<Position>(p, Position{0.0f, 0.0f});
    cs.add_component<Size>(p, Size{40.0f, 40.0f});
    cs.add_component<Rotation>(p, Rotation{0.0f, 0.0f, false});
    cs.add_component<WeaponStats>(p, WeaponStats{});
    cs.add_component<ShipState>(p, ShipState{});
    auto ship = [&]() -> ShipState& { return cs.get_component<ShipState>(p)->get(); };

    PlayerFireSystem fire(7u);
    auto shots = [&]() {
        size_t n = 0;
        for (Entity e : cs.entities_with_component<ProjectileTag>()) { (void)e; ++n; }
        return n;
    };

    // ~12 s of holding empties it (float accumulation can cost a tick either
    // way, so this counts ticks rather than asserting an exact frame).
    int ticks = 0;
    while (!ship().battery_locked && ticks < 300) { fire.update(cs, em, bb); ++ticks; }
    CHECK(ship().battery_locked);
    CHECK(ticks >= 118);
    CHECK(ticks <= 122);
    CHECK_THAT(ship().battery, WithinAbs(0.0f, 1e-3f));

    // Locked out: the trigger is still held and nothing comes out.
    const size_t fired = shots();
    for (int i = 0; i < 20; ++i) fire.update(cs, em, bb);
    CHECK(shots() == fired);

    // It recharges under the held trigger and fires again only once full —
    // 3 s from empty, so ~10 more ticks after the 20 above.
    int back = 20;
    while (ship().battery_locked && back < 100) { fire.update(cs, em, bb); ++back; }
    CHECK_FALSE(ship().battery_locked);
    CHECK(back >= 28);
    CHECK(back <= 32);
    fire.update(cs, em, bb);
    CHECK(shots() > fired);
}
