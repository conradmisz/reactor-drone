/**
 * test_sustain_spawn.cpp — periodic health & shield pickups (#10, D56).
 *
 * Three things can break here and none of them is loud:
 *   - the collection branch crediting the wrong pool, or overhealing past max;
 *   - the cadence drifting, which would only show up as a replay divergence
 *     thousands of frames later;
 *   - the live cap leaking, which turns the arena into a carpet of green scrap.
 *
 * This file also takes over the two "inert" assertions test_scaffolding.cpp used
 * to make about the `sustain` block (D51 says the landing lane deletes them).
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <vector>

#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/destruction.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/project_paths.hpp"
#include "game/arena_config.hpp"
#include "game/enemy_components.hpp"
#include "game/pickup_system.hpp"
#include "game/player_components.hpp"
#include "game/sustain_spawn_system.hpp"

using Catch::Matchers::WithinAbs;

namespace {

ArenaConfig test_arena() {
    ArenaConfig a;
    a.center_x = 480.0f;
    a.center_y = 330.0f;
    a.radius = 320.0f;
    return a;
}

SustainConfig test_sustain() {
    SustainConfig s;
    s.interval = 2.0f;
    s.max_live = 3;
    s.health_amount = 25.0f;
    s.shield_amount = 20.0f;
    s.shield_weight = 0.5f;
    s.min_player_dist = 220.0f;
    return s;
}

/// A world with a drone parked at the arena centre. Returns the player entity.
Entity make_world(ComponentStorage& cs, EntityManager& em, Blackboard& bb,
                  const ArenaConfig& arena, float shield_max = 0.0f) {
    bb.set<double>("delta_time", 1.0 / 60.0);
    bb.set<int>("wave", 1);
    Entity p = em.create_entity();
    cs.add_component<Position>(p, Position{arena.center_x - 20.0f, arena.center_y - 20.0f});
    cs.add_component<Size>(p, Size{40.0f, 40.0f});
    cs.add_component<PlayerTag>(p, PlayerTag{});
    cs.add_component<Health>(p, Health{50.0f, 100.0f, 1.0f});
    ShipState s;
    s.shield_max = shield_max;
    cs.add_component<ShipState>(p, s);
    return p;
}

int live_pickups(const ComponentStorage& cs) {
    return static_cast<int>(cs.entities_with_component<Pickup>().size());
}

/// Run `frames` frames of the spawner at 60Hz and return the placements made.
int run_frames(ComponentStorage& cs, EntityManager& em, Blackboard& bb,
               const SustainConfig& cfg, const ArenaConfig& arena, int frames) {
    EconomyConfig eco;
    const int before = live_pickups(cs);
    for (int i = 0; i < frames; ++i) {
        sustain_spawn(cs, em, bb, cfg, arena, eco);
    }
    return live_pickups(cs) - before;
}

}  // namespace

// --------------------------------------------------------------------------
// Kind routing and clamping (PickupSystem's two new branches)
// --------------------------------------------------------------------------

TEST_CASE("a health pickup repairs the hull and never overheals",
          "[Game][sustain][pickup]") {
    ComponentStorage cs;
    EntityManager em;
    Blackboard bb;
    const ArenaConfig arena = test_arena();
    Entity p = make_world(cs, em, bb, arena);

    auto drop = [&](PickupKind kind, int value) {
        Entity e = em.create_entity();
        cs.add_component<Position>(e, Position{arena.center_x - 6.0f, arena.center_y - 6.0f});
        cs.add_component<Size>(e, Size{12.0f, 12.0f});
        cs.add_component<Pickup>(e, Pickup{static_cast<int>(kind), value, 0.0f});
        return e;
    };

    PickupSystem pickups;
    pickups.set_economy(EconomyConfig{});

    drop(PickupKind::Health, 25);
    pickups.update(cs, em, bb);
    CHECK_THAT(cs.get_component<Health>(p)->get().current, WithinAbs(75.0f, 1e-4f));
    destroy_marked_entities(em, cs);

    // Overheal: 75 + 999 clamps to max_hp, and credits are untouched — a health
    // pickup must not fall through to the currency branch.
    drop(PickupKind::Health, 999);
    pickups.update(cs, em, bb);
    CHECK_THAT(cs.get_component<Health>(p)->get().current, WithinAbs(100.0f, 1e-4f));
    CHECK(cs.get_component<ShipState>(p)->get().currency == 0);
}

TEST_CASE("a shield pickup fills the bank and is worthless without one",
          "[Game][sustain][pickup]") {
    ComponentStorage cs;
    EntityManager em;
    Blackboard bb;
    const ArenaConfig arena = test_arena();
    Entity p = make_world(cs, em, bb, arena, /*shield_max=*/0.0f);

    PickupSystem pickups;
    pickups.set_economy(EconomyConfig{});

    auto drop_shield = [&](int value) {
        Entity e = em.create_entity();
        cs.add_component<Position>(e, Position{arena.center_x - 6.0f, arena.center_y - 6.0f});
        cs.add_component<Size>(e, Size{12.0f, 12.0f});
        cs.add_component<Pickup>(e, Pickup{static_cast<int>(PickupKind::Shield), value, 0.0f});
    };

    // No capacitor bought yet -> clamps to 0, and does NOT become currency.
    drop_shield(20);
    pickups.update(cs, em, bb);
    CHECK_THAT(cs.get_component<ShipState>(p)->get().shield, WithinAbs(0.0f, 1e-4f));
    CHECK(cs.get_component<ShipState>(p)->get().currency == 0);
    destroy_marked_entities(em, cs);

    cs.get_component<ShipState>(p)->get().shield_max = 30.0f;
    drop_shield(20);
    pickups.update(cs, em, bb);
    CHECK_THAT(cs.get_component<ShipState>(p)->get().shield, WithinAbs(20.0f, 1e-4f));
    destroy_marked_entities(em, cs);

    // 20 + 20 clamps at shield_max, not 40.
    drop_shield(20);
    pickups.update(cs, em, bb);
    CHECK_THAT(cs.get_component<ShipState>(p)->get().shield, WithinAbs(30.0f, 1e-4f));
}

