#ifndef DEBUG_ADAPTERS_HPP
#define DEBUG_ADAPTERS_HPP

/**
 * debug_adapters.hpp — Game-specific adapter classes for the engine's
 * debug serialization interfaces.
 *
 * Bridges Class-090's 26 component types to the engine's generic
 * IPropertyAccessor, IComponentBridge, ITypeIntrospector, IEntityMapper,
 * and IBlackboardAccessor interfaces used by JSONSerializer.
 */

#include "engine/ecs/debug/debug_types.hpp"
#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/component_storage.hpp"
#include <algorithm>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

using namespace engine::debug;

// ---------------------------------------------------------------------------
// BlackboardAccessorAdapter
// ---------------------------------------------------------------------------

class BlackboardAccessorAdapter : public IBlackboardAccessor {
public:
    explicit BlackboardAccessorAdapter(Blackboard& blackboard)
        : blackboard_(blackboard) {}

    std::vector<std::string> get_all_keys() const override {
        return blackboard_.get_all_keys();
    }

    Value get_value(const std::string& key) const override {
        Value result;

        // Try int
        try {
            int val = blackboard_.get<int>(key);
            result.type = Value::Type::Int;
            result.data = static_cast<int64_t>(val);
            return result;
        } catch (...) {}

        // Try float
        try {
            float val = blackboard_.get<float>(key);
            result.type = Value::Type::Float;
            result.data = static_cast<double>(val);
            return result;
        } catch (...) {}

        // Try double
        try {
            double val = blackboard_.get<double>(key);
            result.type = Value::Type::Float;
            result.data = val;
            return result;
        } catch (...) {}

        // Try bool
        try {
            bool val = blackboard_.get<bool>(key);
            result.type = Value::Type::Bool;
            result.data = val;
            return result;
        } catch (...) {}

        // Try string
        try {
            std::string val = blackboard_.get<std::string>(key);
            result.type = Value::Type::String;
            result.data = val;
            return result;
        } catch (...) {}

        // Try uint64_t
        try {
            uint64_t val = blackboard_.get<uint64_t>(key);
            result.type = Value::Type::Int;
            result.data = static_cast<int64_t>(val);
            return result;
        } catch (...) {}

        throw std::runtime_error(
            "Blackboard key '" + key + "' has unsupported type");
    }

private:
    Blackboard& blackboard_;
};

// ---------------------------------------------------------------------------
// ComponentBridgeAdapter<T>
// ---------------------------------------------------------------------------

template<typename T>
class ComponentBridgeAdapter : public IComponentBridge {
public:
    bool has(Entity entity, const ComponentStorage& cs) const override {
        return cs.has_component<T>(entity);
    }

    std::optional<std::any> get_as_any(Entity entity,
                                        ComponentStorage& cs) const override {
        auto opt = cs.get_component<T>(entity);
        if (!opt.has_value()) return std::nullopt;
        return std::any(opt->get());
    }
};

// ---------------------------------------------------------------------------
// PropertyAccessorAdapter
// ---------------------------------------------------------------------------

class PropertyAccessorAdapter : public IPropertyAccessor {
public:
    template<typename T>
    void register_component(const std::string& name) {
        bridges_.push_back(
            {name, std::make_unique<ComponentBridgeAdapter<T>>()});
    }

    std::vector<std::pair<std::string, const IComponentBridge*>>
    registered_components() const override {
        std::vector<std::pair<std::string, const IComponentBridge*>> result;
        result.reserve(bridges_.size());
        for (const auto& [name, bridge] : bridges_) {
            result.push_back({name, bridge.get()});
        }
        std::sort(result.begin(), result.end(),
                  [](const auto& a, const auto& b) {
                      return a.first < b.first;
                  });
        return result;
    }

private:
    std::vector<std::pair<std::string,
                          std::unique_ptr<IComponentBridge>>> bridges_;
};

// ---------------------------------------------------------------------------
// TypeIntrospectorAdapter
// ---------------------------------------------------------------------------

class TypeIntrospectorAdapter : public ITypeIntrospector {
public:
    using ValueConverter = std::function<Value(const std::any&)>;

    void register_struct(const std::string& name,
                         std::vector<FieldInfo> fields,
                         ValueConverter converter) {
        TypeInfo info;
        info.name = name;
        info.kind = TypeInfo::Kind::Struct;
        info.fields = std::move(fields);
        types_[name] = std::move(info);
        converters_[name] = std::move(converter);
    }

    bool has_type(const std::string& type_name) const override {
        return types_.find(type_name) != types_.end();
    }

    const TypeInfo& get_type(const std::string& type_name) const override {
        auto it = types_.find(type_name);
        if (it == types_.end()) {
            throw std::runtime_error(
                "TypeIntrospectorAdapter: unknown type '" + type_name + "'");
        }
        return it->second;
    }

