/**
 * Unit tests for circle_circle_overlap edge cases
 *
 * Requirements tested: 15.1, 15.2, 15.3, 15.4, 15.5, 15.6
 */

#include <catch2/catch_test_macros.hpp>
#include "engine/ecs/systems/collision_math.hpp"

TEST_CASE("circle_circle_overlap edge cases", "[collision][unit]") {
    SECTION("Overlapping circles return true") {
        // Two circles at (0,0) r=10 and (5,0) r=10 — centers closer than sum of radii
        REQUIRE(circle_circle_overlap(0.0f, 0.0f, 10.0f, 5.0f, 0.0f, 10.0f) == true);
    }

    SECTION("Tangent circles return false (strict inequality)") {
        // Two circles at (0,0) r=5 and (10,0) r=5 — distance == r1+r2
        REQUIRE(circle_circle_overlap(0.0f, 0.0f, 5.0f, 10.0f, 0.0f, 5.0f) == false);
    }

    SECTION("Separated circles return false") {
        // Two circles at (0,0) r=5 and (100,0) r=5 — far apart
        REQUIRE(circle_circle_overlap(0.0f, 0.0f, 5.0f, 100.0f, 0.0f, 5.0f) == false);
    }

    SECTION("Zero radius returns false") {
        REQUIRE(circle_circle_overlap(0.0f, 0.0f, 0.0f, 5.0f, 0.0f, 10.0f) == false);
        REQUIRE(circle_circle_overlap(0.0f, 0.0f, 10.0f, 5.0f, 0.0f, 0.0f) == false);
    }

    SECTION("Same center with positive radii returns true") {
        REQUIRE(circle_circle_overlap(5.0f, 5.0f, 3.0f, 5.0f, 5.0f, 7.0f) == true);
    }

    SECTION("Negative radius returns false") {
        REQUIRE(circle_circle_overlap(0.0f, 0.0f, -5.0f, 5.0f, 0.0f, 10.0f) == false);
        REQUIRE(circle_circle_overlap(0.0f, 0.0f, 10.0f, 5.0f, 0.0f, -3.0f) == false);
    }
}
