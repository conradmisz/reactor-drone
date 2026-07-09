#include "json_serializer.hpp"
#include "engine/ecs/component_storage.hpp"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace engine::debug {

JSONSerializer::JSONSerializer(const IPropertyAccessor& accessor,
                               const ITypeIntrospector& introspector,
                               const IEntityMapper& mapper,
                               IBlackboardAccessor* blackboard)
    : accessor_(accessor)
    , introspector_(introspector)
    , mapper_(mapper)
    , blackboard_(blackboard) {
}

std::string JSONSerializer::serialize_state(size_t frame,
                                            const std::string& scenario,
                                            const std::string& feature,
                                            const std::string& phase,
                                            ComponentStorage& cs) const {
    std::string json = "{\n";
    json += "  \"frame\": " + std::to_string(frame) + ",\n";
    json += "  \"scenario\": \"" + escape_json_string(scenario) + "\",\n";
    json += "  \"feature\": \"" + escape_json_string(feature) + "\",\n";
    json += "  \"phase\": \"" + escape_json_string(phase) + "\",\n";
    json += "  \"timestamp\": \"" + current_timestamp() + "\",\n";

    // Serialize blackboard
    json += "  \"blackboard\": {";
    if (blackboard_) {
        auto keys = blackboard_->get_all_keys();
        std::sort(keys.begin(), keys.end());
        bool first_key = true;
        for (const auto& key : keys) {
            try {
                Value val = blackboard_->get_value(key);
                if (!first_key) json += ",";
                json += "\n    \"" + escape_json_string(key) + "\": ";
                json += value_to_json(val);
                first_key = false;
            } catch (...) {
                // Skip keys that can't be retrieved
            }
        }
        if (!first_key) json += "\n  ";
    }
    json += "},\n";

    // Serialize entities
    json += "  \"entities\": {";

    // Get all logical IDs and sort them for deterministic output
    auto logical_ids = mapper_.get_logical_ids();
    std::sort(logical_ids.begin(), logical_ids.end());

    bool first_entity = true;
    for (int logical_id : logical_ids) {
        Entity actual_id = mapper_.get_actual_id(logical_id);
        std::string entity_json = serialize_entity(actual_id, logical_id, cs);
        if (!entity_json.empty()) {
            if (!first_entity) json += ",";
            json += "\n    \"" + std::to_string(logical_id) + "\": " + entity_json;
            first_entity = false;
        }
    }
    if (!first_entity) json += "\n  ";
    json += "}\n";
    json += "}";
    return json;
}

std::string JSONSerializer::serialize_trace_summary(size_t frame,
                                                     const std::string& scenario,
                                                     const std::string& feature,
                                                     const std::vector<std::string>& systems,
                                                     size_t total_files,
                                                     const std::string& timestamp) const {
    std::string json = "{\n";
    json += "  \"frame\": " + std::to_string(frame) + ",\n";
    json += "  \"scenario\": \"" + escape_json_string(scenario) + "\",\n";
    json += "  \"feature\": \"" + escape_json_string(feature) + "\",\n";
    json += "  \"systems_executed\": [";
    for (size_t i = 0; i < systems.size(); ++i) {
        if (i > 0) json += ", ";
        json += "\"" + escape_json_string(systems[i]) + "\"";
    }
    json += "],\n";
    json += "  \"total_files\": " + std::to_string(total_files) + ",\n";
    json += "  \"timestamp\": \"" + escape_json_string(timestamp) + "\"\n";
    json += "}";
    return json;
}

std::string JSONSerializer::serialize_entity(Entity actual_id, int logical_id,
                                              ComponentStorage& cs) const {
    auto components = accessor_.registered_components();

    std::string json = "{\n";
    json += "      \"logical_id\": " + std::to_string(logical_id) + ",\n";
    json += "      \"actual_id\": " + std::to_string(actual_id) + ",\n";
    json += "      \"components\": {";

    bool first_component = true;
    for (const auto& [comp_name, bridge] : components) {
        if (!bridge->has(actual_id, cs)) continue;

        std::string comp_json = serialize_component(comp_name, *bridge, actual_id, cs);
        if (!comp_json.empty()) {
            if (!first_component) json += ",";
            json += "\n        \"" + escape_json_string(comp_name) + "\": " + comp_json;
            first_component = false;
        }
    }
    if (!first_component) json += "\n      ";
    json += "}\n";
    json += "    }";
    return json;
}

