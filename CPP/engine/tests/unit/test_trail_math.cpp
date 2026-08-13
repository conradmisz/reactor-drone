// Unit tests for trail_math.hpp — the pure sampling/taper half of the v3
// Tier 7 position-history trails. No SDL, no window.

#include <catch2/catch_test_macros.hpp>

#include "engine/ecs/systems/trail_math.hpp"

using line_mesh::P2;
using trail::points_within_budget;
using trail::push_sample;
using trail::taper_widths;

TEST_CASE("push_sample appends when the entity has moved far enough", "[trail]") {
    std::vector<P2> pts;
    CHECK(push_sample(pts, {0.0f, 0.0f}, 2.0f, 8));
    CHECK(push_sample(pts, {5.0f, 0.0f}, 2.0f, 8));
    REQUIRE(pts.size() == 2);
    CHECK(pts.back().x == 5.0f);   // newest last
}

TEST_CASE("push_sample rejects a sample inside min_spacing", "[trail]") {
    std::vector<P2> pts;
    REQUIRE(push_sample(pts, {0.0f, 0.0f}, 2.0f, 8));
    CHECK_FALSE(push_sample(pts, {1.0f, 0.0f}, 2.0f, 8));   // 1 < 2
    CHECK_FALSE(push_sample(pts, {0.0f, 1.9f}, 2.0f, 8));
    CHECK(pts.size() == 1);
}

// The hit-stop case: delta_time is 0 for K frames, so every entity reports the
// exact same position every frame. Without the spacing guard the buffer fills
// with duplicates and the ribbon collapses.
TEST_CASE("a stationary entity never grows its trail", "[trail]") {
    std::vector<P2> pts;
    REQUIRE(push_sample(pts, {10.0f, 10.0f}, 1.0f, 16));
    for (int i = 0; i < 100; ++i) {
        CHECK_FALSE(push_sample(pts, {10.0f, 10.0f}, 1.0f, 16));
    }
    CHECK(pts.size() == 1);
}

TEST_CASE("push_sample drops the oldest point past max_points", "[trail]") {
    std::vector<P2> pts;
    for (int i = 0; i < 20; ++i) {
        push_sample(pts, {static_cast<float>(i) * 10.0f, 0.0f}, 1.0f, 5);
    }
    REQUIRE(pts.size() == 5);
    CHECK(pts.front().x == 150.0f);   // 15..19 survive
    CHECK(pts.back().x == 190.0f);
}

TEST_CASE("max_points of 0 accepts nothing", "[trail]") {
    std::vector<P2> pts;
    CHECK_FALSE(push_sample(pts, {0.0f, 0.0f}, 1.0f, 0));
    CHECK(pts.empty());
}

TEST_CASE("taper_widths runs tail-thin to head-wide", "[trail]") {
    auto w = taper_widths(5, 8.0f);
    REQUIRE(w.size() == 5);
    CHECK(w.front() == 0.0f);   // oldest point, stored first
    CHECK(w.back() == 8.0f);    // newest point = the head
    CHECK(w[2] == 4.0f);
    for (std::size_t i = 1; i < w.size(); ++i) CHECK(w[i] >= w[i - 1]);
}

TEST_CASE("taper_widths honours a non-zero tail width", "[trail]") {
    auto w = taper_widths(3, 10.0f, 2.0f);
    REQUIRE(w.size() == 3);
    CHECK(w.front() == 2.0f);
    CHECK(w.back() == 10.0f);
    CHECK(w[1] == 6.0f);
}

TEST_CASE("taper_widths degenerate sizes", "[trail]") {
    CHECK(taper_widths(0, 8.0f).empty());
    auto one = taper_widths(1, 8.0f);
    REQUIRE(one.size() == 1);
    CHECK(one[0] == 8.0f);
}

TEST_CASE("points_within_budget spends 2 verts per point", "[trail]") {
    CHECK(points_within_budget(10, 100) == 10);   // wants less than it can afford
    CHECK(points_within_budget(10, 12) == 6);     // clamped to the budget
    CHECK(points_within_budget(10, 4) == 2);
}

// A one-point stub is worse than nothing: build_ribbon rejects <2 points, so
// spending budget on it draws nothing at all.
TEST_CASE("points_within_budget drops a trail it cannot draw", "[trail]") {
    CHECK(points_within_budget(10, 3) == 0);   // affords 1 point
    CHECK(points_within_budget(10, 0) == 0);
}
