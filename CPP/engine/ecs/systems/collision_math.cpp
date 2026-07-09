#include "collision_math.hpp"
#include <algorithm>
#include <cmath>

bool aabb_overlap(float ax, float ay, float aw, float ah,
                  float bx, float by, float bw, float bh) {
    // Zero/negative dimensions → no valid AABB
    if (aw <= 0 || ah <= 0 || bw <= 0 || bh <= 0) return false;

    // Compute edges
    float right_a = ax + aw;
    float top_a   = ay + ah;
    float right_b = bx + bw;
    float top_b   = by + bh;

    // Separation test (strict inequality — touching = no overlap)
    if (right_a <= bx || right_b <= ax || top_a <= by || top_b <= ay) return false;

    return true;
}

bool layers_compatible(uint8_t layer_a, uint8_t mask_a,
                       uint8_t layer_b, uint8_t mask_b) {
    return (layer_a & mask_b) != 0 && (layer_b & mask_a) != 0;
}

bool circle_circle_overlap(float x1, float y1, float r1,
                           float x2, float y2, float r2) {
    if (r1 <= 0.0f || r2 <= 0.0f) return false;
    float dx = x2 - x1;
    float dy = y2 - y1;
    float dist_sq = dx * dx + dy * dy;
    float radii_sum = r1 + r2;
    return dist_sq < radii_sum * radii_sum;
}

bool circle_aabb_overlap(float cx, float cy, float r,
                         float ax, float ay, float aw, float ah) {
    if (r <= 0.0f || aw <= 0.0f || ah <= 0.0f) return false;

    // Clamp circle center to AABB bounds
    float closest_x = std::max(ax, std::min(cx, ax + aw));
    float closest_y = std::max(ay, std::min(cy, ay + ah));

    float dx = cx - closest_x;
    float dy = cy - closest_y;

    return (dx * dx + dy * dy) < r * r;
}

bool obb_obb_overlap(float cx1, float cy1, float hw1, float hh1, float angle1,
                     float cx2, float cy2, float hw2, float hh2, float angle2) {
    // Validate half-extents
    if (hw1 <= 0.0f || hh1 <= 0.0f || hw2 <= 0.0f || hh2 <= 0.0f) return false;

    // Compute local axes for OBB1
    float cos1 = std::cos(angle1);
    float sin1 = std::sin(angle1);
    float a1x_x = cos1,  a1x_y = sin1;   // OBB1 local X axis
    float a1y_x = -sin1, a1y_y = cos1;   // OBB1 local Y axis

    // Compute local axes for OBB2
    float cos2 = std::cos(angle2);
    float sin2 = std::sin(angle2);
    float a2x_x = cos2,  a2x_y = sin2;   // OBB2 local X axis
    float a2y_x = -sin2, a2y_y = cos2;   // OBB2 local Y axis

    // Displacement vector between centers
    float dx = cx2 - cx1;
    float dy = cy2 - cy1;

    // Test each of the 4 separating axes
    // For each axis: project displacement and half-extents, check separation

    // Axis 1: OBB1 local X (a1x)
    {
        float dist = std::abs(dx * a1x_x + dy * a1x_y);
        float r1 = hw1;  // |a1x · a1x| = 1, |a1y · a1x| = 0
        float r2 = hw2 * std::abs(a2x_x * a1x_x + a2x_y * a1x_y)
                 + hh2 * std::abs(a2y_x * a1x_x + a2y_y * a1x_y);
        if (dist >= r1 + r2) return false;
    }

    // Axis 2: OBB1 local Y (a1y)
    {
        float dist = std::abs(dx * a1y_x + dy * a1y_y);
        float r1 = hh1;  // |a1x · a1y| = 0, |a1y · a1y| = 1
        float r2 = hw2 * std::abs(a2x_x * a1y_x + a2x_y * a1y_y)
                 + hh2 * std::abs(a2y_x * a1y_x + a2y_y * a1y_y);
        if (dist >= r1 + r2) return false;
    }

    // Axis 3: OBB2 local X (a2x)
    {
        float dist = std::abs(dx * a2x_x + dy * a2x_y);
        float r1 = hw1 * std::abs(a1x_x * a2x_x + a1x_y * a2x_y)
                 + hh1 * std::abs(a1y_x * a2x_x + a1y_y * a2x_y);
        float r2 = hw2;  // |a2x · a2x| = 1, |a2y · a2x| = 0
        if (dist >= r1 + r2) return false;
    }

    // Axis 4: OBB2 local Y (a2y)
    {
        float dist = std::abs(dx * a2y_x + dy * a2y_y);
        float r1 = hw1 * std::abs(a1x_x * a2y_x + a1x_y * a2y_y)
                 + hh1 * std::abs(a1y_x * a2y_x + a1y_y * a2y_y);
        float r2 = hh2;  // |a2x · a2y| = 0, |a2y · a2y| = 1
        if (dist >= r1 + r2) return false;
    }

    // No separating axis found — OBBs overlap
    return true;
}

bool obb_circle_overlap(float obb_cx, float obb_cy, float hw, float hh, float angle,
                        float circle_cx, float circle_cy, float radius) {
    // Validate inputs
    if (radius <= 0.0f || hw <= 0.0f || hh <= 0.0f) return false;

    // Compute relative vector from OBB center to circle center
    float dx = circle_cx - obb_cx;
    float dy = circle_cy - obb_cy;

    // Rotate into OBB local space (rotate by -angle)
    float cos_a = std::cos(angle);
    float sin_a = std::sin(angle);
    float local_x =  dx * cos_a + dy * sin_a;
    float local_y = -dx * sin_a + dy * cos_a;

    // Clamp to OBB extent to find closest point on OBB
    float closest_x = std::max(-hw, std::min(local_x, hw));
    float closest_y = std::max(-hh, std::min(local_y, hh));

    // Compute squared distance from circle center to closest point
    float diff_x = local_x - closest_x;
    float diff_y = local_y - closest_y;
    float dist_sq = diff_x * diff_x + diff_y * diff_y;

    return dist_sq < radius * radius;
}
