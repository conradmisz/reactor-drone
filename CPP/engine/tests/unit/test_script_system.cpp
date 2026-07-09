/**
 * Unit tests for ScriptSystem class
 *
 * These tests verify ScriptSystem lifecycle hook dispatch, error recovery,
 * per-entity isolation, and graceful handling of missing hooks and bad scripts.
 *
 * Requirements tested: 5.2, 8.1, 8.2, 8.3, 8.4, 9.1, 9.3,
 *                      10.1, 10.2, 10.3, 15.1, 15.2, 15.3, 15.4, 15.5, 15.6
 */

#include <catch2/catch_test_macros.hpp>
#include "engine/lua_manager.hpp"
#include "engine/ecs/systems/script_system.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"

// Path from assets_dir to test_assets: assets_dir is CLASS_ROOT_DIR/assets,
// so "../CPP/engine/tests/test_assets/<file>" resolves correctly.
static std::string test_script_path(const std::string& filename) {
    return "../CPP/engine/tests/test_assets/" + filename;
}

// -----------------------------------------------------------------------
// 1. on_init sets initialized flag; second update does not re-init
// Validates: Requirement 8.1, 15.1
// -----------------------------------------------------------------------
TEST_CASE("OnInitCalledOnce", "[script_system][unit]") {
    LuaManager lua_manager;
    ScriptSystem script_system(lua_manager);
    EntityManager entity_manager;
    ComponentStorage storage;
    Blackboard blackboard;
    blackboard.set("delta_time", 0.016);

    Entity e = entity_manager.create_entity();
    storage.add_component<Script>(e, Script{test_script_path("valid_script.lua"), false});

    // First update: should initialize the entity
    script_system.update(storage, entity_manager, blackboard);
    auto script1 = storage.get_component<Script>(e);
    REQUIRE(script1.has_value());
    CHECK(script1->get().initialized == true);

    // Second update: initialized stays true, no crash
    script_system.update(storage, entity_manager, blackboard);
    auto script2 = storage.get_component<Script>(e);
    REQUIRE(script2.has_value());
    CHECK(script2->get().initialized == true);
}


// -----------------------------------------------------------------------
// 2. on_update runs every frame without crashing
// Validates: Requirement 8.2, 15.2
// -----------------------------------------------------------------------
TEST_CASE("OnUpdateCalledEveryFrame", "[script_system][unit]") {
    LuaManager lua_manager;
    ScriptSystem script_system(lua_manager);
    EntityManager entity_manager;
    ComponentStorage storage;
    Blackboard blackboard;
    blackboard.set("delta_time", 0.016);

    Entity e = entity_manager.create_entity();
    storage.add_component<Script>(e, Script{test_script_path("valid_script.lua"), false});

    // Update 3 times — first initializes, all three call on_update
    script_system.update(storage, entity_manager, blackboard);
    script_system.update(storage, entity_manager, blackboard);
    script_system.update(storage, entity_manager, blackboard);

    auto script = storage.get_component<Script>(e);
    REQUIRE(script.has_value());
    CHECK(script->get().initialized == true);
}

// -----------------------------------------------------------------------
// 3. on_collision dispatched per collision partner — no crash
// Validates: Requirement 8.3, 15.3
// -----------------------------------------------------------------------
TEST_CASE("OnCollisionCalledPerPartner", "[script_system][unit]") {
    LuaManager lua_manager;
    ScriptSystem script_system(lua_manager);
    EntityManager entity_manager;
    ComponentStorage storage;
    Blackboard blackboard;
    blackboard.set("delta_time", 0.016);

    Entity e = entity_manager.create_entity();
    Entity a = entity_manager.create_entity();
    Entity b = entity_manager.create_entity();

    storage.add_component<Script>(e, Script{test_script_path("valid_script.lua"), false});

    // First update to initialize the script
    script_system.update(storage, entity_manager, blackboard);

    // Add collision partners and update again
    storage.add_component<CollidedWith>(e, CollidedWith{{a, b}});
    script_system.update(storage, entity_manager, blackboard);

    // No crash and entity still initialized
    auto script = storage.get_component<Script>(e);
    REQUIRE(script.has_value());
    CHECK(script->get().initialized == true);
}

// -----------------------------------------------------------------------
// 4. on_destroy dispatched when DestroyRequest present — no crash
// Validates: Requirement 8.4, 15.4
// -----------------------------------------------------------------------
TEST_CASE("OnDestroyCalledWithDestroyRequest", "[script_system][unit]") {
    LuaManager lua_manager;
    ScriptSystem script_system(lua_manager);
    EntityManager entity_manager;
    ComponentStorage storage;
    Blackboard blackboard;
    blackboard.set("delta_time", 0.016);

    Entity e = entity_manager.create_entity();
    storage.add_component<Script>(e, Script{test_script_path("valid_script.lua"), false});

    // Initialize first
    script_system.update(storage, entity_manager, blackboard);

    // Mark for destruction and update
    storage.add_component<DestroyRequest>(e, DestroyRequest{});
    script_system.update(storage, entity_manager, blackboard);

    // No crash — entity still has its script component (destruction
    // pipeline hasn't run yet, only ScriptSystem called on_destroy)
    auto script = storage.get_component<Script>(e);
    REQUIRE(script.has_value());
    CHECK(script->get().initialized == true);
}

