/**
 * Unit tests for Animation component and AnimationSystem
 *
 * These tests verify:
 * - Default Animation field values
 * - Single frame advance with known timing
 * - Looping wrap-around after full sequence
 * - One-shot completion (finished=true, playing=false)
 * - Paused animation skips timing advancement
 * - Zero frame_duration keeps frame static
 * - SpriteSheet synchronization after update
 * - Animation component storage operations (add, get, has, remove, entities_with)
 *
 * Requirements tested: 1.1–1.8, 9.1, 9.2, 9.3, 9.4, 9.5, 9.6,
 *                      10.1, 10.2, 10.3, 10.4
 */

#include <catch2/catch_test_macros.hpp>
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/systems/animation_system.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

// -----------------------------------------------------------------------
// 1. Default-constructed Animation has correct defaults for all 8 fields
// Validates: Requirements 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8
// -----------------------------------------------------------------------
TEST_CASE("DefaultFieldValues", "[animation]") {
    Animation anim{};
    CHECK(anim.current_frame == 0);
    CHECK(anim.start_frame == 0);
    CHECK(anim.frame_count == 1);
    CHECK(std::fabs(anim.frame_duration - 0.1f) < 1e-6f);
    CHECK(std::fabs(anim.elapsed - 0.0f) < 1e-6f);
    CHECK(anim.looping == true);
    CHECK(anim.playing == true);
    CHECK(anim.finished == false);
}

// -----------------------------------------------------------------------
// 2. Single frame advance: dt=0.1 with frame_duration=0.1 → frame 0→1
// Validates: Requirement 9.1
// -----------------------------------------------------------------------
TEST_CASE("SingleFrameAdvance", "[animation]") {
    EntityManager entity_manager;
    ComponentStorage storage;
    Blackboard blackboard;
    AnimationSystem animation_system;

    Entity e = entity_manager.create_entity();
    storage.add_component<Animation>(e, Animation{0, 0, 4, 0.1f, 0.0f, true, true, false});
    storage.add_component<SpriteSheet>(e, SpriteSheet{"atlas.png", 64, 64, 8, 48, 0});

    blackboard.set<double>("delta_time", 0.1);
    animation_system.update(storage, blackboard);

    auto anim = storage.get_component<Animation>(e);
    REQUIRE(anim.has_value());
    CHECK(anim->get().current_frame == 1);
}

// -----------------------------------------------------------------------
// 3. Looping wrap: start=0, count=4, 4 advances of dt=0.1 → wraps to 0
// Validates: Requirement 9.2
// -----------------------------------------------------------------------
TEST_CASE("LoopingWrap", "[animation]") {
    EntityManager entity_manager;
    ComponentStorage storage;
    Blackboard blackboard;
    AnimationSystem animation_system;

    Entity e = entity_manager.create_entity();
    storage.add_component<Animation>(e, Animation{0, 0, 4, 0.1f, 0.0f, true, true, false});
    storage.add_component<SpriteSheet>(e, SpriteSheet{"atlas.png", 64, 64, 8, 48, 0});

    blackboard.set<double>("delta_time", 0.1);
    for (int i = 0; i < 4; ++i) {
        animation_system.update(storage, blackboard);
    }

    auto anim = storage.get_component<Animation>(e);
    REQUIRE(anim.has_value());
    CHECK(anim->get().current_frame == 0);
}

// -----------------------------------------------------------------------
// 4. One-shot completion: start=0, count=3, looping=false, 3 advances
//    → frame=2, finished=true, playing=false
// Validates: Requirement 9.3
// -----------------------------------------------------------------------
TEST_CASE("OneShotCompletion", "[animation]") {
    EntityManager entity_manager;
    ComponentStorage storage;
    Blackboard blackboard;
    AnimationSystem animation_system;

    Entity e = entity_manager.create_entity();
    storage.add_component<Animation>(e, Animation{0, 0, 3, 0.1f, 0.0f, false, true, false});
    storage.add_component<SpriteSheet>(e, SpriteSheet{"atlas.png", 64, 64, 8, 48, 0});

    blackboard.set<double>("delta_time", 0.1);
    for (int i = 0; i < 3; ++i) {
        animation_system.update(storage, blackboard);
    }

    auto anim = storage.get_component<Animation>(e);
    REQUIRE(anim.has_value());
    CHECK(anim->get().current_frame == 2);
    CHECK(anim->get().finished == true);
    CHECK(anim->get().playing == false);
}

// -----------------------------------------------------------------------
// 5. Paused skip: playing=false, dt applied → no field changes
// Validates: Requirement 9.4
// -----------------------------------------------------------------------
TEST_CASE("PausedSkip", "[animation]") {
    EntityManager entity_manager;
    ComponentStorage storage;
    Blackboard blackboard;
    AnimationSystem animation_system;

    Entity e = entity_manager.create_entity();
    storage.add_component<Animation>(e, Animation{1, 0, 4, 0.1f, 0.05f, true, false, false});
    storage.add_component<SpriteSheet>(e, SpriteSheet{"atlas.png", 64, 64, 8, 48, 0});

    blackboard.set<double>("delta_time", 0.1);
    animation_system.update(storage, blackboard);

    auto anim = storage.get_component<Animation>(e);
    REQUIRE(anim.has_value());
    CHECK(anim->get().current_frame == 1);
    CHECK(std::fabs(anim->get().elapsed - 0.05f) < 1e-6f);
    CHECK(anim->get().finished == false);
}

