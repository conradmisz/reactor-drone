/**
 * Unit tests for loot placement (Lane K, D101).
 *
 * The user's note: money must not occupy the same space as obstacles, hazards,
 * enemy mines or other pickups. Two things are under test:
 *
 *  1. it actually works — a coin dropped onto a hazard ends up off it;
 *  2. it costs the RNG stream nothing. That is the load-bearing one. The whole
 *     project's replay determinism rests on `drop_loot` drawing a fixed number
 *     of values per kill (ENGINE.md §4), and a rejection loop is the classic way
 *     to break it. The last test kills two enemies in two identically-seeded
 *     worlds — one clear, one packed with blockers — and then kills a third in
 *     clean space in each: identical coin positions prove the search consumed no
 *     draws in either world.
 */
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/destruction.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "game/collision_layers.hpp"
#include "game/enemy_components.hpp"
#include "game/enemy_death_system.hpp"
#include "game/player_components.hpp"

namespace {

EconomyConfig test_economy() {
    EconomyConfig ec;
    ec.min_drops = 3;
    ec.max_drops = 3;             // every kill drops exactly three coins
    ec.key_drop_chance = 0.0f;
    ec.pickup_size = 16.0f;
    ec.pickup_scatter = 26.0f;
    return ec;
}

/// A dead enemy at (cx, cy) — Health 0 is what EnemyDeathSystem looks for.
Entity dead_enemy(EntityManager& em, ComponentStorage& cs, float cx, float cy) {
    Entity e = em.create_entity();
    cs.add_component<Position>(e, Position{cx - 20.0f, cy - 20.0f});
    cs.add_component<Size>(e, Size{40.0f, 40.0f});
    cs.add_component<Health>(e, Health{0.0f, 50.0f});
    cs.add_component<EnemyTag>(e, EnemyTag{});
    cs.add_component<ContactDamage>(e, ContactDamage{10.0f, 10, 1, 1.0f});
    return e;
}

Entity blocker(EntityManager& em, ComponentStorage& cs, float cx, float cy,
               float size, uint8_t layer) {
    Entity e = em.create_entity();
    cs.add_component<Position>(e, Position{cx - size * 0.5f, cy - size * 0.5f});
    cs.add_component<Size>(e, Size{size, size});
    cs.add_component<Collider>(e, Collider{size, size, layer, layers::HAZARD_MASK});
    return e;
}

Entity mine(EntityManager& em, ComponentStorage& cs, float cx, float cy, float size) {
    Entity e = em.create_entity();
    cs.add_component<Position>(e, Position{cx - size * 0.5f, cy - size * 0.5f});
    cs.add_component<Size>(e, Size{size, size});
    // D68: a deployed mine is tier 0 and carries no Collider at all.
    cs.add_component<EnemyBehavior>(e,
        EnemyBehavior{behavior_kinds::MINER, 0, 0.5f, 20.0f, 60.0f});
    return e;
}

std::vector<std::pair<float, float>> coin_centres(ComponentStorage& cs, float half) {
    std::vector<std::pair<float, float>> out;
    for (Entity e : cs.entities_with_component<Pickup>()) {
        auto p = cs.get_component<Position>(e);
        if (p.has_value()) out.emplace_back(p->get().x + half, p->get().y + half);
    }
    return out;
}

bool overlaps(float ax, float ay, float ah, float bx, float by, float bh) {
    return std::fabs(ax - bx) < (ah + bh) && std::fabs(ay - by) < (ah + bh);
}

}  // namespace

TEST_CASE("blocked() sees obstacles, hazards, mines and other loot", "[loot][placement]") {
    EntityManager em;
    ComponentStorage cs;
    const float half = 8.0f;

    CHECK_FALSE(loot_place::blocked(cs, 0.0f, 0.0f, half));

    blocker(em, cs, 100.0f, 100.0f, 60.0f, layers::OBSTACLE);
    blocker(em, cs, 300.0f, 100.0f, 60.0f, layers::HAZARD);
    mine(em, cs, 500.0f, 100.0f, 30.0f);
    Entity coin = em.create_entity();
    cs.add_component<Position>(coin, Position{700.0f - half, 100.0f - half});
    cs.add_component<Size>(coin, Size{16.0f, 16.0f});
    cs.add_component<Pickup>(coin, Pickup{});

    CHECK(loot_place::blocked(cs, 100.0f, 100.0f, half));   // obstacle
    CHECK(loot_place::blocked(cs, 300.0f, 100.0f, half));   // hazard
    CHECK(loot_place::blocked(cs, 500.0f, 100.0f, half));   // mine
    CHECK(loot_place::blocked(cs, 700.0f, 100.0f, half));   // another pickup
    CHECK_FALSE(loot_place::blocked(cs, 100.0f, 400.0f, half));

    // An enemy or a projectile is NOT a blocker: loot on top of a live enemy is
    // fine and self-resolving, and treating everything with a Collider as solid
    // would nudge every coin in a crowded arena.
    Entity enemy = em.create_entity();
    cs.add_component<Position>(enemy, Position{900.0f, 900.0f});
    cs.add_component<Size>(enemy, Size{40.0f, 40.0f});
    cs.add_component<Collider>(enemy, Collider{40.0f, 40.0f, layers::ENEMY, layers::ENEMY_MASK});
    CHECK_FALSE(loot_place::blocked(cs, 920.0f, 920.0f, half));
}

