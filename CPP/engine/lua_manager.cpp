#include "lua_manager.hpp"

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

LuaManager::LuaManager() {
    L_ = luaL_newstate();
    luaL_openlibs(L_);
    apply_sandbox();
}

LuaManager::~LuaManager() {
    if (L_) {
        lua_close(L_);
    }
}

LuaManager::LuaManager(LuaManager&& other) noexcept
    : L_(other.L_) {
    other.L_ = nullptr;
}

LuaManager& LuaManager::operator=(LuaManager&& other) noexcept {
    if (this != &other) {
        if (L_) {
            lua_close(L_);
        }
        L_ = other.L_;
        other.L_ = nullptr;
    }
    return *this;
}

void LuaManager::apply_sandbox() {
    lua_pushnil(L_);
    lua_setglobal(L_, "os");

    lua_pushnil(L_);
    lua_setglobal(L_, "io");

    lua_pushnil(L_);
    lua_setglobal(L_, "loadfile");

    lua_pushnil(L_);
    lua_setglobal(L_, "dofile");

    lua_pushnil(L_);
    lua_setglobal(L_, "require");
}

LuaManager::Result LuaManager::load_script(const std::string& filepath) {
    int load_status = luaL_loadfile(L_, filepath.c_str());
    if (load_status != LUA_OK) {
        std::string error = lua_tostring(L_, -1);
        lua_pop(L_, 1);
        return {false, error};
    }

    int call_status = lua_pcall(L_, 0, 0, 0);
    if (call_status != LUA_OK) {
        std::string error = lua_tostring(L_, -1);
        lua_pop(L_, 1);
        return {false, error};
    }

    return {true, ""};
}

LuaManager::Result LuaManager::execute_string(const std::string& code) {
    int load_status = luaL_loadstring(L_, code.c_str());
    if (load_status != LUA_OK) {
        std::string error = lua_tostring(L_, -1);
        lua_pop(L_, 1);
        return {false, error};
    }

    int call_status = lua_pcall(L_, 0, 0, 0);
    if (call_status != LUA_OK) {
        std::string error = lua_tostring(L_, -1);
        lua_pop(L_, 1);
        return {false, error};
    }

    return {true, ""};
}
