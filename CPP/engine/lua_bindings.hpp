#ifndef LUA_BINDINGS_HPP
#define LUA_BINDINGS_HPP

struct lua_State;
class ComponentStorage;
class EntityManager;
class Blackboard;

/// Register all engine.* functions into the Lua global table.
/// Call once per Lua state.
void register_bindings(lua_State* L);

/// Store engine pointers in the Lua registry as light userdata.
/// Call at the start of each ScriptSystem::update().
void store_engine_pointers(lua_State* L,
                           ComponentStorage* storage,
                           EntityManager* entity_manager,
                           Blackboard* blackboard);

#endif // LUA_BINDINGS_HPP
