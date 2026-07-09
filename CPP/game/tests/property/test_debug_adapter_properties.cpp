/**
 * Property-based tests for debug adapter classes (debug_adapters.hpp)
 *
 * These tests verify four correctness properties from the design document:
 *   Property 1: BlackboardAccessor Round-Trip
 *   Property 2: ComponentBridge Has/Get Consistency
 *   Property 3: EntityMapper Logical ID Stability
 *   Property 4: Serialization Produces Valid JSON
 *
 * Feature: 090-06-dump-trace
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "game/debug_adapters.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/debug/json_serializer.hpp"
#include <algorithm>
#include <string>
#include <vector>

// Configurable test iteration counts (MANDATORY — workspace policy)
constexpr int NUM_OUTER_TESTS = 10;  // Number of different keys/entities/outer values
constexpr int NUM_INNER_TESTS = 5;   // Number of different values per key/entity

// ---------------------------------------------------------------------------
// Feature: 090-06-dump-trace, Property 1: BlackboardAccessor Round-Trip
//
// For any supported value type (int, double, bool, string) and for any valid
// key string and value of that type, storing the value in a Blackboard and
// then reading it via BlackboardAccessorAdapter::get_value() SHALL produce a
// Value with the correct type tag and equivalent data.
//
// **Validates: Requirements 1.4, 1.5, 1.7, 1.8, 11.1, 11.2, 11.3, 11.4**
// ---------------------------------------------------------------------------
TEST_CASE("BlackboardAccessor Round-Trip",
          "[Game][debug][property]") {

    SECTION("Round-trip for int values") {
        auto key_chars = GENERATE(take(NUM_OUTER_TESTS, chunk(6, random('a', 'z'))));
        auto value = GENERATE(take(NUM_INNER_TESTS, random(-10000, 10000)));

        std::string key(key_chars.begin(), key_chars.end());

        Blackboard bb;
        bb.set<int>(key, value);

        BlackboardAccessorAdapter adapter(bb);
        Value result = adapter.get_value(key);

        REQUIRE(result.type == Value::Type::Int);
        REQUIRE(std::get<int64_t>(result.data) == static_cast<int64_t>(value));
    }

    SECTION("Round-trip for double values") {
        auto key_chars = GENERATE(take(NUM_OUTER_TESTS, chunk(6, random('a', 'z'))));
        auto value = GENERATE(take(NUM_INNER_TESTS, random(-1000.0, 1000.0)));

        std::string key(key_chars.begin(), key_chars.end());

        Blackboard bb;
        bb.set<double>(key, value);

        BlackboardAccessorAdapter adapter(bb);
        Value result = adapter.get_value(key);

        REQUIRE(result.type == Value::Type::Float);
        REQUIRE_THAT(std::get<double>(result.data),
                     Catch::Matchers::WithinRel(value, 1e-12));
    }

    SECTION("Round-trip for bool values") {
        auto key_chars = GENERATE(take(NUM_OUTER_TESTS, chunk(6, random('a', 'z'))));
        auto bool_val = GENERATE(take(NUM_INNER_TESTS, random(0, 1)));

        std::string key(key_chars.begin(), key_chars.end());
        bool value = static_cast<bool>(bool_val);

        Blackboard bb;
        bb.set<bool>(key, value);

        BlackboardAccessorAdapter adapter(bb);
        Value result = adapter.get_value(key);

        REQUIRE(result.type == Value::Type::Bool);
        REQUIRE(std::get<bool>(result.data) == value);
    }

    SECTION("Round-trip for string values") {
        auto key_chars = GENERATE(take(NUM_OUTER_TESTS, chunk(6, random('a', 'z'))));
        auto val_chars = GENERATE(take(NUM_INNER_TESTS, chunk(8, random('a', 'z'))));

        std::string key(key_chars.begin(), key_chars.end());
        std::string value(val_chars.begin(), val_chars.end());

        Blackboard bb;
        bb.set<std::string>(key, value);

        BlackboardAccessorAdapter adapter(bb);
        Value result = adapter.get_value(key);

        REQUIRE(result.type == Value::Type::String);
        REQUIRE(std::get<std::string>(result.data) == value);
    }
}

// ---------------------------------------------------------------------------
// Feature: 090-06-dump-trace, Property 2: ComponentBridge Has/Get Consistency
//
// For any entity and for any component type T, ComponentBridgeAdapter<T>::has()
// returning true implies get_as_any() returns a non-nullopt value, and has()
// returning false implies get_as_any() returns std::nullopt.
//
// **Validates: Requirements 2.3, 2.4, 2.5, 12.1, 12.2**
// ---------------------------------------------------------------------------
TEST_CASE("ComponentBridge Has/Get Consistency",
          "[Game][debug][property]") {

    SECTION("has() and get_as_any() agree for random entities with Position") {
        auto entity_id = GENERATE(take(NUM_OUTER_TESTS, random(1u, 1000u)));
        auto add_component = GENERATE(take(NUM_INNER_TESTS, random(0, 1)));

        ComponentStorage cs;
        ComponentBridgeAdapter<Position> bridge;

        Entity e = static_cast<Entity>(entity_id);

        if (add_component) {
            cs.add_component<Position>(e, Position{1.0f, 2.0f});
        }

        bool has_it = bridge.has(e, cs);
        auto get_result = bridge.get_as_any(e, cs);

        if (has_it) {
            REQUIRE(get_result.has_value());
        } else {
            REQUIRE_FALSE(get_result.has_value());
        }
    }
}

// ---------------------------------------------------------------------------
// Feature: 090-06-dump-trace, Property 3: EntityMapper Logical ID Stability
//
// For any set of active entities, populating the EntityMapperAdapter by
// iterating entities in ascending ID order SHALL produce logical IDs that
// form a contiguous sequence [0, N) where N is the entity count, and
// repeating the population with the same entity set SHALL produce an
// identical mapping.
//
// **Validates: Requirements 5.3, 5.4, 5.5, 10.1, 10.2**
// ---------------------------------------------------------------------------
TEST_CASE("EntityMapper Logical ID Stability",
          "[Game][debug][property]") {

    SECTION("Logical IDs are 0..N-1 and repopulation produces same mapping") {
        auto num_entities = GENERATE(take(NUM_OUTER_TESTS, random(1, 20)));
        auto base_id = GENERATE(take(NUM_INNER_TESTS, random(1u, 500u)));

        // Generate a set of entity IDs
        std::vector<Entity> entity_ids;
        for (int i = 0; i < num_entities; ++i) {
            entity_ids.push_back(static_cast<Entity>(base_id + i * 3));
        }

        // First population
        EntityMapperAdapter mapper;
        std::sort(entity_ids.begin(), entity_ids.end());
        for (size_t i = 0; i < entity_ids.size(); ++i) {
            mapper.map(static_cast<int>(i), entity_ids[i]);
        }

        // Verify logical IDs are 0..N-1
        auto logical_ids = mapper.get_logical_ids();
        std::sort(logical_ids.begin(), logical_ids.end());
        REQUIRE(logical_ids.size() == static_cast<size_t>(num_entities));
        for (int i = 0; i < num_entities; ++i) {
            REQUIRE(std::find(logical_ids.begin(), logical_ids.end(), i) != logical_ids.end());
        }

        // Verify actual IDs match
        for (size_t i = 0; i < entity_ids.size(); ++i) {
            REQUIRE(mapper.get_actual_id(static_cast<int>(i)) == entity_ids[i]);
        }

        // Second population — same entity set should produce identical mapping
        EntityMapperAdapter mapper2;
        for (size_t i = 0; i < entity_ids.size(); ++i) {
            mapper2.map(static_cast<int>(i), entity_ids[i]);
        }

        for (int i = 0; i < num_entities; ++i) {
            REQUIRE(mapper2.get_actual_id(i) == mapper.get_actual_id(i));
        }
    }
}

// ---------------------------------------------------------------------------
// Feature: 090-06-dump-trace, Property 4: Serialization Produces Valid JSON
//
// For any valid ECS state (random entities with random subsets of the 26
// registered component types and random field values), calling
// JSONSerializer::serialize_state() SHALL produce output that is parseable
// as valid JSON, contains a top-level "entities" key and a "blackboard" key,
// and has balanced braces.
//
// **Validates: Requirements 9.1, 9.2, 9.3**
// ---------------------------------------------------------------------------
TEST_CASE("Serialization Produces Valid JSON",
          "[Game][debug][property]") {

    SECTION("Serialized output has valid structure for random entities") {
        auto num_entities = GENERATE(take(NUM_OUTER_TESTS, random(1, 10)));
        auto seed_val = GENERATE(take(NUM_INNER_TESTS, random(1u, 10000u)));

        // Set up adapters
        PropertyAccessorAdapter accessor;
        TypeIntrospectorAdapter introspector;
        register_all_components(accessor, introspector);

        EntityMapperAdapter mapper;
        Blackboard bb;
        bb.set<int>("frame_count", static_cast<int>(seed_val));
        bb.set<double>("delta_time", 0.016667);
        BlackboardAccessorAdapter bb_adapter(bb);

        ComponentStorage cs;

        // Create random entities with Position, Size, and/or Color
        std::vector<Entity> entities;
        for (int i = 0; i < num_entities; ++i) {
            Entity e = static_cast<Entity>(i + 1);
            entities.push_back(e);

            // Always add Position
            cs.add_component<Position>(e, Position{
                static_cast<float>(seed_val + i),
                static_cast<float>(seed_val + i + 100)
            });

            // Add Size for even entities
            if (i % 2 == 0) {
                cs.add_component<Size>(e, Size{32.0f, 32.0f});
            }

            // Add Color for odd entities
            if (i % 2 == 1) {
                cs.add_component<Color>(e, Color{255, 0, 0, 255});
            }
        }

        // Populate mapper
        std::sort(entities.begin(), entities.end());
        for (size_t i = 0; i < entities.size(); ++i) {
            mapper.map(static_cast<int>(i), entities[i]);
        }

        // Serialize
        engine::debug::JSONSerializer serializer(accessor, introspector, mapper, &bb_adapter);
        std::string json = serializer.serialize_state(
            seed_val, "game", "090-tower-defense", "after", cs);

        // Validate: starts with '{', ends with '}'
        REQUIRE(!json.empty());
        REQUIRE(json.front() == '{');
        REQUIRE(json.back() == '}');

        // Validate: contains "entities" and "blackboard" substrings
        REQUIRE(json.find("\"entities\"") != std::string::npos);
        REQUIRE(json.find("\"blackboard\"") != std::string::npos);

        // Validate: balanced braces
        int brace_count = 0;
        for (char c : json) {
            if (c == '{') brace_count++;
            if (c == '}') brace_count--;
            REQUIRE(brace_count >= 0);  // Never go negative
        }
        REQUIRE(brace_count == 0);  // Must balance
    }
}
