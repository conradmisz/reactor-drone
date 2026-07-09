/**
 * Property-based tests for the Sidecar_Loader (Gen-5).
 *
 * Feature: 090-15-generator-spec
 *
 * These tests verify universal properties of sidecar_loader::load() using
 * Catch2 GENERATE() with bounded iteration counts. Each test writes a
 * generated Sidecar_JSON to a temporary file, loads it, asserts the property,
 * and removes the temporary file.
 *
 * Testing Framework: Catch2 v3 with GENERATE()
 * Iterations: bounded by NUM_OUTER_TESTS / NUM_INNER_TESTS (property-test-bounds)
 *
 * Properties tested (see design.md "Correctness Properties"):
 *   P1. SpriteSheet field round-trip (no magic numbers)
 *   P2. Animation clip fidelity and initialization        (Task 1.7)
 *   P3. Frame-slicing conformance over the clip range      (Task 1.8)
 *   P4. Invalid sidecars are rejected without producing components (Task 1.9)
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>

#include "engine/sidecar_loader.hpp"
#include "engine/ecs/components.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <cmath>
#include <cstdint>
#include <string>

using json = nlohmann::json;
namespace fs = std::filesystem;

// ----------------------------------------------------------------------------
// Bounded-iteration constants (property-test-bounds steering rule).
// Used in EVERY GENERATE(take(...)) call in this file — no hard-coded counts.
// ----------------------------------------------------------------------------
constexpr int NUM_OUTER_TESTS = 10;  // Number of distinct generated sidecars per section
constexpr int NUM_INNER_TESTS = 5;   // Number of value variations per sidecar

// ----------------------------------------------------------------------------
// Shared helpers (used by P1 here and by P2-P4 appended in later tasks).
// ----------------------------------------------------------------------------

// Write a JSON document to a uniquely-named temp file and return its path.
static std::string write_temp_sidecar(const json& doc, const std::string& tag, int id) {
    auto path = fs::temp_directory_path()
              / ("prop_sidecar_" + tag + "_" + std::to_string(id) + ".json");
    std::ofstream out(path);
    out << doc.dump(2);
    out.close();
    return path.string();
}

// Remove a temp file created by write_temp_sidecar.
static void cleanup_sidecar(const std::string& path) {
    std::error_code ec;
    fs::remove(path, ec);
}

// Deterministic pseudo-random generator (xorshift32) seeded from the GENERATEd value.
static uint32_t xorshift(uint32_t& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

// Map xorshift output to an int range [lo, hi] (inclusive).
static int rand_int(uint32_t& state, int lo, int hi) {
    uint32_t v = xorshift(state);
    return lo + static_cast<int>(v % static_cast<uint32_t>(hi - lo + 1));
}

// Pick one of the canonical frame sizes the generator emits (128 / 256 / 512),
// per Requirement 2.4. The selection is driven by the deterministic rng so each
// outer iteration covers a different size.
static int pick_frame_size(uint32_t& state) {
    static const int sizes[] = {128, 256, 512};
    return sizes[rand_int(state, 0, 2)];
}

// ============================================================================
// Property 1: SpriteSheet field round-trip (no magic numbers)
//
// For any Sidecar_JSON with a generated `atlas` name and generated
// `frame_width`, `frame_height`, `columns` (> 0), and `total_frames` (> 0) —
// including frame sizes 128, 256, and 512 — loading any valid clip SHALL
// produce a SpriteSheet whose atlas_filename, frame_width, frame_height,
// columns, and total_frames equal the originating Sidecar_JSON values, with
// no substituted C++ literal.
//
// Feature: 090-15-generator-spec, Property 1: SpriteSheet field round-trip (no magic numbers)
// **Validates: Requirements 1.1, 1.2, 1.3, 1.7, 2.1, 2.4, 8.5**
// ============================================================================
TEST_CASE("Property 1: SpriteSheet field round-trip (no magic numbers)",
          "[Engine][sidecar_loader][property]") {

    // Outer: distinct sidecars. Inner: value variations per sidecar.
    auto outer_seed = GENERATE(take(NUM_OUTER_TESTS, random(1, 100000)));
    auto inner_seed = GENERATE(take(NUM_INNER_TESTS, random(1, 100000)));

    // Combine both seeds into a non-zero xorshift state (the | 1u guarantees != 0).
    uint32_t rng = (static_cast<uint32_t>(outer_seed) ^ (static_cast<uint32_t>(inner_seed) << 1)) | 1u;

    // Generated SpriteSheet values — sourced ONLY from the generated sidecar,
    // never from a literal in the loader.
    std::string atlas = "atlas_" + std::to_string(outer_seed) + "_"
                      + std::to_string(inner_seed) + ".png";
    int frame_width  = pick_frame_size(rng);   // includes 128 / 256 / 512
    int frame_height = pick_frame_size(rng);   // includes 128 / 256 / 512
    int columns      = rand_int(rng, 1, 8);    // columns > 0
    int total_frames = rand_int(rng, 1, 32);   // total_frames > 0

    // A valid clip is required for load() to succeed. Keep it inside range:
    // start_frame == 0, frame_count in [1, total_frames].
    int frame_count = rand_int(rng, 1, total_frames);
    float frame_duration = static_cast<float>(rand_int(rng, 1, 50)) / 100.0f;
    bool looping = (rand_int(rng, 0, 1) == 1);

    json doc;
    doc["atlas"]        = atlas;
    doc["frame_width"]  = frame_width;
    doc["frame_height"] = frame_height;
    doc["columns"]      = columns;
    doc["total_frames"] = total_frames;
    doc["animations"]   = {
        {"anim", {
            {"start_frame", 0},
            {"frame_count", frame_count},
            {"frame_duration", frame_duration},
            {"looping", looping}
        }}
    };

    std::string path = write_temp_sidecar(doc, "p1", outer_seed * 31 + inner_seed);

    auto loaded = sidecar_loader::load(path, "anim");

    // SpriteSheet fields must equal the originating sidecar values exactly,
    // with no substituted C++ literal.
    REQUIRE(loaded.sprite_sheet.atlas_filename == atlas);
    REQUIRE(loaded.sprite_sheet.frame_width    == frame_width);
    REQUIRE(loaded.sprite_sheet.frame_height   == frame_height);
    REQUIRE(loaded.sprite_sheet.columns        == columns);
    REQUIRE(loaded.sprite_sheet.total_frames   == total_frames);

    cleanup_sidecar(path);
}

// ============================================================================
// Property 2: Animation clip fidelity and initialization
//
// For any Sidecar_JSON and any clip present in its `animations` object whose
// start_frame + frame_count <= total_frames, loading that clip SHALL produce
// an Animation whose start_frame, frame_count, frame_duration, and looping
// equal the clip's Sidecar_JSON values, and whose current_frame equals
// start_frame, elapsed equals 0.0, playing equals true, and finished equals
// false; the produced SpriteSheet's current_frame SHALL also equal that
// clip's start_frame.
//
// Feature: 090-15-generator-spec, Property 2: Animation clip fidelity and initialization
// **Validates: Requirements 1.4, 1.5, 1.6, 2.2**
// ============================================================================
TEST_CASE("Property 2: Animation clip fidelity and initialization",
          "[Engine][sidecar_loader][property]") {

    // Outer: distinct sidecars. Inner: clip value variations per sidecar.
    auto outer_seed = GENERATE(take(NUM_OUTER_TESTS, random(1, 100000)));
    auto inner_seed = GENERATE(take(NUM_INNER_TESTS, random(1, 100000)));

    // Combine both seeds into a non-zero xorshift state (the | 1u guarantees != 0).
    uint32_t rng = (static_cast<uint32_t>(outer_seed) ^ (static_cast<uint32_t>(inner_seed) << 1)) | 1u;

    // Generated SpriteSheet values — sourced ONLY from the generated sidecar.
    std::string atlas = "atlas_" + std::to_string(outer_seed) + "_"
                      + std::to_string(inner_seed) + ".png";
    int frame_width  = pick_frame_size(rng);
    int frame_height = pick_frame_size(rng);
    int columns      = rand_int(rng, 1, 8);
    int total_frames = rand_int(rng, 1, 32);

    // Generated clip kept in range: start_frame in [0, total_frames - 1] and
    // frame_count in [1, total_frames - start_frame], so that
    // start_frame + frame_count <= total_frames.
    int start_frame = rand_int(rng, 0, total_frames - 1);
    int frame_count = rand_int(rng, 1, total_frames - start_frame);
    float frame_duration = static_cast<float>(rand_int(rng, 1, 50)) / 100.0f;
    bool looping = (rand_int(rng, 0, 1) == 1);

    json doc;
    doc["atlas"]        = atlas;
    doc["frame_width"]  = frame_width;
    doc["frame_height"] = frame_height;
    doc["columns"]      = columns;
    doc["total_frames"] = total_frames;
    doc["animations"]   = {
        {"clip", {
            {"start_frame", start_frame},
            {"frame_count", frame_count},
            {"frame_duration", frame_duration},
            {"looping", looping}
        }}
    };

    std::string path = write_temp_sidecar(doc, "p2", outer_seed * 31 + inner_seed);

    auto loaded = sidecar_loader::load(path, "clip");

    // Animation clip fields must equal the originating sidecar values exactly.
    REQUIRE(loaded.animation.start_frame    == start_frame);
    REQUIRE(loaded.animation.frame_count    == frame_count);
    REQUIRE(loaded.animation.frame_duration == frame_duration);
    REQUIRE(loaded.animation.looping        == looping);

    // current_frame initialized to start_frame on both components.
    REQUIRE(loaded.animation.current_frame    == start_frame);
    REQUIRE(loaded.sprite_sheet.current_frame == start_frame);

    // Fixed initialization values.
    REQUIRE(loaded.animation.elapsed  == 0.0f);
    REQUIRE(loaded.animation.playing  == true);
    REQUIRE(loaded.animation.finished == false);

    cleanup_sidecar(path);
}

// ============================================================================
// Property 3: Frame-slicing conformance over the clip range
//
// For any loaded SpriteSheet and clip with start_frame + frame_count <=
// total_frames, the produced current_frame SHALL equal start_frame, and for
// every frame index i in [start_frame, start_frame + frame_count) the unchanged
// Frame_Slicing_Formula (col = i % columns, row = i / columns) SHALL satisfy
// i % columns < columns and i / columns < ceil(total_frames / columns).
//
// Feature: 090-15-generator-spec, Property 3: Frame-slicing conformance over the clip range
// **Validates: Requirements 6.4, 8.4**
// ============================================================================
TEST_CASE("Property 3: Frame-slicing conformance over the clip range",
          "[Engine][sidecar_loader][property]") {

    // Outer: distinct sidecars. Inner: clip value variations per sidecar.
    auto outer_seed = GENERATE(take(NUM_OUTER_TESTS, random(1, 100000)));
    auto inner_seed = GENERATE(take(NUM_INNER_TESTS, random(1, 100000)));

    // Combine both seeds into a non-zero xorshift state (the | 1u guarantees != 0).
    uint32_t rng = (static_cast<uint32_t>(outer_seed) ^ (static_cast<uint32_t>(inner_seed) << 1)) | 1u;

    // Generated SpriteSheet values — sourced ONLY from the generated sidecar.
    std::string atlas = "atlas_" + std::to_string(outer_seed) + "_"
                      + std::to_string(inner_seed) + ".png";
    int frame_width  = pick_frame_size(rng);
    int frame_height = pick_frame_size(rng);
    int columns      = rand_int(rng, 1, 8);    // columns > 0
    int total_frames = rand_int(rng, 1, 32);   // total_frames > 0

    // Generated clip kept in range: start_frame in [0, total_frames - 1] and
    // frame_count in [1, total_frames - start_frame], so that
    // start_frame + frame_count <= total_frames.
    int start_frame = rand_int(rng, 0, total_frames - 1);
    int frame_count = rand_int(rng, 1, total_frames - start_frame);
    float frame_duration = static_cast<float>(rand_int(rng, 1, 50)) / 100.0f;
    bool looping = (rand_int(rng, 0, 1) == 1);

    json doc;
    doc["atlas"]        = atlas;
    doc["frame_width"]  = frame_width;
    doc["frame_height"] = frame_height;
    doc["columns"]      = columns;
    doc["total_frames"] = total_frames;
    doc["animations"]   = {
        {"clip", {
            {"start_frame", start_frame},
            {"frame_count", frame_count},
            {"frame_duration", frame_duration},
            {"looping", looping}
        }}
    };

    std::string path = write_temp_sidecar(doc, "p3", outer_seed * 31 + inner_seed);

    auto loaded = sidecar_loader::load(path, "clip");

    // The produced current_frame equals the clip's start_frame.
    REQUIRE(loaded.sprite_sheet.current_frame == start_frame);

    // Row count under the unchanged Frame_Slicing_Formula: ceil(total_frames / columns).
    int loaded_columns = loaded.sprite_sheet.columns;
    int loaded_total   = loaded.sprite_sheet.total_frames;
    int row_count = (loaded_total + loaded_columns - 1) / loaded_columns;  // ceil

    // For every frame index i in [start_frame, start_frame + frame_count) the
    // slicing formula must land within the grid: col < columns and row < row_count.
    for (int i = start_frame; i < start_frame + frame_count; ++i) {
        int col = i % loaded_columns;   // Frame_Slicing_Formula: col = i % columns
        int row = i / loaded_columns;   // Frame_Slicing_Formula: row = i / columns
        REQUIRE(col < loaded_columns);
        REQUIRE(row < row_count);
    }

    cleanup_sidecar(path);
}

// ============================================================================
// Property 4: Invalid sidecars are rejected without producing components
//
// For any Sidecar_JSON that is invalid in exactly one generated way — a missing
// required top-level field, a requested clip whose
// start_frame + frame_count > total_frames, a columns <= 0, or a
// total_frames <= 0 — the Sidecar_Loader SHALL raise an error and SHALL NOT
// produce a SpriteSheet or Animation (the call throws rather than returning a
// partial result).
//
// Feature: 090-15-generator-spec, Property 4: Invalid sidecars are rejected without producing components
// **Validates: Requirements 3.3, 3.5, 3.6**
// ============================================================================
TEST_CASE("Property 4: Invalid sidecars are rejected without producing components",
          "[Engine][sidecar_loader][property]") {

    // The four ways a sidecar can be made invalid in exactly one place. GENERATE
    // selects among them so every invalidation mode is exercised across iterations.
    enum InvalidMode {
        MissingTopLevelField,           // R3.3: drop a required top-level field
        ClipRangeExceedsTotal,          // R3.5: start_frame + frame_count > total_frames
        ColumnsNonPositive,             // R3.6: columns <= 0
        TotalFramesNonPositive          // R3.6: total_frames <= 0
    };
    auto mode = GENERATE(MissingTopLevelField, ClipRangeExceedsTotal,
                         ColumnsNonPositive, TotalFramesNonPositive);

    // Outer: distinct sidecars. Inner: value variations per sidecar.
    auto outer_seed = GENERATE(take(NUM_OUTER_TESTS, random(1, 100000)));
    auto inner_seed = GENERATE(take(NUM_INNER_TESTS, random(1, 100000)));

    // Combine both seeds into a non-zero xorshift state (the | 1u guarantees != 0).
    uint32_t rng = (static_cast<uint32_t>(outer_seed) ^ (static_cast<uint32_t>(inner_seed) << 1)) | 1u;

    // Start from a fully valid sidecar with an in-range clip, then corrupt exactly
    // one aspect according to the selected mode.
    std::string atlas = "atlas_" + std::to_string(outer_seed) + "_"
                      + std::to_string(inner_seed) + ".png";
    int frame_width  = pick_frame_size(rng);
    int frame_height = pick_frame_size(rng);
    int columns      = rand_int(rng, 1, 8);    // columns > 0 (valid baseline)
    int total_frames = rand_int(rng, 1, 32);   // total_frames > 0 (valid baseline)

    // Valid in-range clip: start_frame + frame_count <= total_frames.
    int start_frame = rand_int(rng, 0, total_frames - 1);
    int frame_count = rand_int(rng, 1, total_frames - start_frame);
    float frame_duration = static_cast<float>(rand_int(rng, 1, 50)) / 100.0f;
    bool looping = (rand_int(rng, 0, 1) == 1);

    json doc;
    doc["atlas"]        = atlas;
    doc["frame_width"]  = frame_width;
    doc["frame_height"] = frame_height;
    doc["columns"]      = columns;
    doc["total_frames"] = total_frames;
    doc["animations"]   = {
        {"clip", {
            {"start_frame", start_frame},
            {"frame_count", frame_count},
            {"frame_duration", frame_duration},
            {"looping", looping}
        }}
    };

    // Apply exactly one invalidation.
    switch (mode) {
        case MissingTopLevelField: {
            // Drop one of the six required top-level fields (R3.3). Selection is
            // driven by the deterministic rng so different fields are removed
            // across iterations.
            static const char* required[] = {
                "atlas", "frame_width", "frame_height",
                "columns", "total_frames", "animations"
            };
            doc.erase(required[rand_int(rng, 0, 5)]);
            break;
        }
        case ClipRangeExceedsTotal: {
            // Make start_frame + frame_count strictly greater than total_frames (R3.5).
            doc["animations"]["clip"]["frame_count"] = total_frames - start_frame + 1;
            break;
        }
        case ColumnsNonPositive: {
            // columns <= 0 (R3.6): pick 0 or a negative value.
            doc["columns"] = -rand_int(rng, 0, 8);
            break;
        }
        case TotalFramesNonPositive: {
            // total_frames <= 0 (R3.6): pick 0 or a negative value.
            doc["total_frames"] = -rand_int(rng, 0, 32);
            break;
        }
    }

    std::string path = write_temp_sidecar(doc, "p4", outer_seed * 31 + inner_seed);

    // The loader must throw rather than returning a partial result — no
    // SpriteSheet/Animation is produced on any invalid input.
    REQUIRE_THROWS_AS(sidecar_loader::load(path, "clip"), std::runtime_error);

    cleanup_sidecar(path);
}
