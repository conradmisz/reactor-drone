#include "engine/gamedata_loader.hpp"
#include "engine/tile_map.hpp"
#include "engine/sidecar_loader.hpp"
#include "engine/project_paths.hpp"
#include "game/wave_config.hpp"
#include "game/tower_type_config.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>
#include <memory>
#include <vector>

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Helper: parse a single entity's "components" object and attach to ECS
// ---------------------------------------------------------------------------
static void parse_components(const json& components_obj,
                             Entity entity,
                             ComponentStorage& component_storage) {
    for (auto& [key, value] : components_obj.items()) {
        if (key == "position") {
            component_storage.add_component<Position>(entity, Position{
                value["x"].get<float>(),
                value["y"].get<float>()
            });
        } else if (key == "size") {
            component_storage.add_component<Size>(entity, Size{
                value["width"].get<float>(),
                value["height"].get<float>()
            });
        } else if (key == "color") {
            component_storage.add_component<Color>(entity, Color{
                value["r"].get<uint8_t>(),
                value["g"].get<uint8_t>(),
                value["b"].get<uint8_t>(),
                value["a"].get<uint8_t>()
            });
        } else if (key == "velocity") {
            component_storage.add_component<Velocity>(entity, Velocity{
                value["dx"].get<float>(),
                value["dy"].get<float>()
            });
        } else if (key == "input") {
            component_storage.add_component<Input>(entity, Input{});
        } else if (key == "images") {
            component_storage.add_component<Images>(entity, Images{
                value["names"].get<std::vector<std::string>>(),
                value["active_index"].get<size_t>()
            });
        } else if (key == "text") {
            auto& color_obj = value["color"];
            SDL_Color sdl_color{
                color_obj["r"].get<uint8_t>(),
                color_obj["g"].get<uint8_t>(),
                color_obj["b"].get<uint8_t>(),
                color_obj["a"].get<uint8_t>()
            };
            component_storage.add_component<Text>(entity, Text{
                value["content"].get<std::string>(),
                value["font_name"].get<std::string>(),
                value["font_size"].get<float>(),
                sdl_color
            });
        } else if (key == "screen_position") {
            component_storage.add_component<ScreenPosition>(entity, ScreenPosition{
                value["x"].get<float>(),
                value["y"].get<float>()
            });
        } else if (key == "script") {
            component_storage.add_component<Script>(entity, Script{
                value["filename"].get<std::string>(),
                false
            });
        } else if (key == "collider") {
            component_storage.add_component<Collider>(entity, Collider{
                value["width"].get<float>(),
                value["height"].get<float>(),
                value.value("layer", static_cast<uint8_t>(0)),
                value.value("mask", static_cast<uint8_t>(0))
            });
        } else if (key == "sprite_sheet") {
            // Declarative sprite-sheet entity: references a committed
            // Sidecar_JSON (relative to the assets directory) and a clip name.
            // Frame size, grid, total frames, and clip range come from the
            // referenced sidecar via the Sidecar_Loader — never from GameData
            // literals. The loader either returns a fully populated
            // LoadedSprite or throws std::runtime_error; the two add_component
            // calls run only after a successful load, so a rejection propagates
            // and attaches neither component.
            std::string sidecar_rel = value["sidecar"].get<std::string>();
            std::string clip        = value["clip"].get<std::string>();
            std::string sidecar_abs = project_paths::assets_dir() + "/" + sidecar_rel;

            auto loaded = sidecar_loader::load(sidecar_abs, clip);
            component_storage.add_component<SpriteSheet>(entity, loaded.sprite_sheet);
            component_storage.add_component<Animation>(entity, loaded.animation);
        }
        // Unrecognized keys are silently skipped
    }
}

// ---------------------------------------------------------------------------
// Helper: process an entity array ("entities" or "hud_entities")
// ---------------------------------------------------------------------------
static void process_entity_array(const json& arr,
                                 const std::string& array_name,
                                 EntityManager& entity_manager,
                                 ComponentStorage& component_storage,
                                 Blackboard& blackboard) {
    for (size_t i = 0; i < arr.size(); ++i) {
        const auto& element = arr[i];

        if (!element.contains("id")) {
            throw std::runtime_error(
                "Entity at index " + std::to_string(i) +
                " in '" + array_name + "' is missing 'id' field");
        }

        std::string id = element["id"].get<std::string>();
        Entity entity = entity_manager.create_entity();
        blackboard.set<Entity>("entity.id." + id, entity);

        if (element.contains("components")) {
            parse_components(element["components"], entity, component_storage);
        }
    }
}

