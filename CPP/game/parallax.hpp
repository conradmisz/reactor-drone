/**
 * parallax.hpp — pure parallax-scroll math (v2, Phase 5).
 *
 * A backdrop is drawn as N tiled layers. Each layer has a `scroll_factor` in
 * [0,1] describing how "attached" it is to the camera: at 1 it is glued to the
 * camera (no relative motion, i.e. infinitely far), at 0 it moves fully against
 * the camera (nearest foreground). The per-axis draw offset for a layer is
 *
 *     offset = camera * (1 - scroll_factor)
 *
 * so a far layer (scroll_factor near 1) barely moves and a near layer
 * (scroll_factor near 0) moves the most. `camera` is the camera's displacement
 * from its rest position on that axis (the follow camera's distance from the
 * arena centre, plus screen shake). Pure and side-effect free so it unit-tests
 * without a game loop.
 */
#ifndef PARALLAX_HPP
#define PARALLAX_HPP

namespace parallax {

/** Per-axis tile offset for one backdrop layer: camera * (1 - scroll_factor). */
inline float parallax_offset(float camera, float scroll_factor) {
    return camera * (1.0f - scroll_factor);
}

}  // namespace parallax

#endif  // PARALLAX_HPP
