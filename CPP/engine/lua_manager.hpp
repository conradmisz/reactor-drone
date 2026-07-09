#ifndef LUA_MANAGER_HPP
#define LUA_MANAGER_HPP

#include <string>

// Forward declare — Lua headers are C, included only in the .cpp
struct lua_State;

/**
 * LuaManager — RAII owner of a lua_State*.
 *
 * Creates a Lua state on construction, closes it on destruction.
 * Provides methods to load and execute .lua files with error reporting.
 * Enforces sandboxing by removing dangerous standard libraries.
 *
 * Non-copyable, move-only.
 */
class LuaManager {
public:
    /// Result type for script operations
    struct Result {
        bool success;
        std::string error_message;  // Empty on success
    };

    LuaManager();
    ~LuaManager();

    // Non-copyable
    LuaManager(const LuaManager&) = delete;
    LuaManager& operator=(const LuaManager&) = delete;

    // Movable
    LuaManager(LuaManager&& other) noexcept;
    LuaManager& operator=(LuaManager&& other) noexcept;

    /// Load and execute a .lua file. Returns error info on failure.
    Result load_script(const std::string& filepath);

    /// Execute a string of Lua code. Returns error info on failure.
    Result execute_string(const std::string& code);

    /// Access the raw lua_State* (for ScriptSystem to push/call functions)
    lua_State* state() const { return L_; }

private:
    lua_State* L_ = nullptr;

    /// Remove dangerous libraries (os, io, loadfile, dofile, require)
    void apply_sandbox();
};

#endif // LUA_MANAGER_HPP
