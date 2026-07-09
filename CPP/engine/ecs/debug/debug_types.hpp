#ifndef ENGINE_DEBUG_TYPES_HPP
#define ENGINE_DEBUG_TYPES_HPP

#include <any>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

// Forward declarations — Entity is uint32_t (defined in components.hpp)
using Entity = uint32_t;
class ComponentStorage;

namespace engine::debug {

// ---------------------------------------------------------------------------
// Value — lightweight tagged union for debug serialization
// ---------------------------------------------------------------------------
// Lightweight tagged union in the engine namespace for debug serialization.
// The JSONSerializer uses this to represent
// component field values, blackboard entries, and nested objects/arrays.

struct Value {
    enum class Type { Int, Float, Bool, String, Object, Array, Null };

    Type type = Type::Null;

    std::variant<
        int64_t,                                                    // Int
        double,                                                     // Float
        bool,                                                       // Bool
        std::string,                                                // String
        std::unordered_map<std::string, std::unique_ptr<Value>>,   // Object
        std::vector<std::unique_ptr<Value>>,                       // Array
        std::monostate                                              // Null
    > data = std::monostate{};
};

// ---------------------------------------------------------------------------
// IBlackboardAccessor — read-only blackboard access for serialization
// ---------------------------------------------------------------------------
// Read-only interface — the serializer only reads blackboard state,
// never writes it.

class IBlackboardAccessor {
public:
    virtual ~IBlackboardAccessor() = default;
    virtual Value get_value(const std::string& key) const = 0;
    virtual std::vector<std::string> get_all_keys() const = 0;
};

// ---------------------------------------------------------------------------
// IComponentBridge — type-erased component access for serialization
// ---------------------------------------------------------------------------
// Minimal interface for serialization — only get_as_any() and has()
// are needed by the JSONSerializer.

class IComponentBridge {
public:
    virtual ~IComponentBridge() = default;

    /**
     * Get a component value as std::any, or std::nullopt if the entity lacks it.
     */
    virtual std::optional<std::any> get_as_any(Entity entity, ComponentStorage& cs) const = 0;

    /**
     * Check whether the entity has this component type.
     */
    virtual bool has(Entity entity, const ComponentStorage& cs) const = 0;
};

// ---------------------------------------------------------------------------
// IPropertyAccessor — component registry for serialization
// ---------------------------------------------------------------------------
// Component registry interface. The serializer calls
// registered_components() to iterate over all component types.

class IPropertyAccessor {
public:
    virtual ~IPropertyAccessor() = default;

    /**
     * Get all registered component names and their bridges.
     * Returns pairs in alphabetical order by component name.
     */
    virtual std::vector<std::pair<std::string, const IComponentBridge*>> registered_components() const = 0;
};

// ---------------------------------------------------------------------------
// ITypeIntrospector — type metadata for field-level serialization
// ---------------------------------------------------------------------------
// Type metadata interface for field-level serialization:
// has_type(), get_type() (to check Kind::Struct and iterate fields),
// and cpp_to_value() (to convert a component std::any to a Value).

struct FieldInfo {
    std::string name;
    std::string type_name;
};

struct TypeInfo {
    std::string name;
    enum class Kind { Struct, Other };
    Kind kind;
    std::vector<FieldInfo> fields;
};

class ITypeIntrospector {
public:
    virtual ~ITypeIntrospector() = default;

    /**
     * Query if a type is registered.
     */
    virtual bool has_type(const std::string& type_name) const = 0;

    /**
     * Get type metadata. Throws if type not found.
     */
    virtual const TypeInfo& get_type(const std::string& type_name) const = 0;

    /**
     * Convert a C++ value (from std::any) to a debug Value for serialization.
     */
    virtual Value cpp_to_value(const std::any& cpp_value, const std::string& type_name) const = 0;
};

// ---------------------------------------------------------------------------
// IEntityMapper — logical-to-actual entity ID mapping for serialization
// ---------------------------------------------------------------------------
// Entity ID mapping interface. The serializer calls get_logical_ids()
// and get_actual_id().

class IEntityMapper {
public:
    virtual ~IEntityMapper() = default;

    /**
     * Get all currently mapped logical IDs.
     */
    virtual std::vector<int> get_logical_ids() const = 0;

    /**
     * Get actual Entity ID from logical ID.
     */
    virtual Entity get_actual_id(int logical_id) const = 0;
};

} // namespace engine::debug

#endif // ENGINE_DEBUG_TYPES_HPP
