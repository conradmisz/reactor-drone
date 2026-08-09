/**
 * ui_fade_math.hpp — Pure curve helper for the screen-transition fade overlay.
 *
 * Header-only, inline, and free of any SDL dependency so the fade-overlay alpha
 * curve is a pure function that property and unit tests can exercise directly,
 * without a window. ScreenFadeSystem is a thin SDL shell that calls this helper.
 *
 * The curve is a fade THROUGH black: alpha rises from 0 to FADE_MAX_ALPHA at the
 * midpoint of a transition and falls back to 0, so a settled screen (progress 0
 * or 1) is never obscured. This realizes the high-level plan's "0 -> 200 alpha
 * over 0.2 s" intent as a standard fade-through-black transition.
 *
 * Added in Phase 6 (o-040-06-lua-screens).
 */

#pragma once

#include <cmath>

/// Seconds for a full screen transition (0 -> 1 progress).
constexpr float FADE_DURATION  = 0.2f;
/// Peak overlay alpha (0..255) reached at the midpoint of a transition.
constexpr int   FADE_MAX_ALPHA = 200;

/**
 * Overlay alpha for a normalized transition progress in [0, 1].
 *
 * Triangle curve: 0 at progress 0 and 1, peaking at FADE_MAX_ALPHA at progress
 * 0.5. Input is clamped to [0, 1] (so out-of-range values are well-defined), and
 * the result is always in [0, FADE_MAX_ALPHA]. Non-decreasing on [0, 0.5] and
 * non-increasing on [0.5, 1]; symmetric about 0.5.
 */
inline int fade_overlay_alpha(float progress) {
    float p = progress;
    if (p < 0.0f) p = 0.0f;
    if (p > 1.0f) p = 1.0f;
    float tri = 1.0f - std::fabs(2.0f * p - 1.0f);
    return static_cast<int>(std::lround(FADE_MAX_ALPHA * tri));
}