// --------------------------------------------------------------------------
// Cadence and determinism
// --------------------------------------------------------------------------

TEST_CASE("placements land on a fixed interval, not per frame",
          "[Game][sustain][cadence]") {
    ComponentStorage cs;
    EntityManager em;
    Blackboard bb;
    const ArenaConfig arena = test_arena();
    SustainConfig cfg = test_sustain();   // 2s interval at 60Hz = 120 frames
    make_world(cs, em, bb, arena);

    CHECK(run_frames(cs, em, bb, cfg, arena, 119) == 0);
    CHECK(run_frames(cs, em, bb, cfg, arena, 1) == 1);
    CHECK(run_frames(cs, em, bb, cfg, arena, 119) == 0);
    CHECK(run_frames(cs, em, bb, cfg, arena, 1) == 1);
}

TEST_CASE("interval 0 keeps the feature off entirely", "[Game][sustain][cadence]") {
    ComponentStorage cs;
    EntityManager em;
    Blackboard bb;
    const ArenaConfig arena = test_arena();
    SustainConfig cfg = test_sustain();
    cfg.interval = 0.0f;
    make_world(cs, em, bb, arena);
    CHECK(run_frames(cs, em, bb, cfg, arena, 2000) == 0);
}

TEST_CASE("two identical runs place identical pickups", "[Game][sustain][determinism]") {
    const ArenaConfig arena = test_arena();
    const SustainConfig cfg = test_sustain();

    auto run = [&]() {
        ComponentStorage cs;
        EntityManager em;
        Blackboard bb;
        make_world(cs, em, bb, arena);
        std::vector<float> out;
        for (int i = 0; i < 400; ++i) {
            sustain_spawn(cs, em, bb, cfg, arena, EconomyConfig{});
            for (Entity e : cs.entities_with_component<Pickup>()) {
                if (cs.has_component<DestroyRequest>(e)) continue;
                // Collect the whole world state a divergence could hide in.
                out.push_back(cs.get_component<Position>(e)->get().x);
                out.push_back(cs.get_component<Position>(e)->get().y);
                out.push_back(static_cast<float>(cs.get_component<Pickup>(e)->get().kind));
            }
        }
        return out;
    };

    const std::vector<float> a = run();
    const std::vector<float> b = run();
    REQUIRE(a.size() == b.size());
    REQUIRE_FALSE(a.empty());
    CHECK(a == b);
}

