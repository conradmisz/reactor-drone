/**
 * bloom_math.hpp — pure helpers for the render-target bloom chain (v3 Tier 1).
 *
 * Engine-free and window-free so the chain geometry and intensity handling are
 * unit-testable. The BloomSystem owns the SDL objects; everything decidable
 * without a renderer is decided here.
 */
#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

namespace bloom_math {

/** One downsample level: the pixel size of its render-target texture. */
struct LevelSize {
    int w = 0;
    int h = 0;
};

/**
 * Sizes for a halving downsample chain starting from a base surface.
 *
 * Level 0 is base/2, each subsequent level halves again (rounding up, so a
 * dimension never reaches 0). The chain stops early once a dimension would
 * drop below `min_edge` — blurring below ~8px adds nothing but a mud tint.
 * `levels <= 0` or a degenerate base yields an empty chain (bloom disabled).
 */
inline std::vector<LevelSize> chain_sizes(int base_w, int base_h, int levels,
                                          int min_edge = 8) {
    std::vector<LevelSize> out;
    if (base_w <= 0 || base_h <= 0) return out;
    int w = base_w, h = base_h;
    for (int i = 0; i < levels; ++i) {
        w = (w + 1) / 2;
        h = (h + 1) / 2;
        if (w < min_edge || h < min_edge) break;
        out.push_back(LevelSize{w, h});
    }
    return out;
}

/**
 * Per-level additive composite intensity, as an SDL alpha-mod byte.
 *
 * `intensities[i]` maps to level i; a missing entry falls back to `fallback`.
 * Values clamp to [0,1] before scaling — an authored 1.5 is a hard white-out,
 * not a feature.
 */
inline unsigned char level_alpha(const std::vector<float>& intensities,
                                 std::size_t level, float fallback) {
    float v = level < intensities.size() ? intensities[level] : fallback;
    v = std::clamp(v, 0.0f, 1.0f);
    return static_cast<unsigned char>(v * 255.0f + 0.5f);
}

} // namespace bloom_math
