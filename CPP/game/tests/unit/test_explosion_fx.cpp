// Unit tests for explosion_fx.hpp — the pure staged geometry of the v3 Tier 11
// layered enemy explosion (D216). No SDL, no window, no entities.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>

#include "game/explosion_fx.hpp"

using Catch::Approx;
namespace fx = explosion_fx;

TEST_CASE("the shockwave ring expands fast then decelerates", "[explosionfx]") {
    const float r0 = 6.0f, r1 = 60.0f;
    CHECK(fx::ring_radius(0.0f, r0, r1) == Approx(r0));
    CHECK(fx::ring_radius(1.0f, r0, r1) == Approx(r1));
    // Ease-out: half the time is already past half the distance. A linear ring
    // reads like a growing circle; a decelerating one reads like a blast.
    CHECK(fx::ring_radius(0.5f, r0, r1) > (r0 + r1) * 0.5f);
    // Monotone throughout — a ring must never collapse inward.
    float prev = -1.0f;
    for (int i = 0; i <= 10; ++i) {
        const float r = fx::ring_radius(static_cast<float>(i) / 10.0f, r0, r1);
        CHECK(r >= prev);
        prev = r;
    }
}

TEST_CASE("the ring fades to nothing by the end of the clip", "[explosionfx]") {
    CHECK(fx::ring_alpha(0.0f) > 0.5f);
    CHECK(fx::ring_alpha(1.0f) == Approx(0.0f).margin(1e-5));
    CHECK(fx::ring_alpha(0.9f) < fx::ring_alpha(0.3f));
}

TEST_CASE("ring_points closes the circle at the requested radius",
          "[explosionfx]") {
    const auto pts = fx::ring_points(100.0f, 50.0f, 20.0f, 16);
    // 16 segments + the repeated first point, so the ribbon has no seam gap.
    REQUIRE(pts.size() == 17);
    CHECK(pts.front().x == Approx(pts.back().x));
    CHECK(pts.front().y == Approx(pts.back().y));
    for (const auto& p : pts) {
        const float d = std::hypot(p.x - 100.0f, p.y - 50.0f);
        CHECK(d == Approx(20.0f).margin(1e-3));
    }
}

TEST_CASE("shards hold off, then fly outward and stop", "[explosionfx]") {
    // Nothing at t=0: the flash reads first, debris follows it.
    CHECK(fx::shard_span(0.0f).outer == Approx(fx::shard_span(0.0f).inner));
    const auto mid = fx::shard_span(0.5f);
    CHECK(mid.outer > mid.inner);          // a real segment by mid-clip
    CHECK(mid.inner > 0.0f);               // trailing away from the centre
    const auto late = fx::shard_span(0.9f);
    CHECK(late.inner > mid.inner);         // the whole streak travels outward
}

TEST_CASE("shard angles are evenly spread and stable per explosion",
          "[explosionfx]") {
    const auto a = fx::shard_angle(0, 6, 12345u);
    CHECK(fx::shard_angle(0, 6, 12345u) == Approx(a));   // same seed, same angle
    // Different explosions do not all throw debris the same way.
    CHECK(fx::shard_angle(0, 6, 999u) != Approx(a));
    // Consecutive shards are spread roughly evenly around the circle.
    for (int i = 1; i < 6; ++i) {
        const float d = fx::shard_angle(i, 6, 12345u) - fx::shard_angle(i - 1, 6, 12345u);
        CHECK(d == Approx(6.2831853f / 6.0f).margin(1e-4));
    }
}
