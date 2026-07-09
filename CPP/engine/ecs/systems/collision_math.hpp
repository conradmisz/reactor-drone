#ifndef COLLISION_MATH_HPP
#define COLLISION_MATH_HPP

#include <cstdint>

/**
 * Determines whether two axis-aligned bounding boxes overlap.
 *
 * Each AABB is defined by its bottom-left corner (x, y) and dimensions
 * (width, height). Uses strict inequality — touching edges/corners
 * do not count as overlap. Returns false if either AABB has zero or
 * negative width/height.
 *
 * Bottom-left coordinate system: (x, y) is the bottom-left corner,
 * top-right corner is (x + width, y + height).
 */
bool aabb_overlap(float ax, float ay, float aw, float ah,
                  float bx, float by, float bw, float bh);

/**
 * Determines whether two entities can collide based on layer/mask filtering.
 *
 * Returns true when BOTH of these conditions hold:
 *   (layer_a & mask_b) != 0
 *   (layer_b & mask_a) != 0
 *
 * Returns false if either layer is 0 (no collision group) or either mask
 * is 0 (collides with nothing), since the bitwise AND with 0 is always 0.
 */
bool layers_compatible(uint8_t layer_a, uint8_t mask_a,
                       uint8_t layer_b, uint8_t mask_b);

/**
 * Determines whether two circles overlap using squared-distance comparison.
 *
 * Returns true when the squared distance between centers is strictly less
 * than the square of the sum of radii. Tangent circles (distance == r1+r2)
 * do NOT overlap, consistent with aabb_overlap() strict inequality.
 * Returns false if either radius is zero or negative.
 */
bool circle_circle_overlap(float x1, float y1, float r1,
                           float x2, float y2, float r2);

/**
 * Determines whether a circle overlaps an axis-aligned bounding box.
 *
 * Uses closest-point-on-AABB: clamp circle center to AABB bounds,
 * then check squared distance from circle center to clamped point.
 * Returns true when squared distance is strictly less than r*r.
 * Tangent does NOT count as overlap. Returns false if radius <= 0
 * or AABB has zero/negative dimensions.
 *
 * AABB is specified as bottom-left corner (ax, ay) with dimensions (aw, ah).
 */
bool circle_aabb_overlap(float cx, float cy, float r,
                         float ax, float ay, float aw, float ah);

/**
 * Determines whether two oriented bounding boxes overlap using the
 * Separating Axis Theorem (SAT) with 4 candidate axes.
 *
 * Each OBB is defined by its center (cx, cy), half-extents (hw, hh),
 * and rotation angle in radians. Tests 2 edge normals from each OBB.
 * Returns false when a separating axis exists (strict inequality,
 * consistent with existing overlap functions). Returns false when
 * either OBB has zero or negative half-extents.
 *
 * No dependency on ECS types — pure math function taking floats.
 */
bool obb_obb_overlap(float cx1, float cy1, float hw1, float hh1, float angle1,
                     float cx2, float cy2, float hw2, float hh2, float angle2);

/**
 * Determines whether an oriented bounding box overlaps a circle.
 *
 * Transforms the circle center into the OBB's local coordinate space
 * by rotating the relative vector by the negative of the OBB's angle,
 * then clamps to the OBB's local extent to find the closest point.
 * Returns true when squared distance from circle center (in local space)
 * to closest point is strictly less than radius². Returns false when
 * radius <= 0 or either half-extent <= 0.
 *
 * No dependency on ECS types — pure math function taking floats.
 */
bool obb_circle_overlap(float obb_cx, float obb_cy, float hw, float hh, float angle,
                        float circle_cx, float circle_cy, float radius);

#endif // COLLISION_MATH_HPP
