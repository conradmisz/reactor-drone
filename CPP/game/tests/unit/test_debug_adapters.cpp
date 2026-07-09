/**
 * Unit tests for debug adapter classes (debug_adapters.hpp)
 *
 * Validates: Requirements 1.10, 3.3, 3.4, 4.8, 4.9, 5.3, 5.4
 *
 * Tests cover:
 * - All 26 component types are registered (count from registered_components())
 * - registered_components() returns alphabetically sorted output
 * - Tag components (EnemyTag, TowerTag, etc.) produce empty Value objects
 * - Specific converter: Position{3.0, 4.0} → Object with x=3.0, y=4.0
 * - BlackboardAccessorAdapter throws std::runtime_error on unsupported type
 * - EntityMapperAdapter map() and get_actual_id() round-trip
 * - ComponentBridgeAdapter has() returns false for missing, true for present
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "game/debug_adapters.hpp"
#include "engine/ecs/component_storage.hpp"
#include <memory>
#include <string>

// ===========================================================================
// All 26 component types are registered
// ===========================================================================

TEST_CASE("PropertyAccessorAdapter registers all 26 component types",
          "[Game][debug]") {
    PropertyAccessorAdapter accessor;
    TypeIntrospectorAdapter introspector;
    register_all_components(accessor, introspector);

    auto components = accessor.registered_components();
    REQUIRE(components.size() == 26);
}

// ===========================================================================
// registered_components() returns alphabetically sorted output
// ===========================================================================

TEST_CASE("registered_components() returns alphabetically sorted output",
          "[Game][debug]") {
    PropertyAccessorAdapter accessor;
    TypeIntrospectorAdapter introspector;
    register_all_components(accessor, introspector);

    auto components = accessor.registered_components();
    for (size_t i = 1; i < components.size(); ++i) {
        REQUIRE(components[i - 1].first < components[i].first);
    }

    // Verify first and last entries
    REQUIRE(components.front().first == "Animation");
    REQUIRE(components.back().first == "WrapAround");
}

// ===========================================================================
// Tag components produce empty Value objects via cpp_to_value
// ===========================================================================

TEST_CASE("Tag components produce empty Value objects",
          "[Game][debug]") {
    PropertyAccessorAdapter accessor;
    TypeIntrospectorAdapter introspector;
    register_all_components(accessor, introspector);

    SECTION("EnemyTag produces empty object") {
        EnemyTag tag{};
        std::any val = tag;
        Value result = introspector.cpp_to_value(val, "EnemyTag");
        REQUIRE(result.type == Value::Type::Object);
        auto& obj = std::get<std::unordered_map<std::string, std::unique_ptr<Value>>>(result.data);
        REQUIRE(obj.empty());
    }

    SECTION("TowerTag produces empty object") {
        TowerTag tag{};
        std::any val = tag;
        Value result = introspector.cpp_to_value(val, "TowerTag");
        REQUIRE(result.type == Value::Type::Object);
        auto& obj = std::get<std::unordered_map<std::string, std::unique_ptr<Value>>>(result.data);
        REQUIRE(obj.empty());
    }

    SECTION("ProjectileTag produces empty object") {
        ProjectileTag tag{};
        std::any val = tag;
        Value result = introspector.cpp_to_value(val, "ProjectileTag");
        REQUIRE(result.type == Value::Type::Object);
        auto& obj = std::get<std::unordered_map<std::string, std::unique_ptr<Value>>>(result.data);
        REQUIRE(obj.empty());
    }

    SECTION("DestroyRequest produces empty object") {
        DestroyRequest tag{};
        std::any val = tag;
        Value result = introspector.cpp_to_value(val, "DestroyRequest");
        REQUIRE(result.type == Value::Type::Object);
        auto& obj = std::get<std::unordered_map<std::string, std::unique_ptr<Value>>>(result.data);
        REQUIRE(obj.empty());
    }

    SECTION("WrapAround produces empty object") {
        WrapAround tag{};
        std::any val = tag;
        Value result = introspector.cpp_to_value(val, "WrapAround");
        REQUIRE(result.type == Value::Type::Object);
        auto& obj = std::get<std::unordered_map<std::string, std::unique_ptr<Value>>>(result.data);
        REQUIRE(obj.empty());
    }
}

// ===========================================================================
// Specific converter: Position{3.0, 4.0} → Object with x=3.0, y=4.0
// ===========================================================================

TEST_CASE("Position converter produces correct Object value",
          "[Game][debug]") {
    PropertyAccessorAdapter accessor;
    TypeIntrospectorAdapter introspector;
    register_all_components(accessor, introspector);

    Position pos{3.0f, 4.0f};
    std::any val = pos;
    Value result = introspector.cpp_to_value(val, "Position");

    REQUIRE(result.type == Value::Type::Object);
    auto& obj = std::get<std::unordered_map<std::string, std::unique_ptr<Value>>>(result.data);
    REQUIRE(obj.size() == 2);
    REQUIRE(obj.count("x") == 1);
    REQUIRE(obj.count("y") == 1);
    REQUIRE(obj.at("x")->type == Value::Type::Float);
    REQUIRE(obj.at("y")->type == Value::Type::Float);
    REQUIRE_THAT(std::get<double>(obj.at("x")->data),
                 Catch::Matchers::WithinRel(3.0, 1e-6));
    REQUIRE_THAT(std::get<double>(obj.at("y")->data),
                 Catch::Matchers::WithinRel(4.0, 1e-6));
}

// ===========================================================================
// BlackboardAccessorAdapter throws std::runtime_error on unsupported type
// ===========================================================================

TEST_CASE("BlackboardAccessorAdapter throws on unsupported type",
          "[Game][debug]") {
    Blackboard bb;
    // Store a shared_ptr which is not a supported type
    bb.set<std::shared_ptr<int>>("unsupported", std::make_shared<int>(42));

    BlackboardAccessorAdapter adapter(bb);
    REQUIRE_THROWS_AS(adapter.get_value("unsupported"), std::runtime_error);
}

// ===========================================================================
// EntityMapperAdapter map() and get_actual_id() round-trip
// ===========================================================================

TEST_CASE("EntityMapperAdapter map and get_actual_id round-trip",
          "[Game][debug]") {
    EntityMapperAdapter mapper;

    mapper.map(0, 100);
    mapper.map(1, 200);
    mapper.map(2, 300);

    REQUIRE(mapper.get_actual_id(0) == 100);
    REQUIRE(mapper.get_actual_id(1) == 200);
    REQUIRE(mapper.get_actual_id(2) == 300);

    auto ids = mapper.get_logical_ids();
    REQUIRE(ids.size() == 3);
}

// ===========================================================================
// ComponentBridgeAdapter has() returns false for missing, true for present
// ===========================================================================

TEST_CASE("ComponentBridgeAdapter has() returns correct values",
          "[Game][debug]") {
    ComponentStorage cs;
    ComponentBridgeAdapter<Position> bridge;

    Entity e1 = 1;
    Entity e2 = 2;

    // e1 has no Position yet
    REQUIRE(bridge.has(e1, cs) == false);

    // Add Position to e1
    cs.add_component<Position>(e1, Position{10.0f, 20.0f});
    REQUIRE(bridge.has(e1, cs) == true);

    // e2 still has no Position
    REQUIRE(bridge.has(e2, cs) == false);

    SECTION("get_as_any returns value for present component") {
        auto result = bridge.get_as_any(e1, cs);
        REQUIRE(result.has_value());
    }

    SECTION("get_as_any returns nullopt for missing component") {
        auto result = bridge.get_as_any(e2, cs);
        REQUIRE_FALSE(result.has_value());
    }
}
