/**
 * Unit tests for circle_aabb_overlap edge cases
 *
 * Requirements tested: 16.1, 16.2, 16.3, 16.4, 16.5, 16.6, 16.7
 */

#include <catch2/catch_test_macros.hpp>
#include "engine/ecs/systems/collision_math.hpp"

TEST_CASE("circle_aabb_overlap edge cases", "[collision][unit]") {
    SECTION("Circle center inside AABB returns true") {
        // Circle at (15, 15) r=5, AABB at (0,0) 30x30 — center well inside
        REQUIRE(circle_aabb_overlap(15.0f, 15.0f, 5.0f, 0.0f, 0.0f, 30.0f, 30.0f) == true);
    }

    SECTION("Circle entirely outside AABB returns false") {
        // Circle at (100, 100) r=5, AABB at (0,0) 30x30 — far away
        REQUIRE(circle_aabb_overlap(100.0f, 100.0f, 5.0f, 0.0f, 0.0f, 30.0f, 30.0f) == false);
    }

    SECTION("Circle tangent to AABB edge returns false (strict inequality)") {
        // Circle at (35, 15) r=5, AABB at (0,0) 30x30
        // Closest point on AABB is (30, 15), distance = 5 = r → tangent
        REQUIRE(circle_aabb_overlap(35.0f, 15.0f, 5.0f, 0.0f, 0.0f, 30.0f, 30.0f) == false);
    }

    SECTION("Circle overlaps AABB corner returns true") {
        // Circle at (31, 31) r=5, AABB at (0,0) 30x30
        // Closest point is (30, 30), distance = sqrt(2) ≈ 1.414 < 5
        REQUIRE(circle_aabb_overlap(31.0f, 31.0f, 5.0f, 0.0f, 0.0f, 30.0f, 30.0f) == true);
    }

    SECTION("Circle near corner but does not reach returns false") {
        // Circle at (40, 40) r=5, AABB at (0,0) 30x30
        // Closest point is (30, 30), distance = sqrt(200) ≈ 14.14 > 5
        REQUIRE(circle_aabb_overlap(40.0f, 40.0f, 5.0f, 0.0f, 0.0f, 30.0f, 30.0f) == false);
    }

    SECTION("AABB with zero or negative dimensions returns false") {
        REQUIRE(circle_aabb_overlap(5.0f, 5.0f, 5.0f, 0.0f, 0.0f, 0.0f, 10.0f) == false);
        REQUIRE(circle_aabb_overlap(5.0f, 5.0f, 5.0f, 0.0f, 0.0f, 10.0f, 0.0f) == false);
        REQUIRE(circle_aabb_overlap(5.0f, 5.0f, 5.0f, 0.0f, 0.0f, -5.0f, 10.0f) == false);
        REQUIRE(circle_aabb_overlap(5.0f, 5.0f, 5.0f, 0.0f, 0.0f, 10.0f, -5.0f) == false);
    }

    SECTION("Circle with zero or negative radius returns false") {
        REQUIRE(circle_aabb_overlap(5.0f, 5.0f, 0.0f, 0.0f, 0.0f, 30.0f, 30.0f) == false);
        REQUIRE(circle_aabb_overlap(5.0f, 5.0f, -3.0f, 0.0f, 0.0f, 30.0f, 30.0f) == false);
    }
}
