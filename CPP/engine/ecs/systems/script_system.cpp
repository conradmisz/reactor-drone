#include "script_system.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/blackboard.hpp"
#include "engine/lua_manager.hpp"
#include "engine/lua_bindings.hpp"
#include "engine/project_paths.hpp"

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include <iostream>

ScriptSystem::ScriptSystem(LuaManager& lua_manager)
    : lua_manager_(lua_manager) {}

int ScriptSystem::create_environment(Entity entity) {
    lua_State* L = lua_manager_.state();

    // Create a new table to serve as the entity's environment
    lua_newtable(L);                    // stack: [env]

    // Create a metatable with __index = _G so scripts can access globals
    lua_newtable(L);                    // stack: [env, mt]
    lua_pushglobaltable(L);             // stack: [env, mt, _G]
    lua_setfield(L, -2, "__index");     // mt.__index = _G; stack: [env, mt]
    lua_setmetatable(L, -2);            // setmetatable(env, mt); stack: [env]

    // Store a reference to the environment in the Lua registry
    lua_pushvalue(L, -1);               // stack: [env, env]
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);  // stack: [env]
    lua_pop(L, 1);                      // stack: []

    entity_environments_[entity] = ref;
    return ref;
}

bool ScriptSystem::call_hook(Entity entity, const std::string& hook_name) {
    auto it = entity_environments_.find(entity);
    if (it == entity_environments_.end()) {
        return false;
    }

    lua_State* L = lua_manager_.state();
    int env_ref = it->second;

    // Push the environment table from registry
    lua_rawgeti(L, LUA_REGISTRYINDEX, env_ref);  // stack: [env]

    // Look up the hook function in the environment
    lua_getfield(L, -1, hook_name.c_str());       // stack: [env, fn|nil]

    if (lua_isnil(L, -1)) {
        // Hook not defined — that's fine, just clean up
        lua_pop(L, 2);  // pop nil + env
        return true;
    }

    // Call the function with no arguments
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        std::string error = lua_tostring(L, -1);
        lua_pop(L, 1);  // pop error
        lua_pop(L, 1);  // pop env
        log_error(entity, hook_name, error);
        return false;
    }

    lua_pop(L, 1);  // pop env
    return true;
}

bool ScriptSystem::call_hook(Entity entity, const std::string& hook_name, Entity arg1) {
    auto it = entity_environments_.find(entity);
    if (it == entity_environments_.end()) {
        return false;
    }

    lua_State* L = lua_manager_.state();
    int env_ref = it->second;

    lua_rawgeti(L, LUA_REGISTRYINDEX, env_ref);  // stack: [env]
    lua_getfield(L, -1, hook_name.c_str());       // stack: [env, fn|nil]

    if (lua_isnil(L, -1)) {
        lua_pop(L, 2);
        return true;
    }

    lua_pushinteger(L, static_cast<lua_Integer>(arg1));  // stack: [env, fn, arg1]

    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
        std::string error = lua_tostring(L, -1);
        lua_pop(L, 1);  // pop error
        lua_pop(L, 1);  // pop env
        log_error(entity, hook_name, error);
        return false;
    }

    lua_pop(L, 1);  // pop env
    return true;
}

bool ScriptSystem::call_hook(Entity entity, const std::string& hook_name, Entity arg1, double arg2) {
    auto it = entity_environments_.find(entity);
    if (it == entity_environments_.end()) {
        return false;
    }

    lua_State* L = lua_manager_.state();
    int env_ref = it->second;

    lua_rawgeti(L, LUA_REGISTRYINDEX, env_ref);  // stack: [env]
    lua_getfield(L, -1, hook_name.c_str());       // stack: [env, fn|nil]

    if (lua_isnil(L, -1)) {
        lua_pop(L, 2);
        return true;
    }

    lua_pushinteger(L, static_cast<lua_Integer>(arg1));  // stack: [env, fn, arg1]
    lua_pushnumber(L, arg2);                              // stack: [env, fn, arg1, arg2]

    if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
        std::string error = lua_tostring(L, -1);
        lua_pop(L, 1);  // pop error
        lua_pop(L, 1);  // pop env
        log_error(entity, hook_name, error);
        return false;
    }

    lua_pop(L, 1);  // pop env
    return true;
}

void ScriptSystem::log_error(Entity entity, const std::string& context, const std::string& error) {
    std::cerr << "[ScriptSystem] Entity " << entity << ": " << context << ": " << error << std::endl;
}

void ScriptSystem::update(ComponentStorage& storage, EntityManager& entity_manager, Blackboard& blackboard) {
    lua_State* L = lua_manager_.state();

    // Register bindings once
    if (!bindings_registered_) {
        register_bindings(L);
        bindings_registered_ = true;
    }

    // Store current engine pointers each frame
    store_engine_pointers(L, &storage, &entity_manager, &blackboard);

    double dt = blackboard.get_or<double>("delta_time", 0.0);

    auto scripted_entities = storage.entities_with_component<Script>();

    for (Entity entity : scripted_entities) {
        auto script_opt = storage.get_component<Script>(entity);
        if (!script_opt.has_value()) {
            continue;
        }
        Script& script = script_opt->get();

        if (!script.initialized) {
            // Build the full path to the script file
            std::string filepath = project_paths::assets_dir() + "/" + script.filename;

            // Create a per-entity Lua environment
            int env_ref = create_environment(entity);

            lua_State* L = lua_manager_.state();

            // Load the script file
            int load_status = luaL_loadfile(L, filepath.c_str());
            if (load_status != LUA_OK) {
                std::string error = lua_tostring(L, -1);
                lua_pop(L, 1);
                log_error(entity, "failed to load '" + script.filename + "'", error);
                continue;
            }

            // Set the environment as the chunk's _ENV upvalue
            lua_rawgeti(L, LUA_REGISTRYINDEX, env_ref);  // stack: [chunk, env]
            lua_setupvalue(L, -2, 1);                      // chunk's _ENV = env; stack: [chunk]

            // Execute the chunk to define functions in the environment
            if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
                std::string error = lua_tostring(L, -1);
                lua_pop(L, 1);
                log_error(entity, "failed to execute '" + script.filename + "'", error);
                continue;
            }

            // Call on_init(entity_id) if it exists
            call_hook(entity, "on_init", entity);

            script.initialized = true;
        }

        // Call on_update(entity_id, dt) every frame for initialized scripts
        if (script.initialized) {
            call_hook(entity, "on_update", entity, dt);
        }

        // Call on_collision(entity_id, other_id) for each collision partner
        if (storage.has_component<CollidedWith>(entity)) {
            auto collided_opt = storage.get_component<CollidedWith>(entity);
            if (collided_opt.has_value()) {
                const CollidedWith& collided = collided_opt->get();
                for (Entity other : collided.entities) {
                    call_hook(entity, "on_collision", entity, static_cast<double>(other));
                }
            }
        }

        // Call on_destroy(entity_id) if entity is marked for destruction
        if (storage.has_component<DestroyRequest>(entity)) {
            call_hook(entity, "on_destroy", entity);
        }
    }
}
