/**
 * Property-based tests for GameData_Loader
 *
 * These tests verify universal properties of load_game_data() and
 * serialize_game_data() using Catch2 GENERATE() with bounded iteration counts.
 *
 * Testing Framework: Catch2 v3 with GENERATE()
 * Iterations: NUM_OUTER_TESTS * NUM_INNER_TESTS per property
 *
 * Properties tested:
 *   1. Serialization round-trip — parse → serialize → parse produces equivalent values
 *   2. Unrecognized components are skipped — only known keys produce components
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>
#include "engine/gamedata_loader.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <cmath>

using json = nlohmann::json;
namespace fs = std::filesystem;

constexpr int NUM_OUTER_TESTS = 10;  // Number of different entity configurations to test
constexpr int NUM_INNER_TESTS = 5;   // Number of different component value variations

// Helper: write JSON string to a temp file and return the path
static std::string write_temp_json(const std::string& json_str, int id) {
    auto path = fs::temp_directory_path() / ("prop_gamedata_" + std::to_string(id) + ".json");
    std::ofstream out(path);
    out << json_str;
    out.close();
    return path.string();
}

static void cleanup(const std::string& path) {
    fs::remove(path);
}

// Simple deterministic pseudo-random from seed (xorshift32)
static uint32_t xorshift(uint32_t& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

// Map xorshift output to a float range [lo, hi]
static float rand_float(uint32_t& state, float lo, float hi) {
    uint32_t v = xorshift(state);
    float t = static_cast<float>(v % 10000) / 10000.0f;
    return lo + t * (hi - lo);
}

// Map xorshift output to an int range [lo, hi]
static int rand_int(uint32_t& state, int lo, int hi) {
    uint32_t v = xorshift(state);
    return lo + static_cast<int>(v % static_cast<uint32_t>(hi - lo + 1));
}


// ============================================================================
// Property 1: Serialization round-trip
//
// For any valid GameData.json structure with arbitrary entities and component
// combinations, parsing into ECS state then serializing back then parsing
// again produces entities with equivalent component values.
//
// Feature: 040-03-gamedata-json-import, Property 1: Serialization round-trip
// **Validates: Requirements 2.2, 2.3, 2.4, 2.5, 2.6, 2.7, 2.8, 2.9, 2.10,
//              2.11, 2.12, 2.13, 3.3, 3.4, 3.5, 3.6, 3.7, 4.1, 4.2, 6.1,
//              7.2, 7.3**
// ============================================================================

TEST_CASE("Property 1: Serialization round-trip",
          "[gamedata_loader][property]") {

    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(1, 100000)));

    // Use the seed to deterministically build a test scenario
    uint32_t rng = static_cast<uint32_t>(seed);

    // Random window dimensions
    int win_w = rand_int(rng, 100, 2000);
    int win_h = rand_int(rng, 100, 2000);

    // Random camera values
    float cam_x = rand_float(rng, -1000.0f, 1000.0f);
    float cam_y = rand_float(rng, -1000.0f, 1000.0f);
    float cam_zoom = rand_float(rng, 0.1f, 10.0f);

    // Build JSON programmatically
    json root;
    root["window"] = {{"width", win_w}, {"height", win_h}};
    root["camera"] = {{"lookat_x", cam_x}, {"lookat_y", cam_y}, {"zoom", cam_zoom}};

    json entities_arr = json::array();
    json hud_arr = json::array();

    // Track entity info for verification
    struct EntityInfo {
        std::string id;
        bool is_hud;
        // Component presence flags
        bool has_position = false, has_size = false, has_color = false;
        bool has_velocity = false, has_input = false, has_images = false;
        bool has_text = false, has_screen_position = false;
        // Component values
        float pos_x, pos_y, size_w, size_h;
        uint8_t col_r, col_g, col_b, col_a;
        float vel_dx, vel_dy;
        std::vector<std::string> img_names;
        std::string text_content, text_font;
        float text_font_size;
        uint8_t text_r, text_g, text_b, text_a;
        float sp_x, sp_y;
    };

    std::vector<EntityInfo> infos;

    for (int i = 0; i < NUM_INNER_TESTS; ++i) {
        EntityInfo info;
        info.id = "ent_" + std::to_string(i);

        // Decide if this is a HUD entity (roughly 30% chance)
        info.is_hud = (rand_int(rng, 0, 9) < 3);

        json comps = json::object();

        if (info.is_hud) {
            // HUD entities always have text + screen_position
            info.has_text = true;
            info.has_screen_position = true;

            info.text_content = "text_" + std::to_string(rand_int(rng, 0, 999));
            info.text_font = "font_" + std::to_string(rand_int(rng, 0, 99)) + ".ttf";
            info.text_font_size = static_cast<float>(rand_int(rng, 8, 72));
            info.text_r = static_cast<uint8_t>(rand_int(rng, 0, 255));
            info.text_g = static_cast<uint8_t>(rand_int(rng, 0, 255));
            info.text_b = static_cast<uint8_t>(rand_int(rng, 0, 255));
            info.text_a = static_cast<uint8_t>(rand_int(rng, 0, 255));

            comps["text"] = {
                {"content", info.text_content},
                {"font_name", info.text_font},
                {"font_size", info.text_font_size},
                {"color", {{"r", info.text_r}, {"g", info.text_g},
                           {"b", info.text_b}, {"a", info.text_a}}}
            };

            info.sp_x = rand_float(rng, 0.0f, static_cast<float>(win_w));
            info.sp_y = rand_float(rng, 0.0f, static_cast<float>(win_h));
            comps["screen_position"] = {{"x", info.sp_x}, {"y", info.sp_y}};
        } else {
            // World entity: random subset of components
            if (rand_int(rng, 0, 1) == 1) {
                info.has_position = true;
                info.pos_x = rand_float(rng, -1000.0f, 1000.0f);
                info.pos_y = rand_float(rng, -1000.0f, 1000.0f);
                comps["position"] = {{"x", info.pos_x}, {"y", info.pos_y}};
            }
            if (rand_int(rng, 0, 1) == 1) {
                info.has_size = true;
                info.size_w = rand_float(rng, 1.0f, 500.0f);
                info.size_h = rand_float(rng, 1.0f, 500.0f);
                comps["size"] = {{"width", info.size_w}, {"height", info.size_h}};
            }
            if (rand_int(rng, 0, 1) == 1) {
                info.has_color = true;
                info.col_r = static_cast<uint8_t>(rand_int(rng, 0, 255));
                info.col_g = static_cast<uint8_t>(rand_int(rng, 0, 255));
                info.col_b = static_cast<uint8_t>(rand_int(rng, 0, 255));
                info.col_a = static_cast<uint8_t>(rand_int(rng, 0, 255));
                comps["color"] = {{"r", info.col_r}, {"g", info.col_g},
                                  {"b", info.col_b}, {"a", info.col_a}};
            }
            if (rand_int(rng, 0, 1) == 1) {
                info.has_velocity = true;
                info.vel_dx = rand_float(rng, -500.0f, 500.0f);
                info.vel_dy = rand_float(rng, -500.0f, 500.0f);
                comps["velocity"] = {{"dx", info.vel_dx}, {"dy", info.vel_dy}};
            }
            if (rand_int(rng, 0, 1) == 1) {
                info.has_input = true;
                comps["input"] = json::object();
            }
            if (rand_int(rng, 0, 1) == 1) {
                info.has_images = true;
                int num_imgs = rand_int(rng, 1, 3);
                info.img_names.clear();
                for (int j = 0; j < num_imgs; ++j) {
                    info.img_names.push_back("img_" + std::to_string(j) + ".png");
                }
                comps["images"] = {{"names", info.img_names}, {"active_index", 0}};
            }
        }

        json entry;
        entry["id"] = info.id;
        entry["components"] = comps;

        if (info.is_hud) {
            hud_arr.push_back(entry);
        } else {
            entities_arr.push_back(entry);
        }

        infos.push_back(info);
    }

    root["entities"] = entities_arr;
    root["hud_entities"] = hud_arr;

    std::string json_str = root.dump(2);

    // --- First pass: load ---
    EntityManager em1;
    ComponentStorage cs1;
    Blackboard bb1;

    auto path = write_temp_json(json_str, seed);
    load_game_data(path, em1, cs1, bb1);

    // Collect entity IDs for serialization
    std::vector<std::pair<std::string, Entity>> id_pairs;
    for (const auto& info : infos) {
        Entity e = bb1.get<Entity>("entity.id." + info.id);
        id_pairs.emplace_back(info.id, e);
    }

    // --- Serialize ---
    std::string serialized = serialize_game_data(cs1, bb1, id_pairs);

    // --- Second pass: load serialized output ---
    EntityManager em2;
    ComponentStorage cs2;
    Blackboard bb2;

    auto path2 = write_temp_json(serialized, seed + 1000000);
    load_game_data(path2, em2, cs2, bb2);

    // --- Compare ---
    // Window
    REQUIRE(bb2.get<int>("window_width") == win_w);
    REQUIRE(bb2.get<int>("window_height") == win_h);

    // Camera (float tolerance)
    REQUIRE(std::abs(bb2.get<float>("camera.lookat.x") - cam_x) < 0.01f);
    REQUIRE(std::abs(bb2.get<float>("camera.lookat.y") - cam_y) < 0.01f);
    REQUIRE(std::abs(bb2.get<float>("camera.zoom") - cam_zoom) < 0.01f);

    // Entities
    for (const auto& info : infos) {
        Entity e2 = bb2.get<Entity>("entity.id." + info.id);

        if (info.has_position) {
            REQUIRE(cs2.has_component<Position>(e2));
            auto p = cs2.get_component<Position>(e2);
            REQUIRE(std::abs(p->get().x - info.pos_x) < 0.01f);
            REQUIRE(std::abs(p->get().y - info.pos_y) < 0.01f);
        }
        if (info.has_size) {
            REQUIRE(cs2.has_component<Size>(e2));
            auto s = cs2.get_component<Size>(e2);
            REQUIRE(std::abs(s->get().width - info.size_w) < 0.01f);
            REQUIRE(std::abs(s->get().height - info.size_h) < 0.01f);
        }
        if (info.has_color) {
            REQUIRE(cs2.has_component<Color>(e2));
            auto c = cs2.get_component<Color>(e2);
            REQUIRE(c->get().r == info.col_r);
            REQUIRE(c->get().g == info.col_g);
            REQUIRE(c->get().b == info.col_b);
            REQUIRE(c->get().a == info.col_a);
        }
        if (info.has_velocity) {
            REQUIRE(cs2.has_component<Velocity>(e2));
            auto v = cs2.get_component<Velocity>(e2);
            REQUIRE(std::abs(v->get().dx - info.vel_dx) < 0.01f);
            REQUIRE(std::abs(v->get().dy - info.vel_dy) < 0.01f);
        }
        if (info.has_input) {
            REQUIRE(cs2.has_component<Input>(e2));
        }
        if (info.has_images) {
            REQUIRE(cs2.has_component<Images>(e2));
            auto img = cs2.get_component<Images>(e2);
            REQUIRE(img->get().filenames == info.img_names);
            REQUIRE(img->get().active_index == 0);
        }
        if (info.has_text) {
            REQUIRE(cs2.has_component<Text>(e2));
            auto t = cs2.get_component<Text>(e2);
            REQUIRE(t->get().content == info.text_content);
            REQUIRE(t->get().font_name == info.text_font);
            REQUIRE(std::abs(t->get().font_size - info.text_font_size) < 0.01f);
            REQUIRE(t->get().color.r == info.text_r);
            REQUIRE(t->get().color.g == info.text_g);
            REQUIRE(t->get().color.b == info.text_b);
            REQUIRE(t->get().color.a == info.text_a);
        }
        if (info.has_screen_position) {
            REQUIRE(cs2.has_component<ScreenPosition>(e2));
            auto sp = cs2.get_component<ScreenPosition>(e2);
            REQUIRE(std::abs(sp->get().x - info.sp_x) < 0.01f);
            REQUIRE(std::abs(sp->get().y - info.sp_y) < 0.01f);
        }
    }

    cleanup(path);
    cleanup(path2);
}


// ============================================================================
// Property 2: Unrecognized components are skipped
//
// For any entity with a mix of recognized and unrecognized component keys,
// the loader attaches only the recognized components and silently skips
// the unrecognized ones.
//
// Feature: 040-03-gamedata-json-import, Property 2: Unrecognized components are skipped
// **Validates: Requirements 5.4**
// ============================================================================

TEST_CASE("Property 2: Unrecognized components are skipped",
          "[gamedata_loader][property]") {

    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(1, 1000)));

    uint32_t rng = static_cast<uint32_t>(seed);

    // Build an entity with one recognized component (position) and
    // NUM_INNER_TESTS unrecognized component keys
    float pos_x = rand_float(rng, -1000.0f, 1000.0f);
    float pos_y = rand_float(rng, -1000.0f, 1000.0f);

    json comps = json::object();
    comps["position"] = {{"x", pos_x}, {"y", pos_y}};

    for (int i = 0; i < NUM_INNER_TESTS; ++i) {
        std::string unknown_key = "unknown_" + std::to_string(i) + "_" + std::to_string(seed);
        comps[unknown_key] = {{"data", rand_int(rng, 0, 999)}};
    }

    json root;
    root["window"] = {{"width", 800}, {"height", 600}};
    root["entities"] = json::array({
        {{"id", "test_ent"}, {"components", comps}}
    });

    std::string json_str = root.dump(2);

    EntityManager em;
    ComponentStorage cs;
    Blackboard bb;

    auto path = write_temp_json(json_str, seed + 2000000);
    load_game_data(path, em, cs, bb);

    Entity e = bb.get<Entity>("entity.id.test_ent");

    // Only Position should be attached
    REQUIRE(cs.has_component<Position>(e));
    REQUIRE(std::abs(cs.get_component<Position>(e)->get().x - pos_x) < 0.01f);
    REQUIRE(std::abs(cs.get_component<Position>(e)->get().y - pos_y) < 0.01f);

    // All other component types should NOT be attached
    REQUIRE_FALSE(cs.has_component<Size>(e));
    REQUIRE_FALSE(cs.has_component<Color>(e));
    REQUIRE_FALSE(cs.has_component<Velocity>(e));
    REQUIRE_FALSE(cs.has_component<Input>(e));
    REQUIRE_FALSE(cs.has_component<Images>(e));
    REQUIRE_FALSE(cs.has_component<Text>(e));
    REQUIRE_FALSE(cs.has_component<ScreenPosition>(e));

    cleanup(path);
}