TEST_CASE("nudge_free leaves a free point exactly where it was", "[loot][placement]") {
    EntityManager em;
    ComponentStorage cs;
    blocker(em, cs, 100.0f, 100.0f, 60.0f, layers::HAZARD);

    float x = 500.0f, y = 500.0f;
    loot_place::nudge_free(cs, x, y, 8.0f, 40.0f);
    CHECK(x == 500.0f);
    CHECK(y == 500.0f);
}

TEST_CASE("nudge_free moves a blocked point off the blocker", "[loot][placement]") {
    EntityManager em;
    ComponentStorage cs;
    blocker(em, cs, 100.0f, 100.0f, 60.0f, layers::HAZARD);

    float x = 100.0f, y = 100.0f;
    loot_place::nudge_free(cs, x, y, 8.0f, 80.0f);
    CHECK_FALSE(loot_place::blocked(cs, x, y, 8.0f));
    // ... and not to the other side of the arena.
    CHECK(std::hypot(x - 100.0f, y - 100.0f) <= 80.0f + 1e-3f);
}

TEST_CASE("nudge_free is pure — same input, same output", "[loot][placement][determinism]") {
    EntityManager em;
    ComponentStorage cs;
    blocker(em, cs, 100.0f, 100.0f, 60.0f, layers::HAZARD);

    float x1 = 100.0f, y1 = 100.0f, x2 = 100.0f, y2 = 100.0f;
    loot_place::nudge_free(cs, x1, y1, 8.0f, 80.0f);
    loot_place::nudge_free(cs, x2, y2, 8.0f, 80.0f);
    CHECK(x1 == x2);
    CHECK(y1 == y2);
}

TEST_CASE("a dropped coin never lands on an obstacle, hazard, mine or coin",
          "[loot][placement]") {
    EntityManager em;
    ComponentStorage cs;
    Blackboard bb;
    EnemyDeathSystem deaths;
    deaths.set_economy(test_economy(), 1234u);
    const float half = test_economy().pickup_size * 0.5f;

    // A kill right in the middle of a poison patch, with a mine and a pillar
    // crowding the scatter radius — the exact situation the user reported.
    struct Box { float x, y, h; };
    std::vector<Box> blockers;
    blocker(em, cs, 1000.0f, 1000.0f, 70.0f, layers::HAZARD);
    blockers.push_back({1000.0f, 1000.0f, 35.0f});
    blocker(em, cs, 1030.0f, 1040.0f, 40.0f, layers::OBSTACLE);
    blockers.push_back({1030.0f, 1040.0f, 20.0f});
    mine(em, cs, 960.0f, 1030.0f, 30.0f);
    blockers.push_back({960.0f, 1030.0f, 15.0f});

    dead_enemy(em, cs, 1000.0f, 1000.0f);
    deaths.update(cs, em, bb);

    const auto coins = coin_centres(cs, half);
    REQUIRE(coins.size() == 3);
    for (const auto& c : coins) {
        for (const Box& b : blockers) {
            CHECK_FALSE(overlaps(c.first, c.second, half, b.x, b.y, b.h));
        }
    }
    // ... and not on each other either.
    for (size_t i = 0; i < coins.size(); ++i) {
        for (size_t j = i + 1; j < coins.size(); ++j) {
            CHECK_FALSE(overlaps(coins[i].first, coins[i].second, half,
                                 coins[j].first, coins[j].second, half));
        }
    }
}

TEST_CASE("the placement search draws no RNG, however many spots it rejects",
          "[loot][placement][determinism]") {
    // Two identically-seeded death systems. World A drops into empty space; world
    // B drops into a wall of blockers, so its search rejects candidate after
    // candidate. Then BOTH kill a second enemy in clean space. If the search had
    // consumed a single random value in either world, the second kill's scatter
    // would differ between them.
    auto run = [](bool crowded) {
        EntityManager em;
        ComponentStorage cs;
        Blackboard bb;
        EnemyDeathSystem deaths;
        deaths.set_economy(test_economy(), 777u);

        if (crowded) {
            // A 5x5 grid of hazards blanketing the whole drop + nudge radius,
            // so every candidate the spiral offers is rejected.
            for (int gx = -2; gx <= 2; ++gx)
                for (int gy = -2; gy <= 2; ++gy)
                    blocker(em, cs, 1000.0f + static_cast<float>(gx) * 40.0f,
                            1000.0f + static_cast<float>(gy) * 40.0f, 44.0f, layers::HAZARD);
        }
        dead_enemy(em, cs, 1000.0f, 1000.0f);
        deaths.update(cs, em, bb);

        // Clear the world between kills so the second drop is unobstructed and
        // identical in both runs — only the RNG cursor can still differ.
        for (Entity e : em.all_entities()) cs.add_component<DestroyRequest>(e, DestroyRequest{});
        destroy_marked_entities(em, cs);

        dead_enemy(em, cs, 5000.0f, 5000.0f);
        deaths.update(cs, em, bb);
        auto out = coin_centres(cs, test_economy().pickup_size * 0.5f);
        std::sort(out.begin(), out.end());
        return out;
    };

    const auto clear_world = run(false);
    const auto crowded_world = run(true);

    REQUIRE(clear_world.size() == 3);
    REQUIRE(crowded_world.size() == clear_world.size());
    for (size_t i = 0; i < clear_world.size(); ++i) {
        CHECK(clear_world[i].first == crowded_world[i].first);
        CHECK(clear_world[i].second == crowded_world[i].second);
    }
}
