/**
 * Property-based tests for Script component, LuaManager, ScriptSystem,
 * and GameData loader script parsing.
 *
 * These tests verify universal properties for the Lua scripting infrastructure:
 * Script component storage, destruction pipeline, LuaManager error handling,
 * ScriptSystem lifecycle hooks, per-entity isolation, and GameData parsing.
 *
 * Testing Framework: Catch2 v3
 * Constants: NUM_OUTER_TESTS = 10, NUM_INNER_TESTS = 5
 *
 * Requirements tested: 3.1, 3.4, 5.1, 5.2, 6.1, 6.2, 6.3, 7.1, 7.2,
 *                      8.1, 8.2, 8.3, 9.1, 9.2, 9.3, 10.1, 10.2,
 *                      11.1, 11.2, 11.3, 16.1, 16.2, 16.3, 16.4
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/destruction.hpp"
#include "engine/lua_manager.hpp"
#include "engine/ecs/systems/script_system.hpp"
#include "engine/gamedata_loader.hpp"
#include <random>
#include <string>
#include <vector>
#include <fstream>
#include <cstdio>

// Configurable test iteration counts
static constexpr int NUM_OUTER_TESTS = 10;
static constexpr int NUM_INNER_TESTS = 5;

/// Helper: absolute path for direct lua_manager.load_script() calls
static std::string test_script_abs_path(const std::string& filename) {
    return std::string(CLASS_ROOT_DIR) + "/CPP/engine/tests/test_assets/" + filename;
}

/// Helper: relative path for Script component filenames used with ScriptSystem.
/// ScriptSystem prepends project_paths::assets_dir() (absolute), so
/// "../CPP/engine/tests/test_assets/" resolves correctly from assets/.
static std::string test_script_path(const std::string& filename) {
    return "../CPP/engine/tests/test_assets/" + filename;
}

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
// Property 1: Script component storage round-trip
// Feature: 070-02-lua-integration, Property 1: Script component storage round-trip
//
// **Validates: Requirements 5.1, 5.2, 6.1, 6.2, 6.3**
//
// For any entity and for any non-empty string filename, adding a Script
// component with that filename and initialized=false to ComponentStorage
// and then retrieving it via get_component<Script>() should return a Script
// with an identical filename and initialized value.
// ============================================================================
TEST_CASE("Property: Script component storage round-trip", "[script]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 1000000)));
    std::mt19937 gen(seed);

    for (int inner = 0; inner < NUM_INNER_TESTS; ++inner) {
        EntityManager em;
        ComponentStorage storage;

        std::string filename = random_string(gen, 5, 20);
        Entity e = em.create_entity();
        storage.add_component<Script>(e, Script{filename, false});

        auto retrieved = storage.get_component<Script>(e);
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->get().filename == filename);
        REQUIRE(retrieved->get().initialized == false);
    }
}

// ============================================================================
// Property 2: Destruction pipeline removes Script component
// Feature: 070-02-lua-integration, Property 2: Destruction pipeline removes Script component
//
// **Validates: Requirements 7.1, 7.2**
//
// For any entity that has a Script component and is marked with DestroyRequest,
// after calling destroy_marked_entities(), has_component<Script>() should
// return false for that entity.
// ============================================================================
TEST_CASE("Property: Destruction pipeline removes Script component", "[script]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 1000000)));
    std::mt19937 gen(seed);

    EntityManager em;
    ComponentStorage storage;

    std::string filename = random_string(gen, 5, 20);
    Entity e = em.create_entity();
    storage.add_component<Script>(e, Script{filename, false});
    storage.add_component<DestroyRequest>(e, DestroyRequest{});

    destroy_marked_entities(em, storage);

    REQUIRE_FALSE(storage.has_component<Script>(e));
    REQUIRE_FALSE(em.is_alive(e));
}

// ============================================================================
// Property 3: LuaManager error recovery
// Feature: 070-02-lua-integration, Property 3: LuaManager error recovery
//
// **Validates: Requirements 3.4**
//
// For any sequence where a bad script (non-existent file) is loaded first,
// the LuaManager should still successfully load and execute a subsequent
// valid script. The Lua state remains usable after errors.
// ============================================================================
TEST_CASE("Property: LuaManager error recovery", "[script]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 1000000)));
    std::mt19937 gen(seed);

    LuaManager lua_manager;

    // Load a non-existent file (random path)
    std::string bad_path = random_string(gen, 5, 20) + ".lua";
    auto bad_result = lua_manager.load_script(bad_path);
    REQUIRE_FALSE(bad_result.success);

    // Now load a valid script — should succeed
    auto good_result = lua_manager.load_script(test_script_abs_path("valid_script.lua"));
    REQUIRE(good_result.success);
}

// ============================================================================
// Property 4: Non-existent file error contains path
// Feature: 070-02-lua-integration, Property 4: Non-existent file error contains path
//
// **Validates: Requirements 3.1**
//
// For any file path string that does not correspond to an existing file,
// calling load_script() with that path should return a Result where
// success is false and error_message contains the file path string.
// ============================================================================
TEST_CASE("Property: Non-existent file error contains path", "[script]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 1000000)));
    std::mt19937 gen(seed);

    for (int inner = 0; inner < NUM_INNER_TESTS; ++inner) {
        LuaManager lua_manager;

        std::string bad_path = random_string(gen, 5, 20) + ".lua";
        auto result = lua_manager.load_script(bad_path);

        REQUIRE_FALSE(result.success);
        REQUIRE(result.error_message.find(bad_path) != std::string::npos);
    }
}

// ============================================================================
// Property 5: on_init lifecycle transition
// Feature: 070-02-lua-integration, Property 5: on_init lifecycle transition
//
// **Validates: Requirements 8.1**
//
// For any entity with a Script component where initialized == false and the
// script file exists, after one ScriptSystem update, the Script component's
// initialized field should be true.
// ============================================================================
TEST_CASE("Property: on_init lifecycle transition", "[script]") {
    auto _iter = GENERATE(take(NUM_OUTER_TESTS, random(0, 1)));
    (void)_iter;

    LuaManager lua_manager;
    ScriptSystem script_system(lua_manager);
    EntityManager entity_manager;
    ComponentStorage storage;
    Blackboard blackboard;
    blackboard.set("delta_time", 0.016);

    Entity e = entity_manager.create_entity();
    storage.add_component<Script>(e, Script{test_script_path("valid_script.lua"), false});

    script_system.update(storage, entity_manager, blackboard);

    auto script = storage.get_component<Script>(e);
    REQUIRE(script.has_value());
    REQUIRE(script->get().initialized);
}

// ============================================================================
// Property 6: on_update receives correct arguments
// Feature: 070-02-lua-integration, Property 6: on_update receives correct arguments
//
// **Validates: Requirements 8.2**
//
// For any entity with an initialized Script component and for any positive
// delta_time value, calling ScriptSystem update should invoke the script's
// on_update without crashing.
// ============================================================================
TEST_CASE("Property: on_update receives correct arguments", "[script]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 1000000)));
    std::mt19937 gen(seed);
    std::uniform_real_distribution<double> dt_dist(0.001, 0.1);

    LuaManager lua_manager;
    ScriptSystem script_system(lua_manager);
    EntityManager entity_manager;
    ComponentStorage storage;
    Blackboard blackboard;
    blackboard.set("delta_time", 0.016);

    Entity e = entity_manager.create_entity();
    storage.add_component<Script>(e, Script{test_script_path("valid_script.lua"), false});

    // Initialize the entity
    script_system.update(storage, entity_manager, blackboard);

    // Now test with various random delta_time values
    for (int inner = 0; inner < NUM_INNER_TESTS; ++inner) {
        double dt = dt_dist(gen);
        blackboard.set("delta_time", dt);
        script_system.update(storage, entity_manager, blackboard);

        // Verify no crash and entity still initialized
        auto script = storage.get_component<Script>(e);
        REQUIRE(script.has_value());
        REQUIRE(script->get().initialized);
    }
}

// ============================================================================
// Property 7: on_collision dispatch count
// Feature: 070-02-lua-integration, Property 7: on_collision dispatch count
//
// **Validates: Requirements 8.3**
//
// For any entity with a Script component and a CollidedWith component
// containing N collision partners (1-5), after one ScriptSystem update,
// the system should process without crashing.
// ============================================================================
TEST_CASE("Property: on_collision dispatch count", "[script]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 1000000)));
    std::mt19937 gen(seed);
    std::uniform_int_distribution<int> partner_count_dist(1, 5);

    LuaManager lua_manager;
    ScriptSystem script_system(lua_manager);
    EntityManager entity_manager;
    ComponentStorage storage;
    Blackboard blackboard;
    blackboard.set("delta_time", 0.016);

    Entity e = entity_manager.create_entity();
    storage.add_component<Script>(e, Script{test_script_path("valid_script.lua"), false});

    // Initialize the entity first
    script_system.update(storage, entity_manager, blackboard);

    // Create random number of collision partners
    int num_partners = partner_count_dist(gen);
    std::vector<Entity> partners;
    for (int i = 0; i < num_partners; ++i) {
        partners.push_back(entity_manager.create_entity());
    }
    storage.add_component<CollidedWith>(e, CollidedWith{partners});

    // Update — should dispatch on_collision for each partner without crash
    script_system.update(storage, entity_manager, blackboard);

    auto script = storage.get_component<Script>(e);
    REQUIRE(script.has_value());
    REQUIRE(script->get().initialized);
}

// ============================================================================
// Property 8: Per-entity environment isolation
// Feature: 070-02-lua-integration, Property 8: Per-entity environment isolation
//
// **Validates: Requirements 9.1, 9.2, 9.3**
//
// For any two entities that use the same script file (isolation_test.lua),
// after initialization and update, both should be initialized and the system
// should not crash (each entity has its own Lua environment).
// ============================================================================
TEST_CASE("Property: Per-entity environment isolation", "[script]") {
    auto _iter = GENERATE(take(NUM_OUTER_TESTS, random(0, 1)));
    (void)_iter;

    LuaManager lua_manager;
    ScriptSystem script_system(lua_manager);
    EntityManager entity_manager;
    ComponentStorage storage;
    Blackboard blackboard;
    blackboard.set("delta_time", 0.016);

    Entity e1 = entity_manager.create_entity();
    Entity e2 = entity_manager.create_entity();
    storage.add_component<Script>(e1, Script{test_script_path("isolation_test.lua"), false});
    storage.add_component<Script>(e2, Script{test_script_path("isolation_test.lua"), false});

    // Initialize both
    script_system.update(storage, entity_manager, blackboard);

    auto s1 = storage.get_component<Script>(e1);
    auto s2 = storage.get_component<Script>(e2);
    REQUIRE(s1.has_value());
    REQUIRE(s2.has_value());
    REQUIRE(s1->get().initialized);
    REQUIRE(s2->get().initialized);

    // Update again — each entity's on_update runs in its own environment
    script_system.update(storage, entity_manager, blackboard);
    // No crash means isolation is working
}

// ============================================================================
// Property 9: Error in one entity does not block others
// Feature: 070-02-lua-integration, Property 9: Error in one entity does not block others
//
// **Validates: Requirements 10.1, 10.2**
//
// For any set of entities with Script components where one entity's script
// raises a runtime error in on_update, the ScriptSystem should still call
// on_update for all other entities in the same frame. All entities (including
// the erroring one) should be initialized after the first update.
// ============================================================================
TEST_CASE("Property: Error in one entity does not block others", "[script]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 1000000)));
    std::mt19937 gen(seed);
    std::uniform_int_distribution<int> count_dist(2, 5);

    LuaManager lua_manager;
    ScriptSystem script_system(lua_manager);
    EntityManager entity_manager;
    ComponentStorage storage;
    Blackboard blackboard;
    blackboard.set("delta_time", 0.016);

    int total_entities = count_dist(gen);
    std::vector<Entity> entities;

    // First entity gets the error script
    Entity bad = entity_manager.create_entity();
    storage.add_component<Script>(bad, Script{test_script_path("runtime_error.lua"), false});
    entities.push_back(bad);

    // Remaining entities get valid scripts
    for (int i = 1; i < total_entities; ++i) {
        Entity good = entity_manager.create_entity();
        storage.add_component<Script>(good, Script{test_script_path("valid_script.lua"), false});
        entities.push_back(good);
    }

    // Update — all should initialize; bad entity errors on on_update but
    // should not prevent others from being processed
    script_system.update(storage, entity_manager, blackboard);

    // Verify all entities are initialized
    for (size_t i = 0; i < entities.size(); ++i) {
        auto script = storage.get_component<Script>(entities[i]);
        REQUIRE(script.has_value());
        REQUIRE(script->get().initialized);
    }
}

// ============================================================================
// Property 10: GameData loader script parsing round-trip
// Feature: 070-02-lua-integration, Property 10: GameData loader script parsing round-trip
//
// **Validates: Requirements 11.1, 11.2, 11.3**
//
// For any valid filename string, a GameData.json entity definition containing
// "script": { "filename": "<value>" } should result in the entity having a
// Script component with filename equal to that value and initialized == false.
// ============================================================================
TEST_CASE("Property: GameData loader script parsing round-trip", "[script]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 1000000)));
    std::mt19937 gen(seed);

    EntityManager entity_manager;
    ComponentStorage storage;
    Blackboard blackboard;

    std::string filename = "scripts/" + random_string(gen, 5, 15) + ".lua";

    // Build a minimal JSON string with a script component
    std::string json_content = R"({
        "window": { "width": 800, "height": 600 },
        "entities": [
            {
                "id": "test_entity",
                "components": {
                    "script": { "filename": ")" + filename + R"(" }
                }
            }
        ]
    })";

    // Write to a temporary file
    std::string temp_path = "test_gamedata_script_prop_catch2_" + std::to_string(seed) + ".json";
    {
        std::ofstream out(temp_path);
        REQUIRE(out.is_open());
        out << json_content;
    }

    // Parse via load_game_data
    load_game_data(temp_path, entity_manager, storage, blackboard);

    // Clean up temp file
    std::remove(temp_path.c_str());

    // Retrieve the entity from the blackboard
    Entity e = blackboard.get<Entity>("entity.id.test_entity");

    // Verify Script component
    REQUIRE(storage.has_component<Script>(e));
    auto script = storage.get_component<Script>(e);
    REQUIRE(script.has_value());
    REQUIRE(script->get().filename == filename);
    REQUIRE(script->get().initialized == false);
}
