/**
 * Unit tests for Lua API bindings (lua_bindings.hpp/cpp)
 *
 * Each test creates a fresh LuaManager, registers bindings, stores engine
 * pointers, then exercises the engine.* Lua API and verifies C++ state.
 *
 * Requirements tested: 20.1–20.16
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "engine/lua_manager.hpp"
#include "engine/lua_bindings.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"

#include <cmath>
#include <string>

// Helper: set up a fresh Lua environment with bindings registered
struct LuaTestFixture {
    LuaManager lua_manager;
    EntityManager entity_manager;
    ComponentStorage storage;
    Blackboard blackboard;

    LuaTestFixture() {
        register_bindings(lua_manager.state());
        store_engine_pointers(lua_manager.state(), &storage, &entity_manager, &blackboard);
    }
};

// -----------------------------------------------------------------------
// 1. GetPositionReturnsXY
// Validates: Requirement 20.1
// -----------------------------------------------------------------------
TEST_CASE("GetPositionReturnsXY", "[lua_bindings][unit]") {
    LuaTestFixture f;
    Entity e = f.entity_manager.create_entity();
    f.storage.add_component<Position>(e, Position{100.0f, 200.0f});
    f.blackboard.set("test.entity_id", static_cast<double>(e));

    auto result = f.lua_manager.execute_string(R"(
        local id = engine.get_blackboard("test.entity_id")
        local x, y = engine.get_position(id)
        engine.set_blackboard("test.x", x)
        engine.set_blackboard("test.y", y)
    )");
    REQUIRE(result.success);
    CHECK(f.blackboard.get<double>("test.x") == 100.0);
    CHECK(f.blackboard.get<double>("test.y") == 200.0);
}

// -----------------------------------------------------------------------
// 2. SetPositionUpdatesStorage
// Validates: Requirement 20.2
// -----------------------------------------------------------------------
TEST_CASE("SetPositionUpdatesStorage", "[lua_bindings][unit]") {
    LuaTestFixture f;
    Entity e = f.entity_manager.create_entity();
    f.blackboard.set("test.entity_id", static_cast<double>(e));

    auto result = f.lua_manager.execute_string(R"(
        local id = engine.get_blackboard("test.entity_id")
        engine.set_position(id, 50, 75)
    )");
    REQUIRE(result.success);
    auto pos = f.storage.get_component<Position>(e);
    REQUIRE(pos.has_value());
    CHECK(pos->get().x == 50.0f);
    CHECK(pos->get().y == 75.0f);
}


// -----------------------------------------------------------------------
// 3. GetSetVelocity
// Validates: Requirement 20.3
// -----------------------------------------------------------------------
TEST_CASE("GetSetVelocity", "[lua_bindings][unit]") {
    LuaTestFixture f;
    Entity e = f.entity_manager.create_entity();
    f.blackboard.set("test.entity_id", static_cast<double>(e));

    auto result = f.lua_manager.execute_string(R"(
        local id = engine.get_blackboard("test.entity_id")
        engine.set_velocity(id, 3.5, -7.25)
        local dx, dy = engine.get_velocity(id)
        engine.set_blackboard("test.dx", dx)
        engine.set_blackboard("test.dy", dy)
    )");
    REQUIRE(result.success);

    auto vel = f.storage.get_component<Velocity>(e);
    REQUIRE(vel.has_value());
    CHECK(vel->get().dx == 3.5f);
    CHECK(vel->get().dy == -7.25f);

    CHECK(f.blackboard.get<double>("test.dx") == Catch::Approx(3.5));
    CHECK(f.blackboard.get<double>("test.dy") == Catch::Approx(-7.25));
}

// -----------------------------------------------------------------------
// 4. GetSetRotation
// Validates: Requirement 20.4
// -----------------------------------------------------------------------
TEST_CASE("GetSetRotation", "[lua_bindings][unit]") {
    LuaTestFixture f;
    Entity e = f.entity_manager.create_entity();
    f.blackboard.set("test.entity_id", static_cast<double>(e));

    auto result = f.lua_manager.execute_string(R"(
        local id = engine.get_blackboard("test.entity_id")
        engine.set_rotation(id, 1.57)
        local angle = engine.get_rotation(id)
        engine.set_blackboard("test.angle", angle)
    )");
    REQUIRE(result.success);

    auto rot = f.storage.get_component<Rotation>(e);
    REQUIRE(rot.has_value());
    CHECK(rot->get().angle == Catch::Approx(1.57f));
    CHECK(rot->get().angular_velocity == 0.0f);

    CHECK(f.blackboard.get<double>("test.angle") == Catch::Approx(1.57));
}

// -----------------------------------------------------------------------
// 5. GetSetSize
// Validates: Requirement 20.5
// -----------------------------------------------------------------------
TEST_CASE("GetSetSize", "[lua_bindings][unit]") {
    LuaTestFixture f;
    Entity e = f.entity_manager.create_entity();
    f.blackboard.set("test.entity_id", static_cast<double>(e));

    auto result = f.lua_manager.execute_string(R"(
        local id = engine.get_blackboard("test.entity_id")
        engine.set_size(id, 64, 128)
        local w, h = engine.get_size(id)
        engine.set_blackboard("test.w", w)
        engine.set_blackboard("test.h", h)
    )");
    REQUIRE(result.success);

    auto sz = f.storage.get_component<Size>(e);
    REQUIRE(sz.has_value());
    CHECK(sz->get().width == 64.0f);
    CHECK(sz->get().height == 128.0f);

    CHECK(f.blackboard.get<double>("test.w") == Catch::Approx(64.0));
    CHECK(f.blackboard.get<double>("test.h") == Catch::Approx(128.0));
}

// -----------------------------------------------------------------------
// 6. GetSetColor (including clamping)
// Validates: Requirement 20.6
// -----------------------------------------------------------------------
TEST_CASE("GetSetColor", "[lua_bindings][unit]") {
    LuaTestFixture f;
    Entity e = f.entity_manager.create_entity();
    f.blackboard.set("test.entity_id", static_cast<double>(e));

    // Set color with values that need clamping (300 -> 255, -10 -> 0)
    auto result = f.lua_manager.execute_string(R"(
        local id = engine.get_blackboard("test.entity_id")
        engine.set_color(id, 300, -10, 128, 255)
        local r, g, b, a = engine.get_color(id)
        engine.set_blackboard("test.r", r)
        engine.set_blackboard("test.g", g)
        engine.set_blackboard("test.b", b)
        engine.set_blackboard("test.a", a)
    )");
    REQUIRE(result.success);

    auto col = f.storage.get_component<Color>(e);
    REQUIRE(col.has_value());
    CHECK(col->get().r == 255);
    CHECK(col->get().g == 0);
    CHECK(col->get().b == 128);
    CHECK(col->get().a == 255);

    CHECK(f.blackboard.get<double>("test.r") == 255.0);
    CHECK(f.blackboard.get<double>("test.g") == 0.0);
    CHECK(f.blackboard.get<double>("test.b") == 128.0);
    CHECK(f.blackboard.get<double>("test.a") == 255.0);
}

// -----------------------------------------------------------------------
// 7. GetInputReturnsBooleans
// Validates: Requirement 20.7
// -----------------------------------------------------------------------
TEST_CASE("GetInputReturnsBooleans", "[lua_bindings][unit]") {
    LuaTestFixture f;
    Entity e = f.entity_manager.create_entity();
    f.storage.add_component<Input>(e, Input{true, false, true, false, true});
    f.blackboard.set("test.entity_id", static_cast<double>(e));

    auto result = f.lua_manager.execute_string(R"(
        local id = engine.get_blackboard("test.entity_id")
        local up, down, left, right, fire = engine.get_input(id)
        engine.set_blackboard("test.up", up)
        engine.set_blackboard("test.down", down)
        engine.set_blackboard("test.left", left)
        engine.set_blackboard("test.right", right)
        engine.set_blackboard("test.fire", fire)
    )");
    REQUIRE(result.success);

    CHECK(f.blackboard.get<bool>("test.up") == true);
    CHECK(f.blackboard.get<bool>("test.down") == false);
    CHECK(f.blackboard.get<bool>("test.left") == true);
    CHECK(f.blackboard.get<bool>("test.right") == false);
    CHECK(f.blackboard.get<bool>("test.fire") == true);
}

// -----------------------------------------------------------------------
// 8. BlackboardRoundTrip (double, string, bool)
// Validates: Requirement 20.8
// -----------------------------------------------------------------------
TEST_CASE("BlackboardRoundTrip", "[lua_bindings][unit]") {
    LuaTestFixture f;

    auto result = f.lua_manager.execute_string(R"(
        engine.set_blackboard("num", 42.5)
        engine.set_blackboard("str", "hello")
        engine.set_blackboard("flag", true)

        local n = engine.get_blackboard("num")
        local s = engine.get_blackboard("str")
        local b = engine.get_blackboard("flag")

        engine.set_blackboard("out.num", n)
        engine.set_blackboard("out.str", s)
        engine.set_blackboard("out.flag", b)
    )");
    REQUIRE(result.success);

    CHECK(f.blackboard.get<double>("out.num") == 42.5);
    CHECK(f.blackboard.get<std::string>("out.str") == "hello");
    CHECK(f.blackboard.get<bool>("out.flag") == true);
}


// -----------------------------------------------------------------------
// 9. CreateEntityReturnsAlive
// Validates: Requirement 20.9
// -----------------------------------------------------------------------
TEST_CASE("CreateEntityReturnsAlive", "[lua_bindings][unit]") {
    LuaTestFixture f;

    auto result = f.lua_manager.execute_string(R"(
        local id = engine.create_entity()
        engine.set_blackboard("test.new_id", id)
    )");
    REQUIRE(result.success);

    auto new_id = static_cast<Entity>(f.blackboard.get<double>("test.new_id"));
    CHECK(f.entity_manager.is_alive(new_id));
}

// -----------------------------------------------------------------------
// 10. DestroyEntityAddsRequest
// Validates: Requirement 20.10
// -----------------------------------------------------------------------
TEST_CASE("DestroyEntityAddsRequest", "[lua_bindings][unit]") {
    LuaTestFixture f;
    Entity e = f.entity_manager.create_entity();
    f.blackboard.set("test.entity_id", static_cast<double>(e));

    CHECK_FALSE(f.storage.has_component<DestroyRequest>(e));

    auto result = f.lua_manager.execute_string(R"(
        local id = engine.get_blackboard("test.entity_id")
        engine.destroy_entity(id)
    )");
    REQUIRE(result.success);

    CHECK(f.storage.has_component<DestroyRequest>(e));
}

// -----------------------------------------------------------------------
// 11. AddComponentFromTable
// Validates: Requirement 20.11
// -----------------------------------------------------------------------
TEST_CASE("AddComponentFromTable", "[lua_bindings][unit]") {
    LuaTestFixture f;
    Entity e = f.entity_manager.create_entity();
    f.blackboard.set("test.entity_id", static_cast<double>(e));

    auto result = f.lua_manager.execute_string(R"(
        local id = engine.get_blackboard("test.entity_id")
        engine.add_component(id, "Position", {x = 10, y = 20})
    )");
    REQUIRE(result.success);

    auto pos = f.storage.get_component<Position>(e);
    REQUIRE(pos.has_value());
    CHECK(pos->get().x == 10.0f);
    CHECK(pos->get().y == 20.0f);
}

// -----------------------------------------------------------------------
// 12. HasComponentReturnsCorrect
// Validates: Requirement 20.12
// -----------------------------------------------------------------------
TEST_CASE("HasComponentReturnsCorrect", "[lua_bindings][unit]") {
    LuaTestFixture f;
    Entity e = f.entity_manager.create_entity();
    f.storage.add_component<Position>(e, Position{1.0f, 2.0f});
    f.blackboard.set("test.entity_id", static_cast<double>(e));

    auto result = f.lua_manager.execute_string(R"(
        local id = engine.get_blackboard("test.entity_id")
        engine.set_blackboard("test.has_pos", engine.has_component(id, "Position"))
        engine.set_blackboard("test.has_vel", engine.has_component(id, "Velocity"))
    )");
    REQUIRE(result.success);

    CHECK(f.blackboard.get<bool>("test.has_pos") == true);
    CHECK(f.blackboard.get<bool>("test.has_vel") == false);
}

// -----------------------------------------------------------------------
// 13. EntitiesWithReturnsCorrect
// Validates: Requirement 20.13
// -----------------------------------------------------------------------
TEST_CASE("EntitiesWithReturnsCorrect", "[lua_bindings][unit]") {
    LuaTestFixture f;
    Entity e1 = f.entity_manager.create_entity();
    Entity e2 = f.entity_manager.create_entity();
    Entity e3 = f.entity_manager.create_entity();
    f.storage.add_component<Position>(e1, Position{0, 0});
    f.storage.add_component<Position>(e2, Position{1, 1});
    f.storage.add_component<Position>(e3, Position{2, 2});

    auto result = f.lua_manager.execute_string(R"(
        local entities = engine.entities_with("Position")
        engine.set_blackboard("test.count", #entities)
    )");
    REQUIRE(result.success);

    CHECK(f.blackboard.get<double>("test.count") == 3.0);
}

// -----------------------------------------------------------------------
// 14. DirectionFromAngle
// Validates: Requirement 20.14
// -----------------------------------------------------------------------
TEST_CASE("DirectionFromAngle", "[lua_bindings][unit]") {
    LuaTestFixture f;

    auto result = f.lua_manager.execute_string(R"(
        local dx, dy = engine.direction_from_angle(0)
        engine.set_blackboard("test.dx", dx)
        engine.set_blackboard("test.dy", dy)
    )");
    REQUIRE(result.success);

    CHECK(f.blackboard.get<double>("test.dx") == Catch::Approx(1.0).margin(1e-9));
    CHECK(f.blackboard.get<double>("test.dy") == Catch::Approx(0.0).margin(1e-9));
}

// -----------------------------------------------------------------------
// 15. DistanceCalculation
// Validates: Requirement 20.15
// -----------------------------------------------------------------------
TEST_CASE("DistanceCalculation", "[lua_bindings][unit]") {
    LuaTestFixture f;

    auto result = f.lua_manager.execute_string(R"(
        local d = engine.distance(0, 0, 3, 4)
        engine.set_blackboard("test.dist", d)
    )");
    REQUIRE(result.success);

    CHECK(f.blackboard.get<double>("test.dist") == Catch::Approx(5.0));
}

// -----------------------------------------------------------------------
// 16. GetterReturnsNilForMissing
// Validates: Requirement 20.16
// -----------------------------------------------------------------------
TEST_CASE("GetterReturnsNilForMissing", "[lua_bindings][unit]") {
    LuaTestFixture f;
    Entity e = f.entity_manager.create_entity();
    // Entity has NO Position component
    f.blackboard.set("test.entity_id", static_cast<double>(e));

    auto result = f.lua_manager.execute_string(R"(
        local id = engine.get_blackboard("test.entity_id")
        local val = engine.get_position(id)
        if val == nil then
            engine.set_blackboard("test.is_nil", true)
        else
            engine.set_blackboard("test.is_nil", false)
        end
    )");
    REQUIRE(result.success);

    CHECK(f.blackboard.get<bool>("test.is_nil") == true);
}
