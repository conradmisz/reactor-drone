/**
 * Property tests for next_focus_index (ui_focus_math.hpp) — Option-040 Phase 7.
 *
 * Property 1: the focus index stays in range and cycles — forward then backward
 * is the identity, and `count` forward steps return to the start. Bounded 10x5.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>

#include "engine/ui_focus_math.hpp"

// Bounded property-test iteration counts (property-test-bounds steering).
constexpr int NUM_OUTER_TESTS = 10;  // list sizes
constexpr int NUM_INNER_TESTS = 5;   // starting indices per size

TEST_CASE("next_focus_index: result is in [0, count) for any current",
          "[Engine][ui_focus][property]") {
    auto count   = GENERATE(take(NUM_OUTER_TESTS, random(1, 20)));
    auto current = GENERATE(take(NUM_INNER_TESTS, random(-5, 25)));
    int fwd = next_focus_index(count, current, true);
    int bwd = next_focus_index(count, current, false);
    REQUIRE(fwd >= 0);
    REQUIRE(fwd < count);
    REQUIRE(bwd >= 0);
    REQUIRE(bwd < count);
}

TEST_CASE("next_focus_index: forward then backward returns to start",
          "[Engine][ui_focus][property]") {
    auto count = GENERATE(take(NUM_OUTER_TESTS, random(1, 20)));
    auto start = GENERATE(take(NUM_INNER_TESTS, random(0, 19)));
    int i = start % count;
    int there = next_focus_index(count, i, true);
    int back  = next_focus_index(count, there, false);
    REQUIRE(back == i);
}

TEST_CASE("next_focus_index: count forward steps complete a full cycle",
          "[Engine][ui_focus][property]") {
    auto count = GENERATE(take(NUM_OUTER_TESTS, random(1, 20)));
    auto start = GENERATE(take(NUM_INNER_TESTS, random(0, 19)));
    int i = start % count;
    int cur = i;
    for (int step = 0; step < count; ++step) {
        cur = next_focus_index(count, cur, true);
    }
    REQUIRE(cur == i);  // back to the start after exactly `count` steps
}
