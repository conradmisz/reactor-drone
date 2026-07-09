/**
 * Property-based tests for Animation component storage, AnimationSystem
 * frame advancement, SpriteSheet synchronization, and destruction pipeline
 * integration.
 *
 * These tests verify universal properties for the Animation/AnimationSystem
 * infrastructure: component round-trip, add/remove inverse, looping wrap-around,
 * one-shot completion, SpriteSheet synchronization, and destruction cleanup.
 *
 * Testing Framework: Catch2 v3
 * Constants: NUM_OUTER_TESTS = 10, NUM_INNER_TESTS = 5
 *
 * Requirements tested: 1.1-1.8, 2.1, 3.2-3.7, 4.1-4.3, 6.1-6.3, 7.1, 7.2,
 *                      11.5, 11.6, 12.1, 12.2, 13.1, 13.2, 14.1
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/systems/animation_system.hpp"
#include "engine/ecs/destruction.hpp"
#include <random>
#include <string>

// Configurable test iteration counts
static constexpr int NUM_OUTER_TESTS = 10;
static constexpr int NUM_INNER_TESTS = 5;

/// Helper: generate a random lowercase string of length [min_len, max_len]
static std::string random_string(std::mt19937& gen, int min_len, int max_len) {
    std::uniform_int_distribution<int> len_dist(min_len, max_len);
    std::uniform_int_distribution<int> char_dist('a', 'z');
    int len = len_dist(gen);
    std::string result;
    result.reserve(len);
    for (int i = 0; i < len; ++i) {
        result.push_back(static_cast<char>(char_dist(gen)));
    }
    return result;
}

// ============================================================================
// Property 1: Animation component round-trip
// Feature: 080-03-animation-system, Property 1: Animation component round-trip
//
// **Validates: Requirements 1.1-1.8, 2.1, 12.1**
//
// For any valid Animation component values (current_frame in
// 0..start_frame+frame_count-1, start_frame >= 0, frame_count >= 1,
// frame_duration >= 0.0, elapsed >= 0.0, looping true/false, playing
// true/false, finished true/false), adding the Animation to an entity
// and then retrieving it shall return an Animation with identical field
// values for all eight fields.
// ============================================================================
TEST_CASE("Property: Animation component round-trip", "[animation_system]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 1000000)));
    std::mt19937 gen(seed);

    std::uniform_int_distribution<int> sf_dist(0, 32);
    std::uniform_int_distribution<int> fc_dist(1, 16);
    std::uniform_real_distribution<float> fd_dist(0.0f, 1.0f);
    std::uniform_real_distribution<float> el_dist(0.0f, 2.0f);
    std::uniform_int_distribution<int> bool_dist(0, 1);

    for (int inner = 0; inner < NUM_INNER_TESTS; ++inner) {
        EntityManager em;
        ComponentStorage storage;

        int start_frame = sf_dist(gen);
        int frame_count = fc_dist(gen);
        std::uniform_int_distribution<int> cf_dist(0, start_frame + frame_count - 1);
        int current_frame = cf_dist(gen);
        float frame_duration = fd_dist(gen);
        float elapsed = el_dist(gen);
        bool looping = bool_dist(gen) == 1;
        bool playing = bool_dist(gen) == 1;
        bool finished = bool_dist(gen) == 1;

        Entity e = em.create_entity();
        storage.add_component<Animation>(e, Animation{
            current_frame, start_frame, frame_count,
            frame_duration, elapsed, looping, playing, finished
        });

        auto retrieved = storage.get_component<Animation>(e);
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->get().current_frame == current_frame);
        REQUIRE(retrieved->get().start_frame == start_frame);
        REQUIRE(retrieved->get().frame_count == frame_count);
        REQUIRE(retrieved->get().frame_duration == frame_duration);
        REQUIRE(retrieved->get().elapsed == elapsed);
        REQUIRE(retrieved->get().looping == looping);
        REQUIRE(retrieved->get().playing == playing);
        REQUIRE(retrieved->get().finished == finished);
    }
}

// ============================================================================
// Property 2: Animation add/remove inverse
// Feature: 080-03-animation-system, Property 2: Animation add/remove inverse
//
// **Validates: Requirements 2.1, 12.2**
//
// For any entity, adding an Animation component and then calling
// has_component<Animation>() shall return true. Subsequently removing
// the Animation via remove_component<Animation>() and then calling
// has_component<Animation>() shall return false.
// ============================================================================
TEST_CASE("Property: Animation add/remove inverse", "[animation_system]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 1000000)));
    std::mt19937 gen(seed);

    std::uniform_int_distribution<int> sf_dist(0, 32);
    std::uniform_int_distribution<int> fc_dist(1, 16);
    std::uniform_real_distribution<float> fd_dist(0.0f, 1.0f);
    std::uniform_real_distribution<float> el_dist(0.0f, 2.0f);
    std::uniform_int_distribution<int> bool_dist(0, 1);

    for (int inner = 0; inner < NUM_INNER_TESTS; ++inner) {
        EntityManager em;
        ComponentStorage storage;

        int start_frame = sf_dist(gen);
        int frame_count = fc_dist(gen);
        std::uniform_int_distribution<int> cf_dist(0, start_frame + frame_count - 1);
        int current_frame = cf_dist(gen);
        float frame_duration = fd_dist(gen);
        float elapsed = el_dist(gen);
        bool looping = bool_dist(gen) == 1;
        bool playing = bool_dist(gen) == 1;
        bool finished = bool_dist(gen) == 1;

        Entity e = em.create_entity();
        storage.add_component<Animation>(e, Animation{
            current_frame, start_frame, frame_count,
            frame_duration, elapsed, looping, playing, finished
        });

        REQUIRE(storage.has_component<Animation>(e));

        storage.remove_component<Animation>(e);

        REQUIRE_FALSE(storage.has_component<Animation>(e));
    }
}

// ============================================================================
// Property 3: Looping animation wrap-around
// Feature: 080-03-animation-system, Property 3: Looping animation wrap-around
//
// **Validates: Requirements 3.3-3.6, 6.1, 6.2, 11.5**
//
// For any valid looping Animation configuration (start_frame >= 0,
// frame_count in 1..16, frame_duration in 0.01..1.0), after applying
// exactly frame_count advances of frame_duration each, the Animation's
// current_frame shall equal start_frame (wrap-around).
// ============================================================================
TEST_CASE("Property: Looping animation wrap-around", "[animation_system]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 1000000)));
    std::mt19937 gen(seed);

    std::uniform_int_distribution<int> sf_dist(0, 32);
    std::uniform_int_distribution<int> fc_dist(1, 16);
    std::uniform_real_distribution<float> fd_dist(0.01f, 1.0f);

    for (int inner = 0; inner < NUM_INNER_TESTS; ++inner) {
        EntityManager em;
        ComponentStorage storage;
        Blackboard blackboard;
        AnimationSystem anim_system;

        int start_frame = sf_dist(gen);
        int frame_count = fc_dist(gen);
        float frame_duration = fd_dist(gen);

        Entity e = em.create_entity();
        storage.add_component<Animation>(e, Animation{
            start_frame,      // current_frame starts at start_frame
            start_frame,      // start_frame
            frame_count,      // frame_count
            frame_duration,   // frame_duration
            0.0f,             // elapsed
            true,             // looping
            true,             // playing
            false             // finished
        });

        // Apply exactly frame_count advances of frame_duration each
        for (int step = 0; step < frame_count; ++step) {
            blackboard.set<double>("delta_time", static_cast<double>(frame_duration));
            anim_system.update(storage, blackboard);
        }

        auto retrieved = storage.get_component<Animation>(e);
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->get().current_frame == start_frame);
    }
}

// ============================================================================
// Property 4: One-shot animation completion
// Feature: 080-03-animation-system, Property 4: One-shot animation completion
//
// **Validates: Requirements 3.5, 3.7, 6.3, 11.6**
//
// For any valid one-shot Animation configuration (start_frame >= 0,
// frame_count in 1..16, frame_duration in 0.01..1.0), after applying
// frame_count advances of frame_duration each, the Animation's
// current_frame shall equal start_frame + frame_count - 1 (last valid
// frame), finished shall be true, and playing shall be false.
// ============================================================================
TEST_CASE("Property: One-shot animation completion", "[animation_system]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 1000000)));
    std::mt19937 gen(seed);

    std::uniform_int_distribution<int> sf_dist(0, 32);
    std::uniform_int_distribution<int> fc_dist(1, 16);
    std::uniform_real_distribution<float> fd_dist(0.01f, 1.0f);

    for (int inner = 0; inner < NUM_INNER_TESTS; ++inner) {
        EntityManager em;
        ComponentStorage storage;
        Blackboard blackboard;
        AnimationSystem anim_system;

        int start_frame = sf_dist(gen);
        int frame_count = fc_dist(gen);
        float frame_duration = fd_dist(gen);

        Entity e = em.create_entity();
        storage.add_component<Animation>(e, Animation{
            start_frame,      // current_frame starts at start_frame
            start_frame,      // start_frame
            frame_count,      // frame_count
            frame_duration,   // frame_duration
            0.0f,             // elapsed
            false,            // looping = false (one-shot)
            true,             // playing
            false             // finished
        });

        // Apply frame_count advances of frame_duration each
        for (int step = 0; step < frame_count; ++step) {
            blackboard.set<double>("delta_time", static_cast<double>(frame_duration));
            anim_system.update(storage, blackboard);
        }

        auto retrieved = storage.get_component<Animation>(e);
        REQUIRE(retrieved.has_value());

        int expected_last_frame = start_frame + frame_count - 1;
        REQUIRE(retrieved->get().current_frame == expected_last_frame);
        REQUIRE(retrieved->get().finished);
        REQUIRE_FALSE(retrieved->get().playing);
    }
}

// ============================================================================
// Property 5: SpriteSheet synchronization invariant
// Feature: 080-03-animation-system, Property 5: SpriteSheet synchronization invariant
//
// **Validates: Requirements 3.2, 4.1-4.3, 13.1, 13.2**
//
// For any valid Animation and SpriteSheet configuration, after the
// AnimationSystem updates, SpriteSheet.current_frame shall equal
// Animation.current_frame. This shall hold regardless of whether the
// Animation is playing or paused.
// ============================================================================
TEST_CASE("Property: SpriteSheet synchronization invariant", "[animation_system]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 1000000)));
    std::mt19937 gen(seed);

    std::uniform_int_distribution<int> sf_dist(0, 32);
    std::uniform_int_distribution<int> fc_dist(1, 16);
    std::uniform_real_distribution<float> fd_dist(0.01f, 1.0f);
    std::uniform_real_distribution<double> dt_dist(0.001, 0.5);
    std::uniform_int_distribution<int> bool_dist(0, 1);

    for (int inner = 0; inner < NUM_INNER_TESTS; ++inner) {
        EntityManager em;
        ComponentStorage storage;
        Blackboard blackboard;
        AnimationSystem anim_system;

        int start_frame = sf_dist(gen);
        int frame_count = fc_dist(gen);
        float frame_duration = fd_dist(gen);
        bool playing = bool_dist(gen) == 1;

        // Random SpriteSheet config
        std::string atlas_filename = random_string(gen, 5, 15) + ".png";
        int total_frames = start_frame + frame_count;

        Entity e = em.create_entity();
        storage.add_component<Animation>(e, Animation{
            start_frame,      // current_frame starts at start_frame
            start_frame,      // start_frame
            frame_count,      // frame_count
            frame_duration,   // frame_duration
            0.0f,             // elapsed
            true,             // looping
            playing,          // playing (random: true or false)
            false             // finished
        });
        storage.add_component<SpriteSheet>(e, SpriteSheet{
            atlas_filename,   // atlas_filename
            32,               // frame_width
            32,               // frame_height
            4,                // columns
            total_frames,     // total_frames
            0                 // current_frame (will be overwritten by sync)
        });

        // Apply a random delta_time update
        double dt = dt_dist(gen);
        blackboard.set<double>("delta_time", dt);
        anim_system.update(storage, blackboard);

        auto anim_opt = storage.get_component<Animation>(e);
        auto ss_opt = storage.get_component<SpriteSheet>(e);
        REQUIRE(anim_opt.has_value());
        REQUIRE(ss_opt.has_value());
        REQUIRE(ss_opt->get().current_frame == anim_opt->get().current_frame);
    }
}

// ============================================================================
// Property 6: Destruction pipeline removes Animation
// Feature: 080-03-animation-system, Property 6: Destruction pipeline removes Animation
//
// **Validates: Requirements 7.1, 7.2, 14.1**
//
// For any entity that has an Animation component and is marked with
// DestroyRequest, after calling destroy_marked_entities(),
// has_component<Animation>() shall return false for that entity and
// the entity shall not be alive.
// ============================================================================
TEST_CASE("Property: Destruction pipeline removes Animation", "[animation_system]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 1000000)));
    std::mt19937 gen(seed);

    std::uniform_int_distribution<int> sf_dist(0, 32);
    std::uniform_int_distribution<int> fc_dist(1, 16);
    std::uniform_real_distribution<float> fd_dist(0.0f, 1.0f);
    std::uniform_real_distribution<float> el_dist(0.0f, 2.0f);
    std::uniform_int_distribution<int> bool_dist(0, 1);

    EntityManager em;
    ComponentStorage storage;

    int start_frame = sf_dist(gen);
    int frame_count = fc_dist(gen);
    std::uniform_int_distribution<int> cf_dist(0, start_frame + frame_count - 1);
    int current_frame = cf_dist(gen);
    float frame_duration = fd_dist(gen);
    float elapsed = el_dist(gen);
    bool looping = bool_dist(gen) == 1;
    bool playing = bool_dist(gen) == 1;
    bool finished = bool_dist(gen) == 1;

    Entity e = em.create_entity();
    storage.add_component<Animation>(e, Animation{
        current_frame, start_frame, frame_count,
        frame_duration, elapsed, looping, playing, finished
    });
    storage.add_component<DestroyRequest>(e, DestroyRequest{});

    destroy_marked_entities(em, storage);

    REQUIRE_FALSE(storage.has_component<Animation>(e));
    REQUIRE_FALSE(em.is_alive(e));
}