std::string JSONSerializer::serialize_component(const std::string& name,
                                                 const IComponentBridge& bridge,
                                                 Entity actual_id,
                                                 ComponentStorage& cs) const {
    // Get the component as std::any
    auto component_opt = bridge.get_as_any(actual_id, cs);
    if (!component_opt.has_value()) return "";

    const std::any& component = component_opt.value();

    // Get the struct metadata to iterate fields
    if (!introspector_.has_type(name)) return "";
    const auto& metadata = introspector_.get_type(name);
    if (metadata.kind != TypeInfo::Kind::Struct) return "";

    // Convert the whole component to a Value (which will be an Object)
    Value comp_value = introspector_.cpp_to_value(component, name);
    if (comp_value.type != Value::Type::Object) return "";

    // Serialize the object fields
    const auto& obj = std::get<std::unordered_map<std::string, std::unique_ptr<Value>>>(comp_value.data);

    // Sort field names for deterministic output
    std::vector<std::string> field_names;
    field_names.reserve(obj.size());
    for (const auto& [field_name, _] : obj) {
        field_names.push_back(field_name);
    }
    std::sort(field_names.begin(), field_names.end());

    std::string json = "{";
    bool first_field = true;
    for (const auto& field_name : field_names) {
        auto it = obj.find(field_name);
        if (it == obj.end()) continue;

        if (!first_field) json += ", ";
        json += "\"" + escape_json_string(field_name) + "\": ";
        json += value_to_json(*it->second);
        first_field = false;
    }
    json += "}";
    return json;
}

std::string JSONSerializer::escape_json_string(const std::string& s) {
    std::string result;
    result.reserve(s.size() + 8);  // Small extra for common escapes
    for (char c : s) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b";  break;
            case '\f': result += "\\f";  break;
            case '\n': result += "\\n";  break;
            case '\r': result += "\\r";  break;
            case '\t': result += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    // Control characters: encode as \u00XX
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x",
                                  static_cast<unsigned int>(static_cast<unsigned char>(c)));
                    result += buf;
                } else {
                    result += c;
                }
                break;
        }
    }
    return result;
}

std::string JSONSerializer::value_to_json(const Value& v) const {
    switch (v.type) {
        case Value::Type::Int:
            return std::to_string(std::get<int64_t>(v.data));

        case Value::Type::Float: {
            double d = std::get<double>(v.data);
            std::ostringstream oss;
            oss << d;
            std::string s = oss.str();
            // Ensure there's a decimal point so it reads as a float in JSON
            if (s.find('.') == std::string::npos && s.find('e') == std::string::npos) {
                s += ".0";
            }
            return s;
        }

        case Value::Type::Bool:
            return std::get<bool>(v.data) ? "true" : "false";

        case Value::Type::String:
            return "\"" + escape_json_string(std::get<std::string>(v.data)) + "\"";

        case Value::Type::Object: {
            const auto& obj = std::get<std::unordered_map<std::string, std::unique_ptr<Value>>>(v.data);
            // Sort keys for deterministic output
            std::vector<std::string> keys;
            keys.reserve(obj.size());
            for (const auto& [k, _] : obj) {
                keys.push_back(k);
            }
            std::sort(keys.begin(), keys.end());

            std::string json = "{";
            bool first = true;
            for (const auto& k : keys) {
                auto it = obj.find(k);
                if (it == obj.end()) continue;
                if (!first) json += ", ";
                json += "\"" + escape_json_string(k) + "\": ";
                json += value_to_json(*it->second);
                first = false;
            }
            json += "}";
            return json;
        }

        case Value::Type::Array: {
            const auto& arr = std::get<std::vector<std::unique_ptr<Value>>>(v.data);
            std::string json = "[";
            for (size_t i = 0; i < arr.size(); ++i) {
                if (i > 0) json += ", ";
                json += value_to_json(*arr[i]);
            }
            json += "]";
            return json;
        }

        case Value::Type::Null:
            return "null";
    }
    return "null";
}

std::string JSONSerializer::current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_buf);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H-%M-%S", &tm_buf);
    return std::string(buf);
}

} // namespace engine::debug
