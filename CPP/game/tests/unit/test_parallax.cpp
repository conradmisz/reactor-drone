/**
 * Unit tests for parallax:: — the pure parallax-scroll offset (v2, Phase 5).
 *
 * offset = camera * (1 - scroll_factor): a fully-attached far layer
 * (scroll_factor 1) never moves; a fully-detached near layer (scroll_factor 0)
 * moves 1:1 with the camera; anything in between moves proportionally.
 *
 * All of this is pure — no game loop, no SDL, no Blackboard.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cmath>

#include "game/parallax.hpp"

TEST_CASE("parallax::parallax_offset at the scroll_factor extremes", "[Game][parallax][unit]") {
    // scroll_factor 1 => glued to the camera => no relative motion, any camera.
    CHECK(parallax::parallax_offset(0.0f, 1.0f)  == Catch::Approx(0.0f));
    CHECK(parallax::parallax_offset(123.0f, 1.0f) == Catch::Approx(0.0f));

    // scroll_factor 0 => fully detached => moves 1:1 with the camera.
    CHECK(parallax::parallax_offset(0.0f, 0.0f)  == Catch::Approx(0.0f));
    CHECK(parallax::parallax_offset(40.0f, 0.0f) == Catch::Approx(40.0f));
    CHECK(parallax::parallax_offset(-40.0f, 0.0f) == Catch::Approx(-40.0f));
}

TEST_CASE("parallax::parallax_offset scales linearly between the extremes", "[Game][parallax][unit]") {
    CHECK(parallax::parallax_offset(100.0f, 0.25f) == Catch::Approx(75.0f));
    CHECK(parallax::parallax_offset(100.0f, 0.5f)  == Catch::Approx(50.0f));
    CHECK(parallax::parallax_offset(100.0f, 0.75f) == Catch::Approx(25.0f));
}

TEST_CASE("parallax::parallax_offset — a nearer layer moves at least as much as a farther one",
          "[Game][parallax][unit]") {
    // Same camera displacement, smaller scroll_factor (nearer) => larger offset.
    float cam = 30.0f;
    float near_off = parallax::parallax_offset(cam, 0.2f);
    float far_off  = parallax::parallax_offset(cam, 0.8f);
    CHECK(std::abs(near_off) >= std::abs(far_off));
}
