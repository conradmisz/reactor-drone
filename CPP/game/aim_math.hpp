#ifndef AIM_MATH_HPP
#define AIM_MATH_HPP

#include <cmath>
#include "engine/ecs/components.hpp"

/**
 * Pure aim/geometry helpers for the arena shooter (design risk #1: mouse-aimed
 * firing). Kept as free functions in a header so they are unit- and
 * property-testable with no ECS, SDL, or global state.
 *
 * World space is bottom-left origin, +X right, +Y up. Angles are in radians,
 * measured counter-clockwise from +X (screen-right), matching std::atan2.
 */
namespace aim_math {

/// Angle (radians) of the vector from (fx,fy) to (tx,ty). 0 = +X, CCW positive.
/// Returns 0 when the two points coincide (no meaningful direction).
inline float aim_angle(float fx, float fy, float tx, float ty) {
    float dx = tx - fx;
    float dy = ty - fy;
    if (dx == 0.0f && dy == 0.0f) return 0.0f;
    return std::atan2(dy, dx);
}

/// Velocity of magnitude `speed` pointing along `angle` (radians).
inline Velocity velocity_from_angle(float angle, float speed) {
    return Velocity{std::cos(angle) * speed, std::sin(angle) * speed};
}

/// Wrap an angle into (-pi, pi].
inline float wrap_pi(float a) {
    constexpr float TAU = 6.28318530717958647692f;
    a = std::fmod(a, TAU);
    if (a > static_cast<float>(M_PI)) a -= TAU;
    if (a < -static_cast<float>(M_PI)) a += TAU;
    return a;
}

} // namespace aim_math

#endif // AIM_MATH_HPP
