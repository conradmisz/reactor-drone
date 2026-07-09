#ifndef SIDECAR_LOADER_HPP
#define SIDECAR_LOADER_HPP

#include <string>
#include <utility>
#include <vector>
#include "engine/ecs/components.hpp"   // SpriteSheet, Animation (consumed unchanged)

/**
 * Reads a committed Sidecar_JSON and produces a populated SpriteSheet and
 * Animation component for a named animation clip.
 *
 * The loader hard-codes NO frame size, column count, total frame count, or
 * clip range. Every such value is read from the Sidecar_JSON.
 *
 * Contract — all-or-nothing: load() either returns a fully populated
 * LoadedSprite (both SpriteSheet and Animation) or throws std::runtime_error.
 * It never returns a partial result, and on any throw no component is produced.
 *
 * Throws std::runtime_error on:
 *   - unopenable file path (message names the path)
 *   - unparseable JSON (message indicates a parse failure)
 *   - missing required top-level field (message names the field)
 *   - unknown clip name (message names the clip)
 *   - clip with start_frame + frame_count > total_frames (message names the clip)
 *   - columns <= 0 or total_frames <= 0 (message names the field)
 */
namespace sidecar_loader {

/// Result bundle: a fully populated SpriteSheet + Animation pair.
struct LoadedSprite {
    SpriteSheet sprite_sheet;
    Animation   animation;
};

/// Load `clip_name` from the Sidecar_JSON at `sidecar_path`.
/// @returns a fully populated LoadedSprite on success.
/// @throws std::runtime_error (see namespace doc) — never returns a partial result.
LoadedSprite load(const std::string& sidecar_path, const std::string& clip_name);

/// Read the optional `facing_angles_deg` table for a directional ("facings") clip.
///
/// Returns the per-frame on-screen barrel angles (degrees; 0 = screen-right, CCW) that
/// the generator baked into the clip, used to aim a tower at its target. Returns an
/// EMPTY vector when the clip declares no such table (a non-directional clip) — this is
/// not an error. Throws std::runtime_error only for an unopenable file, unparseable
/// JSON, or an unknown clip name (mirrors load()).
std::vector<float> load_facing_angles(const std::string& sidecar_path,
                                      const std::string& clip_name);

}  // namespace sidecar_loader

#endif // SIDECAR_LOADER_HPP
