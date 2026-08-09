/**
 * ui_focus_math.hpp — Pure focus-cycling helper for keyboard navigation.
 *
 * Header-only, inline, and free of any SDL/ECS dependency so the Tab-order
 * cycling decision is a pure function that property and unit tests exercise
 * directly, without a window. UISystem is a thin shell that builds the ordered
 * focusable list and calls this helper.
 *
 * Added in Phase 7 (o-040-07-polish).
 */

#pragma once

/**
 * The next focused index when cycling a list of `count` focusable widgets.
 *
 * `current` is the current focused index, or -1 when nothing is focused yet.
 * `forward` cycles to the next (true) or previous (false) item, wrapping at the
 * ends. Returns -1 when `count <= 0` (nothing focusable). When `current` is -1,
 * forward yields 0 and backward yields count-1. Always returns a value in
 * [0, count) when count > 0 (out-of-range `current` is normalized by the wrap).
 */
inline int next_focus_index(int count, int current, bool forward) {
    if (count <= 0) return -1;
    if (current < 0) return forward ? 0 : count - 1;
    int step = forward ? 1 : -1;
    return ((current + step) % count + count) % count;
}
