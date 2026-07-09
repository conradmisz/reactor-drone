/**
 * Unit tests for the Sidecar_Loader (Gen-5, spec 090-15-generator-spec).
 *
 * These tests verify:
 * - The loader populates SpriteSheet + Animation for the committed
 *   enemy_runner.json `march` clip with values taken FROM the sidecar
 *   (re-parsed in the test), never from C++ literals (R8.2, R2.3, R1.x).
 * - The loader throws std::runtime_error for every malformed input:
 *   missing file, malformed JSON, missing required top-level field,
 *   unknown clip name, and an out-of-range clip (R8.3, R3.1–R3.5).
 * - Driving the UNCHANGED engine AnimationSystem over a loaded `march`
 *   clip advances current_frame within [start_frame, start_frame +
 *   frame_count) and writes it into SpriteSheet.current_frame (R4.5, R6.5).
 *
 * Requirements tested: 8.1, 8.2, 8.3, 2.3, 4.5, 6.5, 3.1, 3.2, 3.3, 3.4, 3.5
 */

#include <catch2/catch_test_macros.hpp>

#include "engine/sidecar_loader.hpp"
#include "engine/project_paths.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/systems/animation_system.hpp"

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

using json = nlohmann::json;

namespace {

// Absolute path to the committed enemy sidecar (resolved via CLASS_ROOT_DIR,
// so tests find it in the source tree regardless of launch directory).
std::string committed_sidecar_path() {
    return project_paths::assets_dir() + "/images/enemy_runner.json";
}

// Parse the committed sidecar JSON directly so expected values come FROM the
// sidecar rather than from hard-coded literals (R8.2, R2.3).
json read_committed_sidecar() {
    std::ifstream file(committed_sidecar_path());
    REQUIRE(file.is_open());
    json data;
    file >> data;
    return data;
}

// Write `contents` to a uniquely named temp file and return its path. The
// caller is responsible for removing it (see TempFile RAII helper below).
std::string write_temp_file(const std::string& tag, const std::string& contents) {
    static int counter = 0;
    std::filesystem::path p =
        std::filesystem::temp_directory_path() /
        ("sidecar_loader_test_" + tag + "_" + std::to_string(counter++) + ".json");
    std::ofstream out(p);
    REQUIRE(out.is_open());
    out << contents;
    out.close();
    return p.string();
}

// RAII guard that removes a temp file on scope exit (cleanup, R8.3).
struct TempFile {
    std::string path;
    explicit TempFile(std::string p) : path(std::move(p)) {}
    ~TempFile() {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }
};

}  // namespace

// -----------------------------------------------------------------------
// 1. Loader populates SpriteSheet + Animation for the committed `march`
//    clip with values equal to the sidecar (no literals).
// Validates: Requirements 8.2, 2.3, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7
// -----------------------------------------------------------------------
TEST_CASE("Sidecar_Loader: populates SpriteSheet+Animation from committed march clip",
          "[Engine][sidecar_loader][unit]") {
    json sidecar = read_committed_sidecar();
    const json& march = sidecar["animations"]["march"];

    sidecar_loader::LoadedSprite loaded =
        sidecar_loader::load(committed_sidecar_path(), "march");

    // SpriteSheet values come straight from the sidecar (R1.2, R1.3, R2.3).
    const SpriteSheet& ss = loaded.sprite_sheet;
    CHECK(ss.atlas_filename == sidecar["atlas"].get<std::string>());
    CHECK(ss.frame_width == sidecar["frame_width"].get<int>());
    CHECK(ss.frame_height == sidecar["frame_height"].get<int>());
    CHECK(ss.columns == sidecar["columns"].get<int>());
    CHECK(ss.total_frames == sidecar["total_frames"].get<int>());

    // Animation clip values come from the `march` clip (R1.4, R2.2).
    const Animation& anim = loaded.animation;
    CHECK(anim.start_frame == march["start_frame"].get<int>());
    CHECK(anim.frame_count == march["frame_count"].get<int>());
    CHECK(std::fabs(anim.frame_duration - march["frame_duration"].get<float>()) < 1e-6f);
    CHECK(anim.looping == march["looping"].get<bool>());

    // current_frame == start_frame on both components (R1.5).
    CHECK(anim.current_frame == march["start_frame"].get<int>());
    CHECK(ss.current_frame == march["start_frame"].get<int>());

    // Transient fields initialized (R1.6).
    CHECK(std::fabs(anim.elapsed - 0.0f) < 1e-6f);
    CHECK(anim.playing == true);
    CHECK(anim.finished == false);
}

// -----------------------------------------------------------------------
// 2. Missing file → throws std::runtime_error (R3.1).
// Validates: Requirements 8.3, 3.1
// -----------------------------------------------------------------------
TEST_CASE("Sidecar_Loader: missing file throws", "[Engine][sidecar_loader][unit]") {
    std::string missing =
        project_paths::assets_dir() + "/images/this_file_does_not_exist.json";
    REQUIRE_THROWS_AS(sidecar_loader::load(missing, "march"), std::runtime_error);
}

