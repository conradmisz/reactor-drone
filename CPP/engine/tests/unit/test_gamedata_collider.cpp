/**
 * Unit tests for GameData loader collider component parsing
 *
 * Verifies that the "collider" key in a JSON entity definition is correctly
 * parsed into a Collider component with width, height, layer, and mask.
 *
 * Testing Framework: Catch2 v3
 * Tags: [gamedata][collider]
 *
 * Validates: Requirements 5.1, 5.2, 5.3, 14.1
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "engine/gamedata_loader.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

// Helper: write JSON string to a temp file and return the path
static std::string write_temp_json(const std::string& json_str) {
    auto path = fs::temp_directory_path() /
        ("test_collider_" + std::to_string(std::hash<std::string>{}(json_str)) + ".json");
    std::ofstream out(path);
    out << json_str;
    out.close();
    return path.string();
}

static void cleanup(const std::string& path) {
    fs::remove(path);
}

TEST_CASE("Collider component parsing with layer and mask", "[gamedata][collider]") {
    EntityManager em;
    ComponentStorage cs;
    Blackboard bb;

    std::string json_str = R"({
        "window": {"width": 800, "height": 600},
        "entities": [
            {
                "id": "duck_test",
                "components": {
                    "collider": {"width": 55.0, "height": 45.0, "layer": 1, "mask": 6}
                }
            }
        ]
    })";

    auto path = write_temp_json(json_str);
    load_game_data(path, em, cs, bb);

    Entity e = bb.get<Entity>("entity.id.duck_test");
    REQUIRE(cs.has_component<Collider>(e));

    auto col = cs.get_component<Collider>(e);
    REQUIRE(col->get().width == Catch::Approx(55.0f));
    REQUIRE(col->get().height == Catch::Approx(45.0f));
    REQUIRE(col->get().layer == 1);
    REQUIRE(col->get().mask == 6);

    cleanup(path);
}

TEST_CASE("Collider defaults layer and mask to 0 when not specified", "[gamedata][collider]") {
    EntityManager em;
    ComponentStorage cs;
    Blackboard bb;

    std::string json_str = R"({
        "window": {"width": 800, "height": 600},
        "entities": [
            {
                "id": "bare_collider",
                "components": {
                    "collider": {"width": 30.0, "height": 20.0}
                }
            }
        ]
    })";

    auto path = write_temp_json(json_str);
    load_game_data(path, em, cs, bb);

    Entity e = bb.get<Entity>("entity.id.bare_collider");
    REQUIRE(cs.has_component<Collider>(e));

    auto col = cs.get_component<Collider>(e);
    REQUIRE(col->get().width == Catch::Approx(30.0f));
    REQUIRE(col->get().height == Catch::Approx(20.0f));
    REQUIRE(col->get().layer == 0);
    REQUIRE(col->get().mask == 0);

    cleanup(path);
}
