/**
 * Property-based tests for Lua API bindings.
 *
 * These tests verify universal properties for the engine.* Lua API:
 * component round-trips, entity creation uniqueness, math helper invariants.
 *
 * Each test creates a fresh LuaManager, EntityManager, ComponentStorage,
 * and Blackboard, then calls register_bindings and store_engine_pointers
 * before exercising bindings via execute_string().
 *
 * The Blackboard is used as a bridge between Lua and C++ for verification:
 * Lua scripts write results to the blackboard, and C++ reads them back.
 *
 * Testing Framework: Catch2 v3
 * Constants: NUM_OUTER_TESTS = 10, NUM_INNER_TESTS = 5
 *
 * Requirements tested: 3.1, 3.2, 4.1, 4.2, 9.1, 9.5, 10.1, 10.2,
 *                      15.1, 16.1, 21.1–21.8
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>
#include <catch2/catch_approx.hpp>
#include "engine/lua_manager.hpp"
#include "engine/lua_bindings.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"
#include <random>
#include <cmath>
#include <string>
#include <set>
#include <sstream>
#include <iomanip>
#include <limits>

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
// Property 1: Position binding round-trip
// Feature: 070-03-lua-api-bindings, Property 1: Position binding round-trip
//
// **Validates: Requirements 3.1, 3.2, 21.2**
//
// For any entity and for any float values x and y, calling
// engine.set_position(id, x, y) followed by engine.get_position(id)
// shall return the same x and y values.
// ============================================================================
TEST_CASE("Property: Position binding round-trip", "[lua_bindings]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 1000000)));
    std::mt19937 gen(seed);
    std::uniform_real_distribution<double> coord_dist(-10000.0, 10000.0);

    for (int inner = 0; inner < NUM_INNER_TESTS; ++inner) {
        LuaManager lua_manager;
        EntityManager entity_manager;
        ComponentStorage storage;
        Blackboard blackboard;

        register_bindings(lua_manager.state());
        store_engine_pointers(lua_manager.state(), &storage, &entity_manager, &blackboard);

        Entity e = entity_manager.create_entity();
        double x = coord_dist(gen);
        double y = coord_dist(gen);

        // Use Lua to set position, then get it back via blackboard
        std::ostringstream lua_code;
        lua_code << "engine.set_position(" << e << ", " << x << ", " << y << ")\n"
                 << "local rx, ry = engine.get_position(" << e << ")\n"
                 << "engine.set_blackboard('result_x', rx)\n"
                 << "engine.set_blackboard('result_y', ry)\n";

        auto result = lua_manager.execute_string(lua_code.str());
        REQUIRE(result.success);

        double result_x = blackboard.get<double>("result_x");
        double result_y = blackboard.get<double>("result_y");

        // Position uses float internally, so compare with float precision
        REQUIRE(result_x == Catch::Approx(static_cast<float>(x)).margin(1e-4));
        REQUIRE(result_y == Catch::Approx(static_cast<float>(y)).margin(1e-4));
    }
}

// ============================================================================
// Property 2: Velocity binding round-trip
// Feature: 070-03-lua-api-bindings, Property 2: Velocity binding round-trip
//
// **Validates: Requirements 4.1, 4.2, 21.3**
//
// For any entity and for any float values dx and dy, calling
// engine.set_velocity(id, dx, dy) followed by engine.get_velocity(id)
// shall return the same dx and dy values.
// ============================================================================
TEST_CASE("Property: Velocity binding round-trip", "[lua_bindings]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 1000000)));
    std::mt19937 gen(seed);
    std::uniform_real_distribution<double> vel_dist(-5000.0, 5000.0);

    for (int inner = 0; inner < NUM_INNER_TESTS; ++inner) {
        LuaManager lua_manager;
        EntityManager entity_manager;
        ComponentStorage storage;
        Blackboard blackboard;

        register_bindings(lua_manager.state());
        store_engine_pointers(lua_manager.state(), &storage, &entity_manager, &blackboard);

        Entity e = entity_manager.create_entity();
        double dx = vel_dist(gen);
        double dy = vel_dist(gen);

        std::ostringstream lua_code;
        lua_code << "engine.set_velocity(" << e << ", " << dx << ", " << dy << ")\n"
                 << "local rdx, rdy = engine.get_velocity(" << e << ")\n"
                 << "engine.set_blackboard('result_dx', rdx)\n"
                 << "engine.set_blackboard('result_dy', rdy)\n";

        auto result = lua_manager.execute_string(lua_code.str());
        REQUIRE(result.success);

        double result_dx = blackboard.get<double>("result_dx");
        double result_dy = blackboard.get<double>("result_dy");

        REQUIRE(result_dx == Catch::Approx(static_cast<float>(dx)).margin(1e-4));
        REQUIRE(result_dy == Catch::Approx(static_cast<float>(dy)).margin(1e-4));
    }
}

// ============================================================================
// Property 3: Blackboard binding round-trip
// Feature: 070-03-lua-api-bindings, Property 3: Blackboard binding round-trip
//
// **Validates: Requirements 9.1, 9.5, 21.4**
//
// For any random key string and for any double value, calling
// engine.set_blackboard(key, value) followed by engine.get_blackboard(key)
// shall return the same value.
// ============================================================================
TEST_CASE("Property: Blackboard binding round-trip", "[lua_bindings]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 1000000)));
    std::mt19937 gen(seed);
    std::uniform_real_distribution<double> val_dist(-100000.0, 100000.0);

    for (int inner = 0; inner < NUM_INNER_TESTS; ++inner) {
        LuaManager lua_manager;
        EntityManager entity_manager;
        ComponentStorage storage;
        Blackboard blackboard;

        register_bindings(lua_manager.state());
        store_engine_pointers(lua_manager.state(), &storage, &entity_manager, &blackboard);

        std::string key = random_string(gen, 3, 15);
        double value = val_dist(gen);

        // Set via Lua, then get via Lua and write result to a known key
        std::ostringstream lua_code;
        lua_code << std::setprecision(std::numeric_limits<double>::max_digits10)
                 << "engine.set_blackboard('" << key << "', " << value << ")\n"
                 << "local v = engine.get_blackboard('" << key << "')\n"
                 << "engine.set_blackboard('roundtrip_result', v)\n";

        auto result = lua_manager.execute_string(lua_code.str());
        REQUIRE(result.success);

        double roundtrip = blackboard.get<double>("roundtrip_result");
        REQUIRE(roundtrip == Catch::Approx(value).epsilon(1e-9));
    }
}

// ============================================================================
// Property 4: create_entity returns unique, alive entity IDs
// Feature: 070-03-lua-api-bindings, Property 4: create_entity unique alive
//
// **Validates: Requirements 10.1, 10.2, 21.5**
//
// For any sequence of 5 calls to engine.create_entity(), all returned
// entity IDs shall be distinct from each other, and each ID shall be
// alive in the EntityManager.
// ============================================================================
TEST_CASE("Property: create_entity returns unique alive entity IDs", "[lua_bindings]") {
    auto _iter = GENERATE(take(NUM_OUTER_TESTS, random(0, 1)));
    (void)_iter;

    LuaManager lua_manager;
    EntityManager entity_manager;
    ComponentStorage storage;
    Blackboard blackboard;

    register_bindings(lua_manager.state());
    store_engine_pointers(lua_manager.state(), &storage, &entity_manager, &blackboard);

    // Create 5 entities via Lua and store their IDs in the blackboard
    std::ostringstream lua_code;
    lua_code << "local ids = {}\n"
             << "for i = 1, 5 do\n"
             << "  ids[i] = engine.create_entity()\n"
             << "  engine.set_blackboard('entity_' .. i, ids[i])\n"
             << "end\n";

    auto result = lua_manager.execute_string(lua_code.str());
    REQUIRE(result.success);

    // Retrieve IDs and verify uniqueness + alive
    std::set<Entity> seen;
    for (int i = 1; i <= 5; ++i) {
        std::string bb_key = "entity_" + std::to_string(i);
        double id_double = blackboard.get<double>(bb_key);
        Entity id = static_cast<Entity>(id_double);

        REQUIRE(entity_manager.is_alive(id));

        auto [_, inserted] = seen.insert(id);
        REQUIRE(inserted);
    }
}

// ============================================================================
// Property 5: direction_from_angle produces unit vectors
// Feature: 070-03-lua-api-bindings, Property 5: direction_from_angle unit vector
//
// **Validates: Requirements 15.1, 21.6**
//
// For any angle in radians, calling engine.direction_from_angle(angle)
// shall return (dx, dy) such that dx² + dy² ≈ 1.0 within floating-point
// tolerance.
// ============================================================================
TEST_CASE("Property: direction_from_angle produces unit vectors", "[lua_bindings]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 1000000)));
    std::mt19937 gen(seed);
    std::uniform_real_distribution<double> angle_dist(-100.0, 100.0);

    for (int inner = 0; inner < NUM_INNER_TESTS; ++inner) {
        LuaManager lua_manager;
        EntityManager entity_manager;
        ComponentStorage storage;
        Blackboard blackboard;

        register_bindings(lua_manager.state());
        store_engine_pointers(lua_manager.state(), &storage, &entity_manager, &blackboard);

        double angle = angle_dist(gen);

        std::ostringstream lua_code;
        lua_code << "local dx, dy = engine.direction_from_angle(" << angle << ")\n"
                 << "engine.set_blackboard('dir_dx', dx)\n"
                 << "engine.set_blackboard('dir_dy', dy)\n";

        auto result = lua_manager.execute_string(lua_code.str());
        REQUIRE(result.success);

        double dx = blackboard.get<double>("dir_dx");
        double dy = blackboard.get<double>("dir_dy");
        double magnitude_sq = dx * dx + dy * dy;

        REQUIRE(magnitude_sq == Catch::Approx(1.0).margin(1e-9));
    }
}

// ============================================================================
// Property 6: distance is symmetric
// Feature: 070-03-lua-api-bindings, Property 6: distance symmetric
//
// **Validates: Requirements 16.1, 21.7**
//
// For any two points (x1, y1) and (x2, y2),
// engine.distance(x1,y1,x2,y2) == engine.distance(x2,y2,x1,y1).
// ============================================================================
TEST_CASE("Property: distance is symmetric", "[lua_bindings]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 1000000)));
    std::mt19937 gen(seed);
    std::uniform_real_distribution<double> coord_dist(-10000.0, 10000.0);

    for (int inner = 0; inner < NUM_INNER_TESTS; ++inner) {
        LuaManager lua_manager;
        EntityManager entity_manager;
        ComponentStorage storage;
        Blackboard blackboard;

        register_bindings(lua_manager.state());
        store_engine_pointers(lua_manager.state(), &storage, &entity_manager, &blackboard);

        double x1 = coord_dist(gen);
        double y1 = coord_dist(gen);
        double x2 = coord_dist(gen);
        double y2 = coord_dist(gen);

        std::ostringstream lua_code;
        lua_code << "local d1 = engine.distance(" << x1 << ", " << y1 << ", " << x2 << ", " << y2 << ")\n"
                 << "local d2 = engine.distance(" << x2 << ", " << y2 << ", " << x1 << ", " << y1 << ")\n"
                 << "engine.set_blackboard('dist_forward', d1)\n"
                 << "engine.set_blackboard('dist_reverse', d2)\n";

        auto result = lua_manager.execute_string(lua_code.str());
        REQUIRE(result.success);

        double d_forward = blackboard.get<double>("dist_forward");
        double d_reverse = blackboard.get<double>("dist_reverse");

        REQUIRE(d_forward == d_reverse);
    }
}

// ============================================================================
// Property 7: distance returns zero for identical points
// Feature: 070-03-lua-api-bindings, Property 7: distance identity
//
// **Validates: Requirements 16.1, 21.8**
//
// For any point (x, y), engine.distance(x, y, x, y) shall equal 0.0.
// ============================================================================
TEST_CASE("Property: distance returns zero for identical points", "[lua_bindings]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 1000000)));
    std::mt19937 gen(seed);
    std::uniform_real_distribution<double> coord_dist(-10000.0, 10000.0);

    for (int inner = 0; inner < NUM_INNER_TESTS; ++inner) {
        LuaManager lua_manager;
        EntityManager entity_manager;
        ComponentStorage storage;
        Blackboard blackboard;

        register_bindings(lua_manager.state());
        store_engine_pointers(lua_manager.state(), &storage, &entity_manager, &blackboard);

        double x = coord_dist(gen);
        double y = coord_dist(gen);

        std::ostringstream lua_code;
        lua_code << "local d = engine.distance(" << x << ", " << y << ", " << x << ", " << y << ")\n"
                 << "engine.set_blackboard('dist_self', d)\n";

        auto result = lua_manager.execute_string(lua_code.str());
        REQUIRE(result.success);

        double d = blackboard.get<double>("dist_self");

        REQUIRE(d == 0.0);
    }
}