TEST_CASE("placement geometry is a pure function of the counter",
          "[Game][sustain][determinism]") {
    const ArenaConfig arena = test_arena();
    for (int n = 0; n < 200; ++n) {
        float x1, y1, x2, y2;
        sustain_placement_point(n, arena, x1, y1);
        sustain_placement_point(n, arena, x2, y2);
        CHECK(x1 == x2);
        CHECK(y1 == y2);
        // Always inside the arena circle.
        const float dx = x1 - arena.center_x, dy = y1 - arena.center_y;
        CHECK(std::sqrt(dx * dx + dy * dy) <= arena.radius);
    }
    // Consecutive placements are not stacked on top of each other.
    float ax, ay, bx, by;
    sustain_placement_point(3, arena, ax, ay);
    sustain_placement_point(4, arena, bx, by);
    CHECK(std::sqrt((ax - bx) * (ax - bx) + (ay - by) * (ay - by)) > 50.0f);
}

TEST_CASE("the kind split honours shield_weight exactly", "[Game][sustain][determinism]") {
    // 0.35 over 100 placements = 35 shields, no more and no less.
    int shields = 0;
    for (int n = 0; n < 100; ++n) if (sustain_is_shield(n, 0.35f)) ++shields;
    CHECK(shields == 35);

    // Degenerate weights do not spin or wrap.
    for (int n = 0; n < 10; ++n) {
        CHECK_FALSE(sustain_is_shield(n, 0.0f));
        CHECK(sustain_is_shield(n, 1.0f));
    }
}

// --------------------------------------------------------------------------
// The live cap
// --------------------------------------------------------------------------

TEST_CASE("no more than max_live sustain pickups exist at once",
          "[Game][sustain][cap]") {
    ComponentStorage cs;
    EntityManager em;
    Blackboard bb;
    const ArenaConfig arena = test_arena();
    SustainConfig cfg = test_sustain();
    cfg.max_live = 2;
    make_world(cs, em, bb, arena);

    // Twenty intervals' worth of frames with nothing collecting anything.
    run_frames(cs, em, bb, cfg, arena, 120 * 20);
    CHECK(live_pickups(cs) == 2);

    // Collecting one frees exactly one slot, and the next interval refills it —
    // it does not release a backlog of the nineteen that were skipped.
    Entity first = cs.entities_with_component<Pickup>().front();
    cs.add_component<DestroyRequest>(first, DestroyRequest{});
    destroy_marked_entities(em, cs);
    CHECK(live_pickups(cs) == 1);
    run_frames(cs, em, bb, cfg, arena, 120);
    CHECK(live_pickups(cs) == 2);
}

TEST_CASE("currency pickups do not count against the sustain cap",
          "[Game][sustain][cap]") {
    ComponentStorage cs;
    EntityManager em;
    Blackboard bb;
    const ArenaConfig arena = test_arena();
    SustainConfig cfg = test_sustain();
    cfg.max_live = 1;
    make_world(cs, em, bb, arena);

    // A pile of coins sitting on the far side of the arena.
    for (int i = 0; i < 10; ++i) {
        Entity e = em.create_entity();
        cs.add_component<Position>(e, Position{10.0f, 10.0f});
        cs.add_component<Size>(e, Size{12.0f, 12.0f});
        cs.add_component<Pickup>(e, Pickup{static_cast<int>(PickupKind::Currency), 1, 0.0f});
    }
    CHECK(run_frames(cs, em, bb, cfg, arena, 120) == 1);
}

TEST_CASE("placements keep clear of the drone", "[Game][sustain][cap]") {
    ComponentStorage cs;
    EntityManager em;
    Blackboard bb;
    const ArenaConfig arena = test_arena();
    SustainConfig cfg = test_sustain();
    cfg.max_live = 32;
    Entity p = make_world(cs, em, bb, arena);
    const float px = cs.get_component<Position>(p)->get().x + 20.0f;
    const float py = cs.get_component<Position>(p)->get().y + 20.0f;

    run_frames(cs, em, bb, cfg, arena, 120 * 30);
    REQUIRE(live_pickups(cs) > 5);
    for (Entity e : cs.entities_with_component<Pickup>()) {
        const auto& pos = cs.get_component<Position>(e)->get();
        const float half = cs.get_component<Size>(e)->get().width * 0.5f;
        const float dx = pos.x + half - px, dy = pos.y + half - py;
        CHECK(std::sqrt(dx * dx + dy * dy) >= cfg.min_player_dist);
    }
}

TEST_CASE("the shipped GameData turns sustain pickups on", "[Game][sustain][config]") {
    GameConfig cfg = load_arena_config(project_paths::assets_dir() + "/GameData.json");
    CHECK(cfg.sustain.interval > 0.0f);
    CHECK(cfg.sustain.max_live > 0);
}
