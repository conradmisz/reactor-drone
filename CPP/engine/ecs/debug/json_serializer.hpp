#ifndef ENGINE_DEBUG_JSON_SERIALIZER_HPP
#define ENGINE_DEBUG_JSON_SERIALIZER_HPP

#include "debug_types.hpp"
#include <string>
#include <vector>

// Forward declaration
class ComponentStorage;

namespace engine::debug {

/**
 * JSONSerializer - Serializes complete ECS state to JSON strings.
 *
 * Lives in the engine so that both the command-line debug system and a future
 * in-game DebugSystem (F1 pause, N step, J dump) can share the same
 * serialization code.
 *
 * Dependencies are injected via constructor, making the serializer agnostic
 * to the consumer. Uses hand-written JSON string building (no external library).
 *
 * Iterates IPropertyAccessor::registered_components() to discover all component
 * types, then for each entity checks bridge.has() and serializes fields using
 * ITypeIntrospector metadata and field descriptors.
 */
class JSONSerializer {
public:
    /**
     * Constructor injection of dependencies.
     * The command-line debug system and a future DebugSystem wire these up
     * differently.
     *
     * @param accessor      IPropertyAccessor with registered component bridges
     * @param introspector  ITypeIntrospector with struct metadata for field introspection
     * @param mapper        IEntityMapper for logical-to-actual ID mapping
     * @param blackboard    IBlackboardAccessor for blackboard state (may be null)
     */
    JSONSerializer(const IPropertyAccessor& accessor,
                   const ITypeIntrospector& introspector,
                   const IEntityMapper& mapper,
                   IBlackboardAccessor* blackboard);

    /**
     * Serialize complete ECS state to a JSON string.
     *
     * Produces a JSON object with top-level keys:
     *   frame, scenario, feature, phase, timestamp, entities, blackboard
     *
     * @param frame     Current frame number
     * @param scenario  Scenario name
     * @param feature   Feature name
     * @param phase     "before", "after", or system name
     * @param cs        ComponentStorage to read entity data from
     */
    std::string serialize_state(size_t frame,
                                const std::string& scenario,
                                const std::string& feature,
                                const std::string& phase,
                                ComponentStorage& cs) const;

    /**
     * Serialize a trace summary to JSON string.
     *
     * Produces a JSON object with keys:
     *   frame, scenario, feature, systems_executed, total_files, timestamp
     */
    std::string serialize_trace_summary(size_t frame,
                                        const std::string& scenario,
                                        const std::string& feature,
                                        const std::vector<std::string>& systems,
                                        size_t total_files,
                                        const std::string& timestamp) const;

private:
    const IPropertyAccessor& accessor_;
    const ITypeIntrospector& introspector_;
    const IEntityMapper& mapper_;
    IBlackboardAccessor* blackboard_;

    /**
     * Serialize a single entity's components to a JSON object string.
     */
    std::string serialize_entity(Entity actual_id, int logical_id,
                                 ComponentStorage& cs) const;

    /**
     * Serialize a single component's fields to a JSON object string.
     */
    std::string serialize_component(const std::string& name,
                                    const IComponentBridge& bridge,
                                    Entity actual_id,
                                    ComponentStorage& cs) const;

    /**
     * Escape special characters in a string for JSON output.
     */
    static std::string escape_json_string(const std::string& s);

    /**
     * Convert a Value to a JSON string representation.
     */
    std::string value_to_json(const Value& v) const;

    /**
     * Get current timestamp as "YYYY-MM-DDTHH-MM-SS".
     */
    static std::string current_timestamp();
};

} // namespace engine::debug

#endif // ENGINE_DEBUG_JSON_SERIALIZER_HPP
