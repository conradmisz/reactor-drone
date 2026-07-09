#include "engine/sidecar_loader.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>
#include <string>

using json = nlohmann::json;

namespace sidecar_loader {

LoadedSprite load(const std::string& sidecar_path, const std::string& clip_name) {
    // --- 1. Open the file (R3.1) ---
    std::ifstream file(sidecar_path);
    if (!file.is_open()) {
        throw std::runtime_error("Sidecar_Loader: could not open file: " + sidecar_path);
    }

    // --- 2. Parse JSON (R3.2) ---
    json data;
    try {
        data = json::parse(file);
    } catch (const json::parse_error& e) {
        throw std::runtime_error(
            std::string("Sidecar_Loader: JSON parse error: ") + e.what());
    }

    // --- 3. Check each required top-level field (R3.3) ---
    static const char* const required_fields[] = {
        "atlas", "frame_width", "frame_height", "columns", "total_frames", "animations"
    };
    for (const char* field : required_fields) {
        if (!data.contains(field)) {
            throw std::runtime_error(
                std::string("Sidecar_Loader: missing required field: ") + field);
        }
    }

    // --- 4. Read SpriteSheet values (no hard-coded frame size/grid/total) ---
    std::string atlas        = data["atlas"].get<std::string>();
    int frame_width          = data["frame_width"].get<int>();
    int frame_height         = data["frame_height"].get<int>();
    int columns              = data["columns"].get<int>();
    int total_frames         = data["total_frames"].get<int>();

    // --- 5. Validate columns > 0 and total_frames > 0 (R3.6) ---
    if (columns <= 0) {
        throw std::runtime_error(
            "Sidecar_Loader: field 'columns' must be > 0 (got " +
            std::to_string(columns) + ")");
    }
    if (total_frames <= 0) {
        throw std::runtime_error(
            "Sidecar_Loader: field 'total_frames' must be > 0 (got " +
            std::to_string(total_frames) + ")");
    }

    // --- 6. Look up the requested clip (R3.4) ---
    const json& animations = data["animations"];
    if (!animations.contains(clip_name)) {
        throw std::runtime_error(
            "Sidecar_Loader: unknown clip: " + clip_name);
    }
    const json& clip = animations[clip_name];

    // --- 7. Read clip values ---
    int   start_frame    = clip["start_frame"].get<int>();
    int   frame_count    = clip["frame_count"].get<int>();
    float frame_duration = clip["frame_duration"].get<float>();
    bool  looping        = clip["looping"].get<bool>();

    // --- 8. Validate clip range (R3.5) ---
    if (start_frame + frame_count > total_frames) {
        throw std::runtime_error(
            "Sidecar_Loader: clip '" + clip_name +
            "' exceeds total_frames (start_frame + frame_count > total_frames)");
    }

    // --- 9. Only now construct and return the populated components ---
    LoadedSprite result;

    // SpriteSheet — every value copied straight from the sidecar (R1.2, R1.3,
    // R2.1, R2.4); current_frame initialized to the clip start frame (R1.5).
    result.sprite_sheet.atlas_filename = atlas;
    result.sprite_sheet.frame_width    = frame_width;
    result.sprite_sheet.frame_height   = frame_height;
    result.sprite_sheet.columns        = columns;
    result.sprite_sheet.total_frames   = total_frames;
    result.sprite_sheet.current_frame  = start_frame;

    // Animation — clip values copied unaltered (R1.4, R2.2); current_frame set
    // to start_frame (R1.5); transient fields initialized (R1.6).
    result.animation.start_frame    = start_frame;
    result.animation.frame_count    = frame_count;
    result.animation.frame_duration = frame_duration;
    result.animation.looping        = looping;
    result.animation.current_frame  = start_frame;
    result.animation.elapsed        = 0.0f;
    result.animation.playing        = true;
    result.animation.finished       = false;

    return result;
}

std::vector<float> load_facing_angles(const std::string& sidecar_path,
                                      const std::string& clip_name) {
    std::ifstream file(sidecar_path);
    if (!file.is_open()) {
        throw std::runtime_error("Sidecar_Loader: could not open file: " + sidecar_path);
    }

    json data;
    try {
        data = json::parse(file);
    } catch (const json::parse_error& e) {
        throw std::runtime_error(
            std::string("Sidecar_Loader: JSON parse error: ") + e.what());
    }

    if (!data.contains("animations") || !data["animations"].contains(clip_name)) {
        throw std::runtime_error("Sidecar_Loader: unknown clip: " + clip_name);
    }

    const json& clip = data["animations"][clip_name];
    std::vector<float> angles;
    // Optional: a non-directional clip simply has no table -> return empty.
    if (clip.contains("facing_angles_deg") && clip["facing_angles_deg"].is_array()) {
        for (const auto& a : clip["facing_angles_deg"]) {
            angles.push_back(a.get<float>());
        }
    }
    return angles;
}

}  // namespace sidecar_loader
