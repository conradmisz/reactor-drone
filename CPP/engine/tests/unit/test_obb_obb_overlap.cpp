/**
 * Unit tests for obb_obb_overlap()
 *
 * Tests edge cases for the Separating Axis Theorem OBB-OBB overlap function.
 * Uses strict inequality — tangent (touching) edges do NOT count as overlap.
 *
 * Requirements tested: 18.1, 18.2, 18.3, 18.4, 18.5, 18.6, 18.7, 18.8
 */

#include <catch2/catch_test_macros.hpp>
#include "engine/ecs/systems/collision_math.hpp"
#include <cmath>

static constexpr float PI = 3.14159265f;

TEST_CASE("obb_obb_overlap edge cases", "[collision][unit]") {

    SECTION("Two axis-aligned OBBs overlapping — Req 18.1") {
        // OBB1: center (0,0), hw=10, hh=10, angle=0
        // OBB2: center (5,5), hw=10, hh=10, angle=0
        // They overlap significantly
        REQUIRE(obb_obb_overlap(0.0f, 0.0f, 10.0f, 10.0f, 0.0f,
                                5.0f, 5.0f, 10.0f, 10.0f, 0.0f) == true);
    }

    SECTION("Two axis-aligned OBBs separated — Req 18.2") {
        // OBB1: center (0,0), hw=10, hh=10
        // OBB2: center (25,0), hw=10, hh=10
        // Gap of 5 units between them
        REQUIRE(obb_obb_overlap(0.0f, 0.0f, 10.0f, 10.0f, 0.0f,
                                25.0f, 0.0f, 10.0f, 10.0f, 0.0f) == false);
    }

    SECTION("Two axis-aligned OBBs tangent — Req 18.3") {
        // OBB1: center (0,0), hw=10, hh=10
        // OBB2: center (20,0), hw=10, hh=10
        // Touching at x=10, strict inequality → no overlap
        REQUIRE(obb_obb_overlap(0.0f, 0.0f, 10.0f, 10.0f, 0.0f,
                                20.0f, 0.0f, 10.0f, 10.0f, 0.0f) == false);
    }

    SECTION("One OBB rotated 45 degrees overlapping — Req 18.4") {
        // OBB1: center (0,0), hw=10, hh=10, angle=0
        // OBB2: center (12,0), hw=10, hh=10, angle=PI/4
        // Rotated OBB extends further along diagonal, should overlap
        REQUIRE(obb_obb_overlap(0.0f, 0.0f, 10.0f, 10.0f, 0.0f,
                                12.0f, 0.0f, 10.0f, 10.0f, PI / 4.0f) == true);
    }

    SECTION("One OBB rotated 45 degrees separated — Req 18.5") {
        // OBB1: center (0,0), hw=5, hh=5, angle=0
        // OBB2: center (20,0), hw=5, hh=5, angle=PI/4
        // Far apart, no overlap
        REQUIRE(obb_obb_overlap(0.0f, 0.0f, 5.0f, 5.0f, 0.0f,
                                20.0f, 0.0f, 5.0f, 5.0f, PI / 4.0f) == false);
    }

    SECTION("Both OBBs rotated by different angles and overlapping — Req 18.6") {
        // OBB1: center (0,0), hw=10, hh=10, angle=PI/6
        // OBB2: center (5,5), hw=10, hh=10, angle=PI/3
        // Close together, should overlap
        REQUIRE(obb_obb_overlap(0.0f, 0.0f, 10.0f, 10.0f, PI / 6.0f,
                                5.0f, 5.0f, 10.0f, 10.0f, PI / 3.0f) == true);
    }

    SECTION("Zero or negative half-extents — Req 18.7") {
        // Zero half-width on first OBB
        REQUIRE(obb_obb_overlap(0.0f, 0.0f, 0.0f, 10.0f, 0.0f,
                                5.0f, 5.0f, 10.0f, 10.0f, 0.0f) == false);
        // Negative half-height on second OBB
        REQUIRE(obb_obb_overlap(0.0f, 0.0f, 10.0f, 10.0f, 0.0f,
                                5.0f, 5.0f, 10.0f, -5.0f, 0.0f) == false);
        // Zero half-height on first OBB
        REQUIRE(obb_obb_overlap(0.0f, 0.0f, 10.0f, 0.0f, 0.0f,
                                5.0f, 5.0f, 10.0f, 10.0f, 0.0f) == false);
        // Negative half-width on first OBB
        REQUIRE(obb_obb_overlap(0.0f, 0.0f, -3.0f, 10.0f, 0.0f,
                                5.0f, 5.0f, 10.0f, 10.0f, 0.0f) == false);
    }

    SECTION("Same center with positive half-extents — Req 18.8") {
        // Both at same center, various rotations — always overlap
        REQUIRE(obb_obb_overlap(10.0f, 10.0f, 5.0f, 5.0f, 0.0f,
                                10.0f, 10.0f, 5.0f, 5.0f, 0.0f) == true);
        REQUIRE(obb_obb_overlap(10.0f, 10.0f, 5.0f, 5.0f, PI / 4.0f,
                                10.0f, 10.0f, 5.0f, 5.0f, PI / 3.0f) == true);
        REQUIRE(obb_obb_overlap(10.0f, 10.0f, 3.0f, 7.0f, 1.0f,
                                10.0f, 10.0f, 8.0f, 2.0f, 2.5f) == true);
    }
}