// -----------------------------------------------------------------------
// 6. Zero frame_duration: dt applied → elapsed increases, frame unchanged
// Validates: Requirement 9.5
// -----------------------------------------------------------------------
TEST_CASE("ZeroFrameDuration", "[animation]") {
    EntityManager entity_manager;
    ComponentStorage storage;
    Blackboard blackboard;
    AnimationSystem animation_system;

    Entity e = entity_manager.create_entity();
    storage.add_component<Animation>(e, Animation{0, 0, 4, 0.0f, 0.0f, true, true, false});
    storage.add_component<SpriteSheet>(e, SpriteSheet{"atlas.png", 64, 64, 8, 48, 0});

    blackboard.set<double>("delta_time", 0.1);
    animation_system.update(storage, blackboard);

    auto anim = storage.get_component<Animation>(e);
    REQUIRE(anim.has_value());
    CHECK(anim->get().current_frame == 0);
    CHECK(anim->get().elapsed > 0.0f);
}

// -----------------------------------------------------------------------
// 7. SpriteSheet sync: after update, SpriteSheet.current_frame matches
//    Animation.current_frame
// Validates: Requirement 9.6
// -----------------------------------------------------------------------
TEST_CASE("SpriteSheetSync", "[animation]") {
    EntityManager entity_manager;
    ComponentStorage storage;
    Blackboard blackboard;
    AnimationSystem animation_system;

    Entity e = entity_manager.create_entity();
    storage.add_component<Animation>(e, Animation{0, 0, 4, 0.1f, 0.0f, true, true, false});
    storage.add_component<SpriteSheet>(e, SpriteSheet{"atlas.png", 64, 64, 8, 48, 0});

    blackboard.set<double>("delta_time", 0.1);
    animation_system.update(storage, blackboard);

    auto anim = storage.get_component<Animation>(e);
    auto ss = storage.get_component<SpriteSheet>(e);
    REQUIRE(anim.has_value());
    REQUIRE(ss.has_value());
    CHECK(ss->get().current_frame == anim->get().current_frame);
}

// -----------------------------------------------------------------------
// 8. add_component<Animation>() then has_component returns true
// Validates: Requirement 10.1
// -----------------------------------------------------------------------
TEST_CASE("ComponentAddHas", "[animation]") {
    EntityManager entity_manager;
    ComponentStorage storage;

    Entity e = entity_manager.create_entity();
    storage.add_component<Animation>(e, Animation{});

    CHECK(storage.has_component<Animation>(e) == true);
}

// -----------------------------------------------------------------------
// 9. add then get returns matching 8 fields
// Validates: Requirement 10.2
// -----------------------------------------------------------------------
TEST_CASE("ComponentRoundTrip", "[animation]") {
    EntityManager entity_manager;
    ComponentStorage storage;

    Entity e = entity_manager.create_entity();
    Animation original{3, 2, 8, 0.25f, 0.05f, false, true, false};
    storage.add_component<Animation>(e, original);

    auto retrieved = storage.get_component<Animation>(e);
    REQUIRE(retrieved.has_value());

    const auto& anim = retrieved->get();
    CHECK(anim.current_frame == 3);
    CHECK(anim.start_frame == 2);
    CHECK(anim.frame_count == 8);
    CHECK(std::fabs(anim.frame_duration - 0.25f) < 1e-6f);
    CHECK(std::fabs(anim.elapsed - 0.05f) < 1e-6f);
    CHECK(anim.looping == false);
    CHECK(anim.playing == true);
    CHECK(anim.finished == false);
}

// -----------------------------------------------------------------------
// 10. remove then has_component returns false
// Validates: Requirement 10.3
// -----------------------------------------------------------------------
TEST_CASE("ComponentRemove", "[animation]") {
    EntityManager entity_manager;
    ComponentStorage storage;

    Entity e = entity_manager.create_entity();
    storage.add_component<Animation>(e, Animation{});
    REQUIRE(storage.has_component<Animation>(e) == true);

    storage.remove_component<Animation>(e);
    CHECK(storage.has_component<Animation>(e) == false);
}

// -----------------------------------------------------------------------
// 11. entities_with_component returns all expected IDs
// Validates: Requirement 10.4
// -----------------------------------------------------------------------
TEST_CASE("EntitiesWithComponent", "[animation]") {
    EntityManager entity_manager;
    ComponentStorage storage;

    Entity e1 = entity_manager.create_entity();
    Entity e2 = entity_manager.create_entity();
    Entity e3 = entity_manager.create_entity();
    CHECK_FALSE(storage.has_component<Animation>(e2));

    storage.add_component<Animation>(e1, Animation{});
    storage.add_component<Animation>(e3, Animation{0, 0, 4, 0.1f, 0.0f, true, true, false});
    // e2 intentionally has no Animation

    std::vector<Entity> entities = storage.entities_with_component<Animation>();

    CHECK(entities.size() == 2);

    // Sort for deterministic comparison (unordered_map iteration order is unspecified)
    std::sort(entities.begin(), entities.end());
    std::vector<Entity> expected = {e1, e3};
    std::sort(expected.begin(), expected.end());

    CHECK(entities == expected);
}
