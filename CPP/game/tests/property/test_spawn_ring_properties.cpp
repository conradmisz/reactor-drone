/**
 * Property-based tests for ring_spawn_point() — enemy entry points (v2,
 * Upgrade Phase 2).
 *
 * Property P1: the result is always inside the arena circle (within epsilon of
 *              arena_radius from the centre), for any player position inside
 *              the arena and any angle.
 * Property P2: when the unclamped ring point already lies inside the arena, the
 *              result is exactly spawn_radius from the player — so enemies
 *              normally enter from off-screen, and only wall-adjacent spawns
 *              get pulled closer.
 *
 * One Catch2 GENERATE drives the player's offset from the centre; the angle
 * space is swept with a plain internal loop (nesting GENERATE multiplies
 * section replays needlessly).
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>

#include <cmath>

#include "game/wave_spawner_system.hpp"

// Configurable test iteration counts (MANDATORY — workspace policy)
constexpr int NUM_OUTER_TESTS = 10;
constexpr int NUM_INNER_TESTS = 12;

namespace {
constexpr float CX = 1600.0f, CY = 1600.0f;
constexpr float ARENA_R = 1400.0f, SPAWN_R = 620.0f;
constexpr float TWO_PI = 6.28318530717958647692f;

float dist(float ax, float ay, float bx, float by) {
    return std::sqrt((ax - bx) * (ax - bx) + (ay - by) * (ay - by));
}
}  // namespace

TEST_CASE("ring_spawn_point stays in the arena and rings the player",
          "[Game][spawner][property]") {
    // Player somewhere on a random radius from the centre (inside the arena).
    auto player_r = GENERATE(take(NUM_OUTER_TESTS, random(0.0f, ARENA_R)));
    auto player_a = GENERATE(take(1, random(0.0f, TWO_PI)));
    const float px = CX + player_r * std::cos(player_a);
    const float py = CY + player_r * std::sin(player_a);

    for (int i = 0; i < NUM_INNER_TESTS; ++i) {
        float angle = TWO_PI * static_cast<float>(i) / NUM_INNER_TESTS;
        Vec2 s = ring_spawn_point(px, py, angle, SPAWN_R, CX, CY, ARENA_R);

        // P1: never outside the play circle.
        REQUIRE(dist(s.x, s.y, CX, CY) <= ARENA_R + 0.01f);

        // P2: unclamped spawns sit exactly on the player's ring.
        float raw_x = px + SPAWN_R * std::cos(angle);
        float raw_y = py + SPAWN_R * std::sin(angle);
        if (dist(raw_x, raw_y, CX, CY) <= ARENA_R) {
            REQUIRE(std::fabs(dist(s.x, s.y, px, py) - SPAWN_R) < 0.01f);
        } else {
            // Clamped: pulled inward, so never farther than the raw ring point.
            REQUIRE(dist(s.x, s.y, px, py) <= SPAWN_R + 0.01f);
        }
    }
}