// -----------------------------------------------------------------------
// 3. Malformed JSON → throws std::runtime_error (R3.2).
// Validates: Requirements 8.3, 3.2
// -----------------------------------------------------------------------
TEST_CASE("Sidecar_Loader: malformed JSON throws", "[Engine][sidecar_loader][unit]") {
    TempFile tmp(write_temp_file("malformed", "{ this is not valid json :: ]"));
    REQUIRE_THROWS_AS(sidecar_loader::load(tmp.path, "march"), std::runtime_error);
}

// -----------------------------------------------------------------------
// 4. Missing required top-level field → throws std::runtime_error (R3.3).
//    Here `total_frames` is omitted.
// Validates: Requirements 8.3, 3.3
// -----------------------------------------------------------------------
TEST_CASE("Sidecar_Loader: missing required field throws",
          "[Engine][sidecar_loader][unit]") {
    const std::string contents = R"({
        "atlas": "enemy_runner.png",
        "frame_width": 128,
        "frame_height": 128,
        "columns": 4,
        "animations": {
            "march": { "start_frame": 0, "frame_count": 8, "frame_duration": 0.1, "looping": true }
        }
    })";
    TempFile tmp(write_temp_file("missing_field", contents));
    REQUIRE_THROWS_AS(sidecar_loader::load(tmp.path, "march"), std::runtime_error);
}

// -----------------------------------------------------------------------
// 5. Unknown clip name → throws std::runtime_error (R3.4).
// Validates: Requirements 8.3, 3.4
// -----------------------------------------------------------------------
TEST_CASE("Sidecar_Loader: unknown clip name throws", "[Engine][sidecar_loader][unit]") {
    REQUIRE_THROWS_AS(
        sidecar_loader::load(committed_sidecar_path(), "no_such_clip"),
        std::runtime_error);
}

// -----------------------------------------------------------------------
// 6. Out-of-range clip (start_frame + frame_count > total_frames) → throws
//    std::runtime_error (R3.5).
// Validates: Requirements 8.3, 3.5
// -----------------------------------------------------------------------
TEST_CASE("Sidecar_Loader: out-of-range clip throws", "[Engine][sidecar_loader][unit]") {
    // total_frames = 10, clip wants frames [5, 5+8) = [5, 13) → exceeds total.
    const std::string contents = R"({
        "atlas": "enemy_runner.png",
        "frame_width": 128,
        "frame_height": 128,
        "columns": 4,
        "total_frames": 10,
        "animations": {
            "march": { "start_frame": 5, "frame_count": 8, "frame_duration": 0.1, "looping": true }
        }
    })";
    TempFile tmp(write_temp_file("out_of_range", contents));
    REQUIRE_THROWS_AS(sidecar_loader::load(tmp.path, "march"), std::runtime_error);
}

// -----------------------------------------------------------------------
// 7. Driving the UNCHANGED engine AnimationSystem over a loaded `march`
//    clip advances current_frame within [start_frame, start_frame +
//    frame_count) and writes it into SpriteSheet.current_frame.
// Validates: Requirements 4.5, 6.5
// -----------------------------------------------------------------------
TEST_CASE("Sidecar_Loader: AnimationSystem advances a loaded march clip in range",
          "[Engine][sidecar_loader][unit]") {
    sidecar_loader::LoadedSprite loaded =
        sidecar_loader::load(committed_sidecar_path(), "march");

    const int start = loaded.animation.start_frame;
    const int count = loaded.animation.frame_count;
    const float frame_duration = loaded.animation.frame_duration;
    REQUIRE(count > 1);
    REQUIRE(frame_duration > 0.0f);

    EntityManager entity_manager;
    ComponentStorage storage;
    Blackboard blackboard;
    AnimationSystem animation_system;

    Entity e = entity_manager.create_entity();
    storage.add_component<SpriteSheet>(e, loaded.sprite_sheet);
    storage.add_component<Animation>(e, loaded.animation);

    // Each tick accumulates exactly one frame_duration worth of time.
    blackboard.set<double>("delta_time", static_cast<double>(frame_duration));

    // Drive the system for more ticks than the clip length to exercise the
    // looping wrap; current_frame must always stay within the clip range and
    // stay synchronized to the SpriteSheet.
    for (int tick = 0; tick < count * 3; ++tick) {
        animation_system.update(storage, blackboard);

        auto anim = storage.get_component<Animation>(e);
        auto ss = storage.get_component<SpriteSheet>(e);
        REQUIRE(anim.has_value());
        REQUIRE(ss.has_value());

        int cur = anim->get().current_frame;
        CHECK(cur >= start);
        CHECK(cur < start + count);
        CHECK(ss->get().current_frame == cur);  // SpriteSheet sync (R6.5)
    }
}