// ---------------------------------------------------------------------------
// load_game_data
// ---------------------------------------------------------------------------
void load_game_data(const std::string& file_path,
                    EntityManager& entity_manager,
                    ComponentStorage& component_storage,
                    Blackboard& blackboard) {
    // 1. Read file
    std::ifstream file(file_path);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + file_path);
    }

    // 2. Parse JSON
    json data;
    try {
        data = json::parse(file);
    } catch (const json::parse_error& e) {
        throw std::runtime_error(std::string("JSON parse error: ") + e.what());
    }

    // 3. Window configuration
    if (data.contains("window")) {
        const auto& win = data["window"];
        blackboard.set<int>("window_width", win["width"].get<int>());
        blackboard.set<int>("window_height", win["height"].get<int>());
    }

    // 4. Camera configuration (defaults if absent)
    if (data.contains("camera")) {
        const auto& cam = data["camera"];
        blackboard.set<float>("camera.lookat.x", cam.value("lookat_x", 0.0f));
        blackboard.set<float>("camera.lookat.y", cam.value("lookat_y", 0.0f));
        blackboard.set<float>("camera.zoom", cam.value("zoom", 1.0f));
    } else {
        blackboard.set<float>("camera.lookat.x", 0.0f);
        blackboard.set<float>("camera.lookat.y", 0.0f);
        blackboard.set<float>("camera.zoom", 1.0f);
    }

    // 4.5 World bounds (optional — used for debug border rendering)
    if (data.contains("world")) {
        const auto& world = data["world"];
        blackboard.set<float>("world.x", world.value("x", 0.0f));
        blackboard.set<float>("world.y", world.value("y", 0.0f));
        blackboard.set<float>("world.width", world.value("width", 0.0f));
        blackboard.set<float>("world.height", world.value("height", 0.0f));
    }

    // 4.6 Debug configuration (optional)
    if (data.contains("debug")) {
        const auto& debug = data["debug"];
        blackboard.set<int>("debug.overlay_alpha", debug.value("overlay_alpha", 128));
    }

    // 4.7 Grid configuration (optional)
    if (data.contains("grid")) {
        const auto& grid = data["grid"];
        blackboard.set<int>("grid.rows", grid["rows"].get<int>());
        blackboard.set<int>("grid.cols", grid["cols"].get<int>());
        blackboard.set<int>("grid.cell_size", grid["cell_size"].get<int>());
        blackboard.set<float>("grid.offset_x", grid["offset_x"].get<float>());
        blackboard.set<float>("grid.offset_y", grid["offset_y"].get<float>());
        if (grid.contains("gem_colors")) {
            blackboard.set<std::vector<std::string>>(
                "grid.gem_colors",
                grid["gem_colors"].get<std::vector<std::string>>());
        }
    }

    // 4.8 Atlas configuration (optional)
    if (data.contains("atlas")) {
        const auto& atlas = data["atlas"];
        blackboard.set<std::string>("atlas.filename", atlas["filename"].get<std::string>());
        blackboard.set<int>("atlas.frame_width", atlas["frame_width"].get<int>());
        blackboard.set<int>("atlas.frame_height", atlas["frame_height"].get<int>());
        blackboard.set<int>("atlas.columns", atlas["columns"].get<int>());
    }

    // 4.9 Scoring configuration (optional)
    if (data.contains("scoring")) {
        const auto& scoring = data["scoring"];
        blackboard.set<int>("scoring.3_match", scoring["3_match"].get<int>());
        blackboard.set<int>("scoring.4_match", scoring["4_match"].get<int>());
        blackboard.set<int>("scoring.5_match", scoring["5_match"].get<int>());
        blackboard.set<int>("scoring.6_match", scoring["6_match"].get<int>());
        blackboard.set<int>("scoring.7_match", scoring["7_match"].get<int>());
    }

    // 4.10 Physics configuration (optional)
    if (data.contains("physics")) {
        const auto& physics = data["physics"];
        blackboard.set<float>("physics.fall_speed", physics["fall_speed"].get<float>());
    }

    // 4.11 Levels configuration (optional)
    if (data.contains("levels")) {
        const auto& levels = data["levels"];
        for (auto& [level_num, level_obj] : levels.items()) {
            std::string prefix = "levels." + level_num;
            blackboard.set<std::string>(prefix + ".name", level_obj["name"].get<std::string>());
            blackboard.set<int>(prefix + ".moves", level_obj["moves"].get<int>());

            if (level_obj.contains("objectives")) {
                const auto& objectives = level_obj["objectives"];
                blackboard.set<int>(prefix + ".objective_count", static_cast<int>(objectives.size()));
                for (size_t i = 0; i < objectives.size(); ++i) {
                    std::string obj_prefix = prefix + ".objective." + std::to_string(i);
                    blackboard.set<std::string>(obj_prefix + ".type",
                        objectives[i]["type"].get<std::string>());
                    blackboard.set<int>(obj_prefix + ".target",
                        objectives[i]["target"].get<int>());
                }
            } else {
                blackboard.set<int>(prefix + ".objective_count", 0);
            }
        }
    }

    // 4.12 Animation definitions (optional)
    if (data.contains("animation_definitions")) {
        const auto& anim_defs = data["animation_definitions"];
        for (auto& [gem_id, states_obj] : anim_defs.items()) {
            for (auto& [state_name, params] : states_obj.items()) {
                std::string prefix = "anim_def." + gem_id + "." + state_name;
                blackboard.set<int>(prefix + ".start_frame", params["start_frame"].get<int>());
                blackboard.set<int>(prefix + ".frame_count", params["frame_count"].get<int>());
                blackboard.set<float>(prefix + ".frame_duration", params["frame_duration"].get<float>());
                blackboard.set<bool>(prefix + ".looping", params["looping"].get<bool>());
            }
        }
    }

    // 4.13 Tilemap parsing (optional)
    if (data.contains("tilemap")) {
        const auto& tilemap_obj = data["tilemap"];

        int tile_size = tilemap_obj.value("tile_size", 64);

        // Parse spawn and destination
        GridCoord spawn{0, 0};
        GridCoord destination{0, 0};
        if (tilemap_obj.contains("spawn")) {
            spawn.col = tilemap_obj["spawn"]["col"].get<int>();
            spawn.row = tilemap_obj["spawn"]["row"].get<int>();
        }
        if (tilemap_obj.contains("destination")) {
            destination.col = tilemap_obj["destination"]["col"].get<int>();
            destination.row = tilemap_obj["destination"]["row"].get<int>();
        }

        // Parse tiles array (top-to-bottom in JSON, flip to bottom-left origin)
        const auto& tiles_arr = tilemap_obj["tiles"];
        int num_json_rows = static_cast<int>(tiles_arr.size());
        int num_cols = 0;

        if (num_json_rows > 0) {
            num_cols = static_cast<int>(tiles_arr[0].size());
        }

        // Validate consistent row lengths and tile codes
        for (int json_row = 0; json_row < num_json_rows; ++json_row) {
            int row_len = static_cast<int>(tiles_arr[json_row].size());
            if (row_len != num_cols) {
                throw std::runtime_error(
                    "Tilemap row " + std::to_string(json_row) +
                    " has " + std::to_string(row_len) +
                    " columns, expected " + std::to_string(num_cols));
            }
            for (int col = 0; col < row_len; ++col) {
                int code = tiles_arr[json_row][col].get<int>();
                if (code < 0 || code > 2) {
                    throw std::runtime_error(
                        "Invalid tile code " + std::to_string(code) +
                        " at row " + std::to_string(json_row) +
                        ", col " + std::to_string(col));
                }
            }
        }

        int num_rows = num_json_rows;

        // Validate spawn bounds
        if (spawn.col < 0 || spawn.col >= num_cols || spawn.row < 0 || spawn.row >= num_rows) {
            throw std::runtime_error(
                "Spawn tile (" + std::to_string(spawn.col) + ", " + std::to_string(spawn.row) +
                ") is outside grid bounds (" + std::to_string(num_cols) + "x" + std::to_string(num_rows) + ")");
        }

        // Validate destination bounds
        if (destination.col < 0 || destination.col >= num_cols || destination.row < 0 || destination.row >= num_rows) {
            throw std::runtime_error(
                "Destination tile (" + std::to_string(destination.col) + ", " + std::to_string(destination.row) +
                ") is outside grid bounds (" + std::to_string(num_cols) + "x" + std::to_string(num_rows) + ")");
        }

        // Build column-major tiles array with row flipping
        // Row mapping: grid_row = (num_json_rows - 1) - json_row_index
        std::vector<std::vector<TileType>> tiles(num_cols, std::vector<TileType>(num_rows, TileType::GRASS));
        for (int json_row = 0; json_row < num_json_rows; ++json_row) {
            int grid_row = (num_json_rows - 1) - json_row;
            for (int col = 0; col < num_cols; ++col) {
                int code = tiles_arr[json_row][col].get<int>();
                tiles[col][grid_row] = static_cast<TileType>(code);
            }
        }

        // Validate spawn is PATH
        if (tiles[spawn.col][spawn.row] != TileType::PATH) {
            throw std::runtime_error(
                "Spawn tile (" + std::to_string(spawn.col) + ", " + std::to_string(spawn.row) +
                ") must be a PATH tile");
        }

        // Validate destination is PATH
        if (tiles[destination.col][destination.row] != TileType::PATH) {
            throw std::runtime_error(
                "Destination tile (" + std::to_string(destination.col) + ", " + std::to_string(destination.row) +
                ") must be a PATH tile");
        }

        // Construct TileMap
        auto tilemap = std::make_shared<TileMap>();
        tilemap->tiles = std::move(tiles);
        tilemap->rows = num_rows;
        tilemap->cols = num_cols;
        tilemap->tile_size = tile_size;
        tilemap->spawn = spawn;
        tilemap->destination = destination;

        // Store on Blackboard
        blackboard.set<std::shared_ptr<TileMap>>("tilemap", tilemap);

        // Set world bounds
        float world_width = static_cast<float>(num_cols * tile_size);
        float world_height = static_cast<float>(num_rows * tile_size);
        blackboard.set<float>("world.x", 0.0f);
        blackboard.set<float>("world.y", 0.0f);
        blackboard.set<float>("world.width", world_width);
        blackboard.set<float>("world.height", world_height);

        // Center camera on tile world
        blackboard.set<float>("camera.lookat.x", world_width / 2.0f);
        blackboard.set<float>("camera.lookat.y", world_height / 2.0f);
    }

    // 4.14 Game configuration (optional — wave spawning and player lives)
    {
        int lives = 10;
        auto wave_configs = std::make_shared<std::vector<WaveConfig>>();

        if (data.contains("game")) {
            const auto& game_obj = data["game"];
            lives = game_obj.value("lives", 10);

            // Parse starting_gold → Blackboard "player_gold"
            int starting_gold = game_obj.value("starting_gold", 100);
            blackboard.set<int>("player_gold", starting_gold);

            // Parse tower configuration
            if (game_obj.contains("tower")) {
                const auto& tower_obj = game_obj["tower"];
                int tower_cost = tower_obj.value("cost", 50);
                float tower_range = tower_obj.value("range", 150.0f);
                float tower_fire_rate = tower_obj.value("fire_rate", 1.0f);
                float tower_damage = tower_obj.value("damage", 25.0f);
                int kill_reward = tower_obj.value("kill_reward", 10);

                // Validate tower parameters
                if (tower_cost <= 0) {
                    throw std::runtime_error("Tower config: cost must be > 0");
                }
                if (tower_range <= 0.0f) {
                    throw std::runtime_error("Tower config: range must be > 0");
                }
                if (tower_fire_rate <= 0.0f) {
                    throw std::runtime_error("Tower config: fire_rate must be > 0");
                }
                if (tower_damage <= 0.0f) {
                    throw std::runtime_error("Tower config: damage must be > 0");
                }

                blackboard.set<int>("tower_cost", tower_cost);
                blackboard.set<float>("tower_range", tower_range);
                blackboard.set<float>("tower_fire_rate", tower_fire_rate);
                blackboard.set<float>("tower_damage", tower_damage);
                blackboard.set<int>("kill_reward", kill_reward);
            } else {
                // Defaults if "tower" object absent
                blackboard.set<int>("tower_cost", 50);
                blackboard.set<float>("tower_range", 150.0f);
                blackboard.set<float>("tower_fire_rate", 1.0f);
                blackboard.set<float>("tower_damage", 25.0f);
                blackboard.set<int>("kill_reward", 10);
            }

            // Parse tower_types array (per-type tower configurations)
            if (game_obj.contains("tower_types")) {
                const auto& tower_types_arr = game_obj["tower_types"];
                auto tower_types = std::make_shared<std::vector<TowerTypeConfig>>();

                for (size_t i = 0; i < tower_types_arr.size(); ++i) {
                    const auto& tt = tower_types_arr[i];
                    TowerTypeConfig config;
                    config.type = tt.value("type", std::string("cannon"));
                    config.cost = tt.value("cost", 50);
                    config.range = tt.value("range", 150.0f);
                    config.fire_rate = tt.value("fire_rate", 1.0f);
                    config.damage = tt.value("damage", 25.0f);
                    config.laser_duration = tt.value("laser_duration", 0.0f);
                    config.sidecar = tt.value("sidecar", std::string(""));
                    config.clip = tt.value("clip", std::string("idle"));

                    // Validate tower type parameters
                    if (config.type.empty()) {
                        throw std::runtime_error(
                            "tower_types[" + std::to_string(i) + "]: type must not be empty");
                    }
                    if (config.cost <= 0) {
                        throw std::runtime_error(
                            "tower_types[" + std::to_string(i) + "]: cost must be > 0");
                    }
                    if (config.range <= 0.0f) {
                        throw std::runtime_error(
                            "tower_types[" + std::to_string(i) + "]: range must be > 0");
                    }
                    if (config.fire_rate <= 0.0f) {
                        throw std::runtime_error(
                            "tower_types[" + std::to_string(i) + "]: fire_rate must be > 0");
                    }
                    if (config.damage <= 0.0f) {
                        throw std::runtime_error(
                            "tower_types[" + std::to_string(i) + "]: damage must be > 0");
                    }

                    tower_types->push_back(config);
                }

                blackboard.set<std::shared_ptr<std::vector<TowerTypeConfig>>>("tower_types", tower_types);
            }

            if (game_obj.contains("waves")) {
                const auto& waves_arr = game_obj["waves"];
                for (size_t i = 0; i < waves_arr.size(); ++i) {
                    const auto& w = waves_arr[i];
                    WaveConfig wc;
                    wc.enemy_count = w.value("enemy_count", 5);
                    wc.spawn_interval = w.value("spawn_interval", 1.0f);
                    wc.wave_delay = w.value("wave_delay", 2.0f);
                    wc.enemy_speed = w.value("enemy_speed", 64.0f);
                    wc.enemy_hp = w.value("enemy_hp", 100.0f);
                    wc.enemy_sidecar = w.value("enemy_sidecar", std::string("images/enemy_runner.json"));
                    wc.enemy_clip = w.value("enemy_clip", std::string("march"));

                    // Validate wave parameters
                    if (wc.enemy_count <= 0) {
                        throw std::runtime_error(
                            "Wave " + std::to_string(i) + ": enemy_count must be > 0");
                    }
                    if (wc.spawn_interval <= 0.0f) {
                        throw std::runtime_error(
                            "Wave " + std::to_string(i) + ": spawn_interval must be > 0");
                    }
                    if (wc.enemy_speed <= 0.0f) {
                        throw std::runtime_error(
                            "Wave " + std::to_string(i) + ": enemy_speed must be > 0");
                    }
                    if (wc.enemy_hp <= 0.0f) {
                        throw std::runtime_error(
                            "Wave " + std::to_string(i) + ": enemy_hp must be > 0");
                    }

                    // Treat negative wave_delay as 0 (immediate start)
                    if (wc.wave_delay < 0.0f) {
                        wc.wave_delay = 0.0f;
                    }

                    wave_configs->push_back(wc);
                }
            }
        } else {
            // "game" object absent — set tower defaults
            blackboard.set<int>("player_gold", 100);
            blackboard.set<int>("tower_cost", 50);
            blackboard.set<float>("tower_range", 150.0f);
            blackboard.set<float>("tower_fire_rate", 1.0f);
            blackboard.set<float>("tower_damage", 25.0f);
            blackboard.set<int>("kill_reward", 10);
        }

        // If no waves were parsed, create a default single wave
        if (wave_configs->empty()) {
            WaveConfig default_wave;
            default_wave.enemy_count = 5;
            default_wave.spawn_interval = 1.0f;
            default_wave.wave_delay = 2.0f;
            default_wave.enemy_speed = 64.0f;
            default_wave.enemy_hp = 100.0f;
            wave_configs->push_back(default_wave);
        }

        blackboard.set<int>("player_lives", lives);
        blackboard.set<std::shared_ptr<std::vector<WaveConfig>>>("wave_configs", wave_configs);
    }

    // 5. Entities
    if (data.contains("entities")) {
        process_entity_array(data["entities"], "entities",
                             entity_manager, component_storage, blackboard);
    }

    // 6. HUD entities
    if (data.contains("hud_entities")) {
        process_entity_array(data["hud_entities"], "hud_entities",
                             entity_manager, component_storage, blackboard);
    }

    // 7-11: Game-specific entity loading removed (Asteroids code stripped for Class-070)
    // Game-specific loaders (ship, bullet, asteroids, spawn, lives) are added
    // by each game's main.cpp or a game-specific loader, not the engine.
}

