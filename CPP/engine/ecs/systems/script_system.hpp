#ifndef SCRIPT_SYSTEM_HPP
#define SCRIPT_SYSTEM_HPP

#include <unordered_map>
#include <string>
#include "engine/ecs/components.hpp"

class LuaManager;
class ComponentStorage;
class EntityManager;
class Blackboard;

/**
 * ScriptSystem — dispatches Lua lifecycle hooks to scripted entities.
 *
 * For each entity with a Script component:
 *   - If not initialized: load script, create per-entity environment,
 *     call on_init(entity_id), set initialized = true
 *   - If initialized: call on_update(entity_id, dt)
 *   - If entity has CollidedWith: call on_collision(entity_id, other_id)
 *     for each collision partner
 *   - If entity has DestroyRequest: call on_destroy(entity_id)
 *
 * All Lua calls use lua_pcall for protected execution. Errors are
 * logged to stderr and do not interrupt processing of other entities.
 */
class ScriptSystem {
public:
    explicit ScriptSystem(LuaManager& lua_manager);

    /// Process all scripted entities for one frame
    void update(ComponentStorage& storage, EntityManager& entity_manager, Blackboard& blackboard);

private:
    LuaManager& lua_manager_;
    bool bindings_registered_ = false;

    /// Per-entity Lua environment table references (Lua registry refs)
    std::unordered_map<Entity, int> entity_environments_;

    /// Create a new Lua environment table for an entity, stored in the registry
    int create_environment(Entity entity);

    /// Call a Lua function within an entity's environment (no extra args)
    bool call_hook(Entity entity, const std::string& hook_name);

    /// Call a Lua function within an entity's environment (one integer arg)
    bool call_hook(Entity entity, const std::string& hook_name, Entity arg1);

    /// Call a Lua function within an entity's environment (integer + double args)
    bool call_hook(Entity entity, const std::string& hook_name, Entity arg1, double arg2);

    /// Log a Lua error to stderr
    void log_error(Entity entity, const std::string& context, const std::string& error);
};

#endif // SCRIPT_SYSTEM_HPP
