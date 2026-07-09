#include "debug_adapters.hpp"
#include "engine/ecs/components.hpp"
#include "enemy_components.hpp"
#include "tower_components.hpp"

void register_all_components(PropertyAccessorAdapter& accessor,
                             TypeIntrospectorAdapter& introspector) {
    // --- Register all 26 component types with PropertyAccessorAdapter ---
    accessor.register_component<Animation>("Animation");
    accessor.register_component<CircleCollider>("CircleCollider");
    accessor.register_component<Collider>("Collider");
    accessor.register_component<CollidedWith>("CollidedWith");
    accessor.register_component<Color>("Color");
    accessor.register_component<DestroyRequest>("DestroyRequest");
    accessor.register_component<EnemyTag>("EnemyTag");
    accessor.register_component<Health>("Health");
    accessor.register_component<Images>("Images");
    accessor.register_component<Input>("Input");
    accessor.register_component<Lifetime>("Lifetime");
    accessor.register_component<OBBCollider>("OBBCollider");
    accessor.register_component<PathFollower>("PathFollower");
    accessor.register_component<Position>("Position");
    accessor.register_component<ProjectileData>("ProjectileData");
    accessor.register_component<ProjectileTag>("ProjectileTag");
    accessor.register_component<Rotation>("Rotation");
    accessor.register_component<ScreenPosition>("ScreenPosition");
    accessor.register_component<Script>("Script");
    accessor.register_component<Size>("Size");
    accessor.register_component<SpriteSheet>("SpriteSheet");
    accessor.register_component<Text>("Text");
    accessor.register_component<TowerStats>("TowerStats");
    accessor.register_component<TowerTag>("TowerTag");
    accessor.register_component<Velocity>("Velocity");
    accessor.register_component<WrapAround>("WrapAround");

    // --- Register all 26 component types with TypeIntrospectorAdapter ---

    // Animation
    introspector.register_struct("Animation",
        {{"current_frame", "int"}, {"start_frame", "int"}, {"frame_count", "int"},
         {"frame_duration", "float"}, {"elapsed", "float"},
         {"looping", "bool"}, {"playing", "bool"}, {"finished", "bool"}},
        [](const std::any& v) -> Value {
            const auto& a = std::any_cast<const Animation&>(v);
            Value val;
            val.type = Value::Type::Object;
            std::unordered_map<std::string, std::unique_ptr<Value>> obj;
            obj["current_frame"] = make_int_value(static_cast<int64_t>(a.current_frame));
            obj["start_frame"] = make_int_value(static_cast<int64_t>(a.start_frame));
            obj["frame_count"] = make_int_value(static_cast<int64_t>(a.frame_count));
            obj["frame_duration"] = make_float_value(static_cast<double>(a.frame_duration));
            obj["elapsed"] = make_float_value(static_cast<double>(a.elapsed));
            obj["looping"] = make_bool_value(a.looping);
            obj["playing"] = make_bool_value(a.playing);
            obj["finished"] = make_bool_value(a.finished);
            val.data = std::move(obj);
            return val;
        });

    // CircleCollider
    introspector.register_struct("CircleCollider",
        {{"radius", "float"}, {"offset_x", "float"}, {"offset_y", "float"}},
        [](const std::any& v) -> Value {
            const auto& c = std::any_cast<const CircleCollider&>(v);
            Value val;
            val.type = Value::Type::Object;
            std::unordered_map<std::string, std::unique_ptr<Value>> obj;
            obj["radius"] = make_float_value(static_cast<double>(c.radius));
            obj["offset_x"] = make_float_value(static_cast<double>(c.offset_x));
            obj["offset_y"] = make_float_value(static_cast<double>(c.offset_y));
            val.data = std::move(obj);
            return val;
        });

    // Collider
    introspector.register_struct("Collider",
        {{"width", "float"}, {"height", "float"}, {"layer", "int"}, {"mask", "int"}},
        [](const std::any& v) -> Value {
            const auto& c = std::any_cast<const Collider&>(v);
            Value val;
            val.type = Value::Type::Object;
            std::unordered_map<std::string, std::unique_ptr<Value>> obj;
            obj["width"] = make_float_value(static_cast<double>(c.width));
            obj["height"] = make_float_value(static_cast<double>(c.height));
            obj["layer"] = make_int_value(static_cast<int64_t>(c.layer));
            obj["mask"] = make_int_value(static_cast<int64_t>(c.mask));
            val.data = std::move(obj);
            return val;
        });

    // CollidedWith
    introspector.register_struct("CollidedWith",
        {{"entities", "array"}},
        [](const std::any& v) -> Value {
            const auto& c = std::any_cast<const CollidedWith&>(v);
            Value val;
            val.type = Value::Type::Object;
            std::unordered_map<std::string, std::unique_ptr<Value>> obj;
            // entities as Array of Int
            auto arr_val = std::make_unique<Value>();
            arr_val->type = Value::Type::Array;
            std::vector<std::unique_ptr<Value>> arr;
            for (Entity e : c.entities) {
                arr.push_back(make_int_value(static_cast<int64_t>(e)));
            }
            arr_val->data = std::move(arr);
            obj["entities"] = std::move(arr_val);
            val.data = std::move(obj);
            return val;
        });

    // Color
    introspector.register_struct("Color",
        {{"r", "int"}, {"g", "int"}, {"b", "int"}, {"a", "int"}},
        [](const std::any& v) -> Value {
            const auto& c = std::any_cast<const Color&>(v);
            Value val;
            val.type = Value::Type::Object;
            std::unordered_map<std::string, std::unique_ptr<Value>> obj;
            obj["r"] = make_int_value(static_cast<int64_t>(c.r));
            obj["g"] = make_int_value(static_cast<int64_t>(c.g));
            obj["b"] = make_int_value(static_cast<int64_t>(c.b));
            obj["a"] = make_int_value(static_cast<int64_t>(c.a));
            val.data = std::move(obj);
            return val;
        });

    // DestroyRequest (tag — empty object)
    introspector.register_struct("DestroyRequest", {},
        [](const std::any&) -> Value {
            Value val;
            val.type = Value::Type::Object;
            std::unordered_map<std::string, std::unique_ptr<Value>> obj;
            val.data = std::move(obj);
            return val;
        });

    // EnemyTag (tag — empty object)
    introspector.register_struct("EnemyTag", {},
        [](const std::any&) -> Value {
            Value val;
            val.type = Value::Type::Object;
            std::unordered_map<std::string, std::unique_ptr<Value>> obj;
            val.data = std::move(obj);
            return val;
        });

    // Health
    introspector.register_struct("Health",
        {{"current", "float"}, {"max_hp", "float"}, {"armor_multiplier", "float"}},
        [](const std::any& v) -> Value {
            const auto& h = std::any_cast<const Health&>(v);
            Value val;
            val.type = Value::Type::Object;
            std::unordered_map<std::string, std::unique_ptr<Value>> obj;
            obj["current"] = make_float_value(static_cast<double>(h.current));
            obj["max_hp"] = make_float_value(static_cast<double>(h.max_hp));
            obj["armor_multiplier"] = make_float_value(static_cast<double>(h.armor_multiplier));
            val.data = std::move(obj);
            return val;
        });

    // Images
    introspector.register_struct("Images",
        {{"filenames", "array"}, {"active_index", "int"}},
        [](const std::any& v) -> Value {
            const auto& img = std::any_cast<const Images&>(v);
            Value val;
            val.type = Value::Type::Object;
            std::unordered_map<std::string, std::unique_ptr<Value>> obj;
            obj["active_index"] = make_int_value(static_cast<int64_t>(img.active_index));
            // filenames as Array of String
            auto arr_val = std::make_unique<Value>();
            arr_val->type = Value::Type::Array;
            std::vector<std::unique_ptr<Value>> arr;
            for (const auto& fn : img.filenames) {
                arr.push_back(make_string_value(fn));
            }
            arr_val->data = std::move(arr);
            obj["filenames"] = std::move(arr_val);
            val.data = std::move(obj);
            return val;
        });

    // Input
    introspector.register_struct("Input",
        {{"up", "bool"}, {"down", "bool"}, {"left", "bool"},
         {"right", "bool"}, {"fire", "bool"}},
        [](const std::any& v) -> Value {
            const auto& inp = std::any_cast<const Input&>(v);
            Value val;
            val.type = Value::Type::Object;
            std::unordered_map<std::string, std::unique_ptr<Value>> obj;
            obj["up"] = make_bool_value(inp.up);
            obj["down"] = make_bool_value(inp.down);
            obj["left"] = make_bool_value(inp.left);
            obj["right"] = make_bool_value(inp.right);
            obj["fire"] = make_bool_value(inp.fire);
            val.data = std::move(obj);
            return val;
        });

    // Lifetime
    introspector.register_struct("Lifetime",
        {{"remaining", "float"}},
        [](const std::any& v) -> Value {
            const auto& l = std::any_cast<const Lifetime&>(v);
            Value val;
            val.type = Value::Type::Object;
            std::unordered_map<std::string, std::unique_ptr<Value>> obj;
            obj["remaining"] = make_float_value(static_cast<double>(l.remaining));
            val.data = std::move(obj);
            return val;
        });

    // OBBCollider
    introspector.register_struct("OBBCollider",
        {{"half_width", "float"}, {"half_height", "float"}},
        [](const std::any& v) -> Value {
            const auto& o = std::any_cast<const OBBCollider&>(v);
            Value val;
            val.type = Value::Type::Object;
            std::unordered_map<std::string, std::unique_ptr<Value>> obj;
            obj["half_width"] = make_float_value(static_cast<double>(o.half_width));
            obj["half_height"] = make_float_value(static_cast<double>(o.half_height));
            val.data = std::move(obj);
            return val;
        });

    // PathFollower
    introspector.register_struct("PathFollower",
        {{"waypoint_index", "int"}, {"progress", "float"}, {"speed", "float"}},
        [](const std::any& v) -> Value {
            const auto& p = std::any_cast<const PathFollower&>(v);
            Value val;
            val.type = Value::Type::Object;
            std::unordered_map<std::string, std::unique_ptr<Value>> obj;
            obj["waypoint_index"] = make_int_value(static_cast<int64_t>(p.waypoint_index));
            obj["progress"] = make_float_value(static_cast<double>(p.progress));
            obj["speed"] = make_float_value(static_cast<double>(p.speed));
            val.data = std::move(obj);
            return val;
        });

    // Position
    introspector.register_struct("Position",
        {{"x", "float"}, {"y", "float"}},
        [](const std::any& v) -> Value {
            const auto& p = std::any_cast<const Position&>(v);
            Value val;
            val.type = Value::Type::Object;
            std::unordered_map<std::string, std::unique_ptr<Value>> obj;
            obj["x"] = make_float_value(static_cast<double>(p.x));
            obj["y"] = make_float_value(static_cast<double>(p.y));
            val.data = std::move(obj);
            return val;
        });

    // ProjectileData
    introspector.register_struct("ProjectileData",
        {{"target", "int"}, {"speed", "float"}, {"damage", "float"}},
        [](const std::any& v) -> Value {
            const auto& p = std::any_cast<const ProjectileData&>(v);
            Value val;
            val.type = Value::Type::Object;
            std::unordered_map<std::string, std::unique_ptr<Value>> obj;
            obj["target"] = make_int_value(static_cast<int64_t>(p.target));
            obj["speed"] = make_float_value(static_cast<double>(p.speed));
            obj["damage"] = make_float_value(static_cast<double>(p.damage));
            val.data = std::move(obj);
            return val;
        });

    // ProjectileTag (tag — empty object)
    introspector.register_struct("ProjectileTag", {},
        [](const std::any&) -> Value {
            Value val;
            val.type = Value::Type::Object;
            std::unordered_map<std::string, std::unique_ptr<Value>> obj;
            val.data = std::move(obj);
            return val;
        });

    // Rotation
    introspector.register_struct("Rotation",
        {{"angle", "float"}, {"angular_velocity", "float"}},
        [](const std::any& v) -> Value {
            const auto& r = std::any_cast<const Rotation&>(v);
            Value val;
            val.type = Value::Type::Object;
            std::unordered_map<std::string, std::unique_ptr<Value>> obj;
            obj["angle"] = make_float_value(static_cast<double>(r.angle));
            obj["angular_velocity"] = make_float_value(static_cast<double>(r.angular_velocity));
            val.data = std::move(obj);
            return val;
        });

    // ScreenPosition
    introspector.register_struct("ScreenPosition",
        {{"x", "float"}, {"y", "float"}},
        [](const std::any& v) -> Value {
            const auto& sp = std::any_cast<const ScreenPosition&>(v);
            Value val;
            val.type = Value::Type::Object;
            std::unordered_map<std::string, std::unique_ptr<Value>> obj;
            obj["x"] = make_float_value(static_cast<double>(sp.x));
            obj["y"] = make_float_value(static_cast<double>(sp.y));
            val.data = std::move(obj);
            return val;
        });

    // Script
    introspector.register_struct("Script",
        {{"filename", "string"}, {"initialized", "bool"}},
        [](const std::any& v) -> Value {
            const auto& s = std::any_cast<const Script&>(v);
            Value val;
            val.type = Value::Type::Object;
            std::unordered_map<std::string, std::unique_ptr<Value>> obj;
            obj["filename"] = make_string_value(s.filename);
            obj["initialized"] = make_bool_value(s.initialized);
            val.data = std::move(obj);
            return val;
        });

    // Size
    introspector.register_struct("Size",
        {{"width", "float"}, {"height", "float"}},
        [](const std::any& v) -> Value {
            const auto& s = std::any_cast<const Size&>(v);
            Value val;
            val.type = Value::Type::Object;
            std::unordered_map<std::string, std::unique_ptr<Value>> obj;
            obj["width"] = make_float_value(static_cast<double>(s.width));
            obj["height"] = make_float_value(static_cast<double>(s.height));
            val.data = std::move(obj);
            return val;
        });

    // SpriteSheet
    introspector.register_struct("SpriteSheet",
        {{"atlas_filename", "string"}, {"frame_width", "int"},
         {"frame_height", "int"}, {"columns", "int"},
         {"total_frames", "int"}, {"current_frame", "int"}},
        [](const std::any& v) -> Value {
            const auto& ss = std::any_cast<const SpriteSheet&>(v);
            Value val;
            val.type = Value::Type::Object;
            std::unordered_map<std::string, std::unique_ptr<Value>> obj;
            obj["atlas_filename"] = make_string_value(ss.atlas_filename);
            obj["frame_width"] = make_int_value(static_cast<int64_t>(ss.frame_width));
            obj["frame_height"] = make_int_value(static_cast<int64_t>(ss.frame_height));
            obj["columns"] = make_int_value(static_cast<int64_t>(ss.columns));
            obj["total_frames"] = make_int_value(static_cast<int64_t>(ss.total_frames));
            obj["current_frame"] = make_int_value(static_cast<int64_t>(ss.current_frame));
            val.data = std::move(obj);
            return val;
        });

    // Text (with nested SDL_Color)
    introspector.register_struct("Text",
        {{"content", "string"}, {"font_name", "string"},
         {"font_size", "float"}, {"color", "object"}},
        [](const std::any& v) -> Value {
            const auto& t = std::any_cast<const Text&>(v);
            Value val;
            val.type = Value::Type::Object;
            std::unordered_map<std::string, std::unique_ptr<Value>> obj;
            obj["content"] = make_string_value(t.content);
            obj["font_name"] = make_string_value(t.font_name);
            obj["font_size"] = make_float_value(static_cast<double>(t.font_size));
            // Nested SDL_Color as object {r, g, b, a}
            auto color_obj = std::make_unique<Value>();
            color_obj->type = Value::Type::Object;
            std::unordered_map<std::string, std::unique_ptr<Value>> color_fields;
            color_fields["r"] = make_int_value(static_cast<int64_t>(t.color.r));
            color_fields["g"] = make_int_value(static_cast<int64_t>(t.color.g));
            color_fields["b"] = make_int_value(static_cast<int64_t>(t.color.b));
            color_fields["a"] = make_int_value(static_cast<int64_t>(t.color.a));
            color_obj->data = std::move(color_fields);
            obj["color"] = std::move(color_obj);
            val.data = std::move(obj);
            return val;
        });

    // TowerStats
    introspector.register_struct("TowerStats",
        {{"range", "float"}, {"fire_rate", "float"}, {"damage", "float"},
         {"cooldown_remaining", "float"}, {"target_entity", "int"}},
        [](const std::any& v) -> Value {
            const auto& ts = std::any_cast<const TowerStats&>(v);
            Value val;
            val.type = Value::Type::Object;
            std::unordered_map<std::string, std::unique_ptr<Value>> obj;
            obj["range"] = make_float_value(static_cast<double>(ts.range));
            obj["fire_rate"] = make_float_value(static_cast<double>(ts.fire_rate));
            obj["damage"] = make_float_value(static_cast<double>(ts.damage));
            obj["cooldown_remaining"] = make_float_value(static_cast<double>(ts.cooldown_remaining));
            obj["target_entity"] = make_int_value(static_cast<int64_t>(ts.target_entity));
            val.data = std::move(obj);
            return val;
        });

    // TowerTag (tag — empty object)
    introspector.register_struct("TowerTag", {},
        [](const std::any&) -> Value {
            Value val;
            val.type = Value::Type::Object;
            std::unordered_map<std::string, std::unique_ptr<Value>> obj;
            val.data = std::move(obj);
            return val;
        });

    // Velocity
    introspector.register_struct("Velocity",
        {{"dx", "float"}, {"dy", "float"}},
        [](const std::any& v) -> Value {
            const auto& vel = std::any_cast<const Velocity&>(v);
            Value val;
            val.type = Value::Type::Object;
            std::unordered_map<std::string, std::unique_ptr<Value>> obj;
            obj["dx"] = make_float_value(static_cast<double>(vel.dx));
            obj["dy"] = make_float_value(static_cast<double>(vel.dy));
            val.data = std::move(obj);
            return val;
        });

    // WrapAround (tag — empty object)
    introspector.register_struct("WrapAround", {},
        [](const std::any&) -> Value {
            Value val;
            val.type = Value::Type::Object;
            std::unordered_map<std::string, std::unique_ptr<Value>> obj;
            val.data = std::move(obj);
            return val;
        });
}