    Value cpp_to_value(const std::any& cpp_value,
                       const std::string& type_name) const override {
        auto it = converters_.find(type_name);
        if (it == converters_.end()) {
            throw std::runtime_error(
                "TypeIntrospectorAdapter: no converter for '" + type_name + "'");
        }
        return it->second(cpp_value);
    }

private:
    std::unordered_map<std::string, TypeInfo> types_;
    std::unordered_map<std::string, ValueConverter> converters_;
};

// ---------------------------------------------------------------------------
// EntityMapperAdapter
// ---------------------------------------------------------------------------

class EntityMapperAdapter : public IEntityMapper {
public:
    void map(int logical_id, Entity actual_id) {
        mapping_[logical_id] = actual_id;
    }

    void clear() {
        mapping_.clear();
    }

    std::vector<int> get_logical_ids() const override {
        std::vector<int> ids;
        ids.reserve(mapping_.size());
        for (const auto& [lid, _] : mapping_) {
            ids.push_back(lid);
        }
        return ids;
    }

    Entity get_actual_id(int logical_id) const override {
        return mapping_.at(logical_id);
    }

private:
    std::unordered_map<int, Entity> mapping_;
};

// ---------------------------------------------------------------------------
// Value construction helpers
// ---------------------------------------------------------------------------

inline std::unique_ptr<Value> make_float_value(double v) {
    auto val = std::make_unique<Value>();
    val->type = Value::Type::Float;
    val->data = v;
    return val;
}

inline std::unique_ptr<Value> make_int_value(int64_t v) {
    auto val = std::make_unique<Value>();
    val->type = Value::Type::Int;
    val->data = v;
    return val;
}

inline std::unique_ptr<Value> make_bool_value(bool v) {
    auto val = std::make_unique<Value>();
    val->type = Value::Type::Bool;
    val->data = v;
    return val;
}

inline std::unique_ptr<Value> make_string_value(const std::string& v) {
    auto val = std::make_unique<Value>();
    val->type = Value::Type::String;
    val->data = v;
    return val;
}

// ---------------------------------------------------------------------------
// refresh_entity_mapper helper
// ---------------------------------------------------------------------------

/**
 * Clear and repopulate the EntityMapper from current active entities.
 * Collects all entity IDs that have at least one component, sorts them
 * in ascending order, and assigns logical IDs 0, 1, 2...
 */
inline void refresh_entity_mapper(EntityMapperAdapter& mapper,
                                   ComponentStorage& cs) {
    mapper.clear();

    // Collect all unique entity IDs from all component storages
    std::unordered_map<Entity, bool> seen;

    auto collect = [&](const std::vector<Entity>& entities) {
        for (Entity e : entities) {
            seen[e] = true;
        }
    };

    collect(cs.entities_with_component<Position>());
    collect(cs.entities_with_component<Size>());
    collect(cs.entities_with_component<Color>());
    collect(cs.entities_with_component<Input>());
    collect(cs.entities_with_component<Velocity>());
    collect(cs.entities_with_component<Images>());
    collect(cs.entities_with_component<Text>());
    collect(cs.entities_with_component<ScreenPosition>());
    collect(cs.entities_with_component<Rotation>());
    collect(cs.entities_with_component<Collider>());
    collect(cs.entities_with_component<CircleCollider>());
    collect(cs.entities_with_component<OBBCollider>());
    collect(cs.entities_with_component<Lifetime>());
    collect(cs.entities_with_component<WrapAround>());
    collect(cs.entities_with_component<DestroyRequest>());
    collect(cs.entities_with_component<CollidedWith>());
    collect(cs.entities_with_component<Script>());
    collect(cs.entities_with_component<SpriteSheet>());
    collect(cs.entities_with_component<Animation>());
    collect(cs.entities_with_component<EnemyTag>());
    collect(cs.entities_with_component<PathFollower>());
    collect(cs.entities_with_component<Health>());
    collect(cs.entities_with_component<TowerTag>());
    collect(cs.entities_with_component<TowerStats>());
    collect(cs.entities_with_component<ProjectileTag>());
    collect(cs.entities_with_component<ProjectileData>());

    // Sort entity IDs in ascending order
    std::vector<Entity> sorted_ids;
    sorted_ids.reserve(seen.size());
    for (const auto& [id, _] : seen) {
        sorted_ids.push_back(id);
    }
    std::sort(sorted_ids.begin(), sorted_ids.end());

    // Assign logical IDs 0, 1, 2...
    for (size_t i = 0; i < sorted_ids.size(); ++i) {
        mapper.map(static_cast<int>(i), sorted_ids[i]);
    }
}

// ---------------------------------------------------------------------------
// register_all_components declaration
// ---------------------------------------------------------------------------

/**
 * Register all Class-090 component types with the PropertyAccessor and
 * TypeIntrospector adapters. Called once during initialization.
 */
void register_all_components(PropertyAccessorAdapter& accessor,
                             TypeIntrospectorAdapter& introspector);

#endif // DEBUG_ADAPTERS_HPP