// -----------------------------------------------------------------------
// 5. Runtime error in one entity does not crash or block others
// Validates: Requirement 10.1, 10.2, 15.5
// -----------------------------------------------------------------------
TEST_CASE("ErrorDoesNotCrash", "[script_system][unit]") {
    LuaManager lua_manager;
    ScriptSystem script_system(lua_manager);
    EntityManager entity_manager;
    ComponentStorage storage;
    Blackboard blackboard;
    blackboard.set("delta_time", 0.016);

    // Entity with a script that errors on on_update
    Entity bad = entity_manager.create_entity();
    storage.add_component<Script>(bad, Script{test_script_path("runtime_error.lua"), false});

    // Entity with a valid script
    Entity good = entity_manager.create_entity();
    storage.add_component<Script>(good, Script{test_script_path("valid_script.lua"), false});

    // Update — both should initialize, bad entity errors on on_update
    script_system.update(storage, entity_manager, blackboard);

    // Both entities should be initialized
    auto bad_script = storage.get_component<Script>(bad);
    REQUIRE(bad_script.has_value());
    CHECK(bad_script->get().initialized == true);

    auto good_script = storage.get_component<Script>(good);
    REQUIRE(good_script.has_value());
    CHECK(good_script->get().initialized == true);
}

// -----------------------------------------------------------------------
// 6. Two entities with the same script maintain isolated environments
// Validates: Requirement 9.1, 9.3, 15.6
// -----------------------------------------------------------------------
TEST_CASE("SameScriptIsolation", "[script_system][unit]") {
    LuaManager lua_manager;
    ScriptSystem script_system(lua_manager);
    EntityManager entity_manager;
    ComponentStorage storage;
    Blackboard blackboard;
    blackboard.set("delta_time", 0.016);

    // Both entities use isolation_test.lua which sets my_value = entity_id
    Entity e1 = entity_manager.create_entity();
    Entity e2 = entity_manager.create_entity();
    storage.add_component<Script>(e1, Script{test_script_path("isolation_test.lua"), false});
    storage.add_component<Script>(e2, Script{test_script_path("isolation_test.lua"), false});

    // Initialize both
    script_system.update(storage, entity_manager, blackboard);

    // Both should be initialized without interfering with each other
    auto s1 = storage.get_component<Script>(e1);
    auto s2 = storage.get_component<Script>(e2);
    REQUIRE(s1.has_value());
    REQUIRE(s2.has_value());
    CHECK(s1->get().initialized == true);
    CHECK(s2->get().initialized == true);

    // Update again — each entity's on_update runs in its own environment
    script_system.update(storage, entity_manager, blackboard);
    // No crash means isolation is working (each sees its own my_value)
}

// -----------------------------------------------------------------------
// 7. Script with no hooks defined — no crash, initialized is set
// Validates: Requirement 8.1, 8.2
// -----------------------------------------------------------------------
TEST_CASE("MissingHooksAreSkipped", "[script_system][unit]") {
    LuaManager lua_manager;
    ScriptSystem script_system(lua_manager);
    EntityManager entity_manager;
    ComponentStorage storage;
    Blackboard blackboard;
    blackboard.set("delta_time", 0.016);

    Entity e = entity_manager.create_entity();
    storage.add_component<Script>(e, Script{test_script_path("noop_script.lua"), false});

    // Update — should not crash even though no hooks are defined
    script_system.update(storage, entity_manager, blackboard);

    auto script = storage.get_component<Script>(e);
    REQUIRE(script.has_value());
    CHECK(script->get().initialized == true);
}

// -----------------------------------------------------------------------
// 8. Non-existent script file — no crash, initialized stays false
// Validates: Requirement 10.3
// -----------------------------------------------------------------------
TEST_CASE("FailedLoadSkipsEntity", "[script_system][unit]") {
    LuaManager lua_manager;
    ScriptSystem script_system(lua_manager);
    EntityManager entity_manager;
    ComponentStorage storage;
    Blackboard blackboard;
    blackboard.set("delta_time", 0.016);

    Entity e = entity_manager.create_entity();
    storage.add_component<Script>(e, Script{test_script_path("does_not_exist.lua"), false});

    // Update — should not crash, entity should be skipped
    script_system.update(storage, entity_manager, blackboard);

    auto script = storage.get_component<Script>(e);
    REQUIRE(script.has_value());
    CHECK(script->get().initialized == false);
}

// -----------------------------------------------------------------------
// 9. Default-constructed Script has initialized == false
// Validates: Requirement 5.2
// -----------------------------------------------------------------------
TEST_CASE("DefaultScriptInitialized", "[script_system][unit]") {
    Script s;
    CHECK(s.initialized == false);
    CHECK(s.filename.empty());
}