// ---------------------------------------------------------------------------
// serialize_game_data
// ---------------------------------------------------------------------------
std::string serialize_game_data(
    const ComponentStorage& component_storage,
    const Blackboard& blackboard,
    const std::vector<std::pair<std::string, Entity>>& entity_ids) {

    json root;

    // Window
    root["window"] = {
        {"width",  blackboard.get<int>("window_width")},
        {"height", blackboard.get<int>("window_height")}
    };

    // Camera
    root["camera"] = {
        {"lookat_x", blackboard.get<float>("camera.lookat.x")},
        {"lookat_y", blackboard.get<float>("camera.lookat.y")},
        {"zoom",     blackboard.get<float>("camera.zoom")}
    };

    json entities_arr = json::array();
    json hud_arr      = json::array();

    for (const auto& [id, entity] : entity_ids) {
        json entry;
        entry["id"] = id;
        json comps = json::object();

        // Determine if this is a HUD entity (has both Text and ScreenPosition)
        bool is_hud = component_storage.has_component<Text>(entity) &&
                      component_storage.has_component<ScreenPosition>(entity);

        // Position
        if (component_storage.has_component<Position>(entity)) {
            auto pos = component_storage.get_component<Position>(entity);
            comps["position"] = {{"x", pos->get().x}, {"y", pos->get().y}};
        }

        // Size
        if (component_storage.has_component<Size>(entity)) {
            auto sz = component_storage.get_component<Size>(entity);
            comps["size"] = {{"width", sz->get().width}, {"height", sz->get().height}};
        }

        // Color
        if (component_storage.has_component<Color>(entity)) {
            auto c = component_storage.get_component<Color>(entity);
            comps["color"] = {
                {"r", c->get().r}, {"g", c->get().g},
                {"b", c->get().b}, {"a", c->get().a}
            };
        }

        // Velocity
        if (component_storage.has_component<Velocity>(entity)) {
            auto v = component_storage.get_component<Velocity>(entity);
            comps["velocity"] = {{"dx", v->get().dx}, {"dy", v->get().dy}};
        }

        // Input
        if (component_storage.has_component<Input>(entity)) {
            comps["input"] = json::object();
        }

        // Images
        if (component_storage.has_component<Images>(entity)) {
            auto img = component_storage.get_component<Images>(entity);
            comps["images"] = {
                {"names",        img->get().filenames},
                {"active_index", img->get().active_index}
            };
        }

        // Text
        if (component_storage.has_component<Text>(entity)) {
            auto t = component_storage.get_component<Text>(entity);
            comps["text"] = {
                {"content",   t->get().content},
                {"font_name", t->get().font_name},
                {"font_size", t->get().font_size},
                {"color", {
                    {"r", t->get().color.r}, {"g", t->get().color.g},
                    {"b", t->get().color.b}, {"a", t->get().color.a}
                }}
            };
        }

        // ScreenPosition
        if (component_storage.has_component<ScreenPosition>(entity)) {
            auto sp = component_storage.get_component<ScreenPosition>(entity);
            comps["screen_position"] = {{"x", sp->get().x}, {"y", sp->get().y}};
        }

        entry["components"] = comps;

        if (is_hud) {
            hud_arr.push_back(entry);
        } else {
            entities_arr.push_back(entry);
        }
    }

    root["entities"]     = entities_arr;
    root["hud_entities"] = hud_arr;

    return root.dump(2);
}
