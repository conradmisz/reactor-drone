/**
 * Unit tests for collision math functions (layers_compatible)
 *
 * These tests verify layer/mask filtering with concrete game scenarios
 * from the Asteroids game: ship, bullet, asteroid collision groups.
 *
 * Requirements tested: 9.1, 9.2, 9.3, 9.4, 9.5, 9.6
 */

#include <catch2/catch_test_macros.hpp>
#include "engine/ecs/systems/collision_math.hpp"

TEST_CASE("layers_compatible filtering", "[collision][unit]") {
    // Ship:     layer=1 (0x01), mask=4 (0x04) — collides with asteroids
    // Bullet:   layer=2 (0x02), mask=4 (0x04) — collides with asteroids
    // Asteroid: layer=4 (0x04), mask=3 (0x03) — collides with ships and bullets

    SECTION("ShipAsteroidCompatible — Req 9.1") {
        // ship layer=1, mask=4 vs asteroid layer=4, mask=3
        // (1 & 3) != 0 → true, (4 & 4) != 0 → true → compatible
        REQUIRE(layers_compatible(1, 4, 4, 3) == true);
    }

    SECTION("ShipBulletIncompatible — Req 9.2") {
        // ship layer=1, mask=4 vs bullet layer=2, mask=4
        // (1 & 4) != 0 → false → incompatible
        REQUIRE(layers_compatible(1, 4, 2, 4) == false);
    }

    SECTION("AsteroidAsteroidIncompatible — Req 9.3") {
        // asteroid layer=4, mask=3 vs asteroid layer=4, mask=3
        // (4 & 3) != 0 → false → incompatible
        REQUIRE(layers_compatible(4, 3, 4, 3) == false);
    }

    SECTION("LayerZeroAlwaysFalse — Req 9.4") {
        // layer=0 means "no collision group"
        REQUIRE(layers_compatible(0, 0xFF, 4, 3) == false);
        REQUIRE(layers_compatible(4, 3, 0, 0xFF) == false);
    }

    SECTION("MaskZeroAlwaysFalse — Req 9.5") {
        // mask=0 means "collides with nothing"
        REQUIRE(layers_compatible(1, 0, 4, 3) == false);
        REQUIRE(layers_compatible(4, 3, 1, 0) == false);
    }

    SECTION("AllBitsSetCompatible — Req 9.6") {
        // layer=0xFF, mask=0xFF vs same → all-collide configuration
        REQUIRE(layers_compatible(0xFF, 0xFF, 0xFF, 0xFF) == true);
    }
}
