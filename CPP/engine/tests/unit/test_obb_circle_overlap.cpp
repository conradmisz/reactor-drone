/**
 * Unit tests for obb_circle_overlap()
 *
 * Tests edge cases for the OBB-circle overlap function using
 * closest-point-on-rotated-rect algorithm. Strict inequality —
 * tangent does NOT count as overlap.
 *
 * Requirements tested: 19.1, 19.2, 19.3, 19.4, 19.5, 19.6, 19.7, 19.8
 */

#include <catch2/catch_test_macros.hpp>
#include "engine/ecs/systems/collision_math.hpp"
#include <cmath>

static constexpr float PI = 3.14159265f;

TEST_CASE("obb_circle_overlap edge cases", "[collision][unit]") {

    SECTION("Circle center inside axis-aligned OBB — Req 19.1") {
        // OBB: center (0,0), hw=10, hh=10, angle=0
        // Circle: center (3,3), radius=2 — fully inside
        REQUIRE(obb_circle_overlap(0.0f, 0.0f, 10.0f, 10.0f, 0.0f,
                                   3.0f, 3.0f, 2.0f) == true);
    }

    SECTION("Circle entirely outside axis-aligned OBB — Req 19.2") {
        // OBB: center (0,0), hw=10, hh=10, angle=0
        // Circle: center (25,0), radius=5 — far away
        REQUIRE(obb_circle_overlap(0.0f, 0.0f, 10.0f, 10.0f, 0.0f,
                                   25.0f, 0.0f, 5.0f) == false);
    }

    SECTION("Circle tangent to axis-aligned OBB edge — Req 19.3") {
        // OBB: center (0,0), hw=10, hh=10, angle=0
        // Circle: center (15,0), radius=5 — touches at x=10 exactly
        REQUIRE(obb_circle_overlap(0.0f, 0.0f, 10.0f, 10.0f, 0.0f,
                                   15.0f, 0.0f, 5.0f) == false);
    }

    SECTION("OBB rotated 45 degrees with circle overlapping a corner — Req 19.4") {
        // OBB: center (0,0), hw=10, hh=10, angle=PI/4
        // The corner of the rotated OBB extends to ~14.14 along the x-axis
        // Circle: center (13,0), radius=5 — overlaps the corner
        REQUIRE(obb_circle_overlap(0.0f, 0.0f, 10.0f, 10.0f, PI / 4.0f,
                                   13.0f, 0.0f, 5.0f) == true);
    }

    SECTION("OBB rotated 45 degrees with circle near corner but not reaching — Req 19.5") {
        // OBB: center (0,0), hw=10, hh=10, angle=PI/4
        // Corner extends to ~14.14 along x-axis
        // Circle: center (25,0), radius=5 — too far from corner
        REQUIRE(obb_circle_overlap(0.0f, 0.0f, 10.0f, 10.0f, PI / 4.0f,
                                   25.0f, 0.0f, 5.0f) == false);
    }

    SECTION("OBB rotated 90 degrees with circle overlapping an edge — Req 19.6") {
        // OBB: center (0,0), hw=10, hh=5, angle=PI/2
        // After 90° rotation, the OBB's local X-axis becomes (0,1) and Y-axis becomes (-1,0)
        // So the OBB extends ±5 along world X and ±10 along world Y
        // Circle: center (4,0), radius=3 — overlaps the edge at x=5
        REQUIRE(obb_circle_overlap(0.0f, 0.0f, 10.0f, 5.0f, PI / 2.0f,
                                   4.0f, 0.0f, 3.0f) == true);
    }

    SECTION("Circle with zero or negative radius — Req 19.7") {
        REQUIRE(obb_circle_overlap(0.0f, 0.0f, 10.0f, 10.0f, 0.0f,
                                   0.0f, 0.0f, 0.0f) == false);
        REQUIRE(obb_circle_overlap(0.0f, 0.0f, 10.0f, 10.0f, 0.0f,
                                   0.0f, 0.0f, -5.0f) == false);
    }

    SECTION("OBB with zero or negative half-extents — Req 19.8") {
        REQUIRE(obb_circle_overlap(0.0f, 0.0f, 0.0f, 10.0f, 0.0f,
                                   0.0f, 0.0f, 5.0f) == false);
        REQUIRE(obb_circle_overlap(0.0f, 0.0f, 10.0f, 0.0f, 0.0f,
                                   0.0f, 0.0f, 5.0f) == false);
        REQUIRE(obb_circle_overlap(0.0f, 0.0f, -5.0f, 10.0f, 0.0f,
                                   0.0f, 0.0f, 5.0f) == false);
    }
}
