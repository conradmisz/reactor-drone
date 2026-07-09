/**
 * Property-based tests for AABB overlap and layers_compatible functions
 *
 * These tests verify universal properties that should hold across all inputs
 * using Catch2's GENERATE() for property-based testing.
 *
 * Requirements tested: 11.1–11.7, 1.2, 2.2, 2.3, 8.6
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>
#include "engine/ecs/systems/collision_math.hpp"

// Configurable test iteration counts
constexpr int NUM_OUTER_TESTS = 10;  // Number of different AABB configurations
constexpr int NUM_INNER_TESTS = 5;   // Number of variations per configuration

// Feature: 050-02-aabb-collision-detection, Property 1: AABB overlap commutativity
// **Validates: Requirements 11.5**
TEST_CASE("AABB overlap commutativity", "[collision][property]") {
    SECTION("aabb_overlap(a,b) == aabb_overlap(b,a) for random AABBs") {
        auto ax = GENERATE(take(NUM_OUTER_TESTS, random(-500.0f, 500.0f)));
        auto ay = GENERATE(take(NUM_INNER_TESTS, random(-500.0f, 500.0f)));
        // Use arbitrary dimensions (can be zero or negative to test edge cases)
        float aw = ax + 100.0f;  // Derived to avoid combinatorial explosion
        float ah = 50.0f;
        auto bx = GENERATE(take(NUM_INNER_TESTS, random(-500.0f, 500.0f)));
        float by = ay + 30.0f;   // Derived
        float bw = 80.0f;
        float bh = 60.0f;

        REQUIRE(aabb_overlap(ax, ay, aw, ah, bx, by, bw, bh) ==
                aabb_overlap(bx, by, bw, bh, ax, ay, aw, ah));
    }
}

// Feature: 050-02-aabb-collision-detection, Property 2: Containment implies overlap
// **Validates: Requirements 11.6, 1.2, 8.6**
TEST_CASE("Containment implies overlap", "[collision][property]") {
    SECTION("Inner AABB fully contained within outer → overlap is true") {
        auto ox = GENERATE(take(NUM_OUTER_TESTS, random(-500.0f, 500.0f)));
        auto oy = GENERATE(take(NUM_INNER_TESTS, random(-500.0f, 500.0f)));
        float ow = 200.0f;  // Outer dimensions (always positive)
        float oh = 200.0f;

        // Inner AABB: offset inside outer, smaller dimensions
        float ix = ox + 10.0f;
        float iy = oy + 10.0f;
        float iw = 50.0f;   // Fits within 200 - 10 margin
        float ih = 50.0f;

        REQUIRE(aabb_overlap(ox, oy, ow, oh, ix, iy, iw, ih) == true);
    }
}

// Feature: 050-02-aabb-collision-detection, Property 3: Self-overlap (reflexivity)
// **Validates: Requirements 11.7, 1.2**
TEST_CASE("Self-overlap reflexivity", "[collision][property]") {
    SECTION("AABB with positive dims overlaps itself") {
        auto x = GENERATE(take(NUM_OUTER_TESTS, random(-500.0f, 500.0f)));
        auto y = GENERATE(take(NUM_INNER_TESTS, random(-500.0f, 500.0f)));
        float w = 100.0f;  // Positive width
        float h = 75.0f;   // Positive height

        REQUIRE(aabb_overlap(x, y, w, h, x, y, w, h) == true);
    }
}

// Feature: 050-02-aabb-collision-detection, Property 4: layers_compatible matches bitwise model
// **Validates: Requirements 2.2, 2.3**
TEST_CASE("layers_compatible matches bitwise model", "[collision][property]") {
    SECTION("Function result equals direct bitwise expression") {
        auto layer_a = GENERATE(take(NUM_OUTER_TESTS, random(0, 255)));
        auto mask_a  = GENERATE(take(NUM_INNER_TESTS, random(0, 255)));
        // Derive b values to avoid 4-level combinatorial explosion
        int layer_b = (layer_a + 37) % 256;
        int mask_b  = (mask_a + 73) % 256;

        uint8_t la = static_cast<uint8_t>(layer_a);
        uint8_t ma = static_cast<uint8_t>(mask_a);
        uint8_t lb = static_cast<uint8_t>(layer_b);
        uint8_t mb = static_cast<uint8_t>(mask_b);

        bool expected = ((la & mb) != 0) && ((lb & ma) != 0);
        REQUIRE(layers_compatible(la, ma, lb, mb) == expected);
    }
}
