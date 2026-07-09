/**
 * Unit tests for LuaManager class
 *
 * These tests verify LuaManager lifecycle, script loading, error handling,
 * sandbox enforcement, and string execution.
 *
 * Requirements tested: 2.1, 2.2, 2.3, 2.4, 3.1, 3.2, 3.3, 3.4,
 *                      4.1, 4.2, 4.3, 4.4, 14.1, 14.2, 14.3, 14.4, 14.5
 */

#include <catch2/catch_test_macros.hpp>
#include "engine/lua_manager.hpp"
#include <string>

static std::string test_assets_dir() {
    return std::string(CLASS_ROOT_DIR) + "/CPP/engine/tests/test_assets";
}

TEST_CASE("LuaManager: construct and destroy", "[Engine][lua][unit]") {
    LuaManager mgr;
    REQUIRE(mgr.state() != nullptr);
}

TEST_CASE("LuaManager: load valid script", "[Engine][lua][unit]") {
    LuaManager mgr;
    auto result = mgr.load_script(test_assets_dir() + "/valid_script.lua");
    REQUIRE(result.success);
    REQUIRE(result.error_message.empty());
}

TEST_CASE("LuaManager: load non-existent file", "[Engine][lua][unit]") {
    LuaManager mgr;
    std::string bad_path = "/nonexistent/path.lua";
    auto result = mgr.load_script(bad_path);
    REQUIRE_FALSE(result.success);
    REQUIRE(result.error_message.find(bad_path) != std::string::npos);
}

TEST_CASE("LuaManager: syntax error reporting", "[Engine][lua][unit]") {
    LuaManager mgr;
    std::string path = test_assets_dir() + "/syntax_error.lua";
    auto result = mgr.load_script(path);
    REQUIRE_FALSE(result.success);
    REQUIRE(result.error_message.find("syntax_error.lua") != std::string::npos);
}

TEST_CASE("LuaManager: runtime error reporting", "[Engine][lua][unit]") {
    LuaManager mgr;
    auto result = mgr.execute_string("error('test error')");
    REQUIRE_FALSE(result.success);
    REQUIRE(result.error_message.find("test error") != std::string::npos);
}

TEST_CASE("LuaManager: sandbox blocks os", "[Engine][lua][unit]") {
    LuaManager mgr;
    auto result = mgr.execute_string("os.execute('echo hi')");
    REQUIRE_FALSE(result.success);
}

TEST_CASE("LuaManager: sandbox blocks io", "[Engine][lua][unit]") {
    LuaManager mgr;
    auto result = mgr.execute_string("io.open('test')");
    REQUIRE_FALSE(result.success);
}

TEST_CASE("LuaManager: sandbox blocks loadfile", "[Engine][lua][unit]") {
    LuaManager mgr;
    auto result = mgr.execute_string("loadfile('test.lua')");
    REQUIRE_FALSE(result.success);
}

TEST_CASE("LuaManager: sandbox blocks dofile", "[Engine][lua][unit]") {
    LuaManager mgr;
    auto result = mgr.execute_string("dofile('test.lua')");
    REQUIRE_FALSE(result.success);
}

TEST_CASE("LuaManager: sandbox allows math", "[Engine][lua][unit]") {
    LuaManager mgr;
    auto result = mgr.execute_string("local x = math.sqrt(4)");
    REQUIRE(result.success);
}

TEST_CASE("LuaManager: sandbox allows string", "[Engine][lua][unit]") {
    LuaManager mgr;
    auto result = mgr.execute_string("local x = string.len('hello')");
    REQUIRE(result.success);
}

TEST_CASE("LuaManager: sandbox allows table", "[Engine][lua][unit]") {
    LuaManager mgr;
    auto result = mgr.execute_string("local t = {}; table.insert(t, 1)");
    REQUIRE(result.success);
}

TEST_CASE("LuaManager: execute string", "[Engine][lua][unit]") {
    LuaManager mgr;
    auto result = mgr.execute_string("local x = 1 + 2");
    REQUIRE(result.success);
}
