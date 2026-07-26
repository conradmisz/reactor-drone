/**
 * Property-based tests for PlayerControlSystem's diagonal normalization (v2).
 *
 * Property P1: the commanded velocity magnitude equals move_speed for EVERY
 *              non-idle input combination — the eight compass directions all
 *              move at the same speed. Before normalization the four diagonals
 *              were sqrt(2) (~41%) faster than the cardinals.
 * Property P2: opposing keys on an axis cancel, so left+right+up is exactly the
 *              cardinal "up" case (still magnitude move_speed), and all-four /
 *              no-keys are the only idle inputs.
 *
 * One Catch2 GENERATE drives the speed; the 16 input combinations are swept with
 * a plain internal loop over the 4 key bits.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>

#include <cmath>

#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/systems/player_control_system.hpp"

// Configurable test iteration counts (MANDATORY — workspace policy)
constexpr int NUM_OUTER_TESTS = 10;

TEST_CASE("PlayerControlSystem moves at one speed in all eight directions",
          "[Game][player_control][property]") {
    auto speed = GENERATE(take(NUM_OUTER_TESTS, random(1.0f, 900.0f)));

    for (int bits = 0; bits < 16; ++bits) {
        EntityManager em;
        ComponentStorage storage;
        Entity e = em.create_entity();

        Input in{};
        in.up    = (bits & 1) != 0;
        in.down  = (bits & 2) != 0;
        in.left  = (bits & 4) != 0;
        in.right = (bits & 8) != 0;
        storage.add_component<Input>(e, in);
        storage.add_component<Velocity>(e, Velocity{0.0f, 0.0f});

        PlayerControlSystem control(speed);
        control.update(storage);

        auto vel = storage.get_component<Velocity>(e);
        REQUIRE(vel.has_value());
        float mag = std::sqrt(vel->get().dx * vel->get().dx +
                              vel->get().dy * vel->get().dy);

        // Opposing keys cancel, so the axis is live only when exactly one of its
        // two keys is held. Idle iff neither axis is live.
        bool x_live = in.left != in.right;
        bool y_live = in.up != in.down;

        if (!x_live && !y_live) {
            CHECK(mag == 0.0f);
        } else {
            // P1/P2: every live combination — cardinal or diagonal — is move_speed.
            CHECK(std::abs(mag - speed) <= speed * 1e-5f);
        }
    }
}
