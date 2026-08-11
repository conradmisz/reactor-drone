// Unit tests for bloom_math.hpp — the pure geometry/intensity half of the
// v3 Tier 1 render-target bloom. No SDL, no window.

#include <catch2/catch_test_macros.hpp>

#include "engine/ecs/systems/bloom_math.hpp"

using bloom_math::chain_sizes;
using bloom_math::level_alpha;

TEST_CASE("chain_sizes halves each level, rounding up", "[bloom]") {
    auto c = chain_sizes(980, 660, 4);
    REQUIRE(c.size() == 4);
    CHECK(c[0].w == 490); CHECK(c[0].h == 330);
    CHECK(c[1].w == 245); CHECK(c[1].h == 165);
    CHECK(c[2].w == 123); CHECK(c[2].h == 83);   // (245+1)/2, (165+1)/2
    CHECK(c[3].w == 62);  CHECK(c[3].h == 42);
}

TEST_CASE("chain_sizes stops before a dimension drops below min_edge", "[bloom]") {
    // 64 -> 32 -> 16 -> 8 -> (4 < 8, stop)
    auto c = chain_sizes(64, 64, 10, 8);
    REQUIRE(c.size() == 3);
    CHECK(c.back().w == 8);
}

TEST_CASE("chain_sizes degenerate inputs yield an empty chain", "[bloom]") {
    CHECK(chain_sizes(0, 660, 4).empty());
    CHECK(chain_sizes(980, -1, 4).empty());
    CHECK(chain_sizes(980, 660, 0).empty());
    CHECK(chain_sizes(980, 660, -3).empty());
}

TEST_CASE("level_alpha reads the authored list, falls back past its end", "[bloom]") {
    std::vector<float> in{1.0f, 0.5f};
    CHECK(level_alpha(in, 0, 0.2f) == 255);
    CHECK(level_alpha(in, 1, 0.2f) == 128);           // 0.5*255+0.5 rounds to 128
    CHECK(level_alpha(in, 2, 0.2f) == 51);            // fallback 0.2
    CHECK(level_alpha({}, 0, 0.0f) == 0);
}

TEST_CASE("level_alpha clamps out-of-range intensities", "[bloom]") {
    std::vector<float> in{-2.0f, 1.5f};
    CHECK(level_alpha(in, 0, 0.5f) == 0);
    CHECK(level_alpha(in, 1, 0.5f) == 255);
    CHECK(level_alpha(in, 5, 9.0f) == 255);           // fallback clamps too
}
