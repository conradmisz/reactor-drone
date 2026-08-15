// Unit tests for line_mesh_math.hpp — the pure geometry of the v3 Tier 5
// neon line renderer. No SDL, no window.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>

#include "engine/ecs/systems/line_mesh_math.hpp"

using namespace line_mesh;
using Catch::Approx;

TEST_CASE("segment_normal is unit-length and perpendicular", "[linemesh]") {
    P2 n = segment_normal(P2{0, 0}, P2{10, 0});
    CHECK(n.x == Approx(0.0f));
    CHECK(n.y == Approx(1.0f));
    // Degenerate segment: zero normal, no NaN.
    P2 z = segment_normal(P2{3, 3}, P2{3, 3});
    CHECK(z.x == 0.0f);
    CHECK(z.y == 0.0f);
}

TEST_CASE("miter_normals preserves width through a right angle", "[linemesh]") {
    // L-shape: right angle at the middle point. The miter normal there must be
    // scaled 1/cos(45°) = sqrt(2) so the ribbon keeps its width.
    std::vector<P2> pts{{0, 0}, {10, 0}, {10, 10}};
    auto n = miter_normals(pts);
    REQUIRE(n.size() == 3);
    float mid_len = std::sqrt(n[1].x * n[1].x + n[1].y * n[1].y);
    CHECK(mid_len == Approx(std::sqrt(2.0f)).margin(1e-4));
    // Endpoints use the plain segment normals (unit length).
    CHECK(std::sqrt(n[0].x * n[0].x + n[0].y * n[0].y) == Approx(1.0f));
    CHECK(std::sqrt(n[2].x * n[2].x + n[2].y * n[2].y) == Approx(1.0f));
}

TEST_CASE("miter_normals clamps hairpins to the miter limit", "[linemesh]") {
    // 180° turn: the unclamped miter would be infinite.
    std::vector<P2> pts{{0, 0}, {10, 0}, {0, 0}};
    auto n = miter_normals(pts, 3.0f);
    REQUIRE(n.size() == 3);
    float mid_len = std::sqrt(n[1].x * n[1].x + n[1].y * n[1].y);
    CHECK(mid_len <= 3.0f + 1e-4f);
}

TEST_CASE("build_ribbon emits 2 verts per point with correct u/v", "[linemesh]") {
    std::vector<P2> pts{{0, 0}, {10, 0}, {20, 0}};
    auto r = build_ribbon(pts, 4.0f);
    REQUIRE(r.size() == 6);
    // v alternates left/right; u runs 0 -> 0.5 -> 1 along the straight line.
    CHECK(r[0].v == 0.0f);
    CHECK(r[1].v == 1.0f);
    CHECK(r[0].u == Approx(0.0f));
    CHECK(r[2].u == Approx(0.5f));
    CHECK(r[4].u == Approx(1.0f));
    // Straight horizontal line, width 4: edges sit at y = ±2.
    CHECK(r[0].y == Approx(2.0f));
    CHECK(r[1].y == Approx(-2.0f));
}

TEST_CASE("build_ribbon degenerate inputs are empty", "[linemesh]") {
    CHECK(build_ribbon({}, 4.0f).empty());
    CHECK(build_ribbon({{0, 0}}, 4.0f).empty());
    CHECK(build_ribbon({{0, 0}, {1, 0}}, 0.0f).empty());
    CHECK(build_ribbon({{0, 0}, {1, 0}}, -2.0f).empty());
}

TEST_CASE("strip_indices triangulates a strip", "[linemesh]") {
    // 6 strip verts -> 4 triangles -> 12 indices, consecutive windows.
    auto idx = strip_indices(6);
    REQUIRE(idx.size() == 12);
    CHECK(idx[0] == 0); CHECK(idx[1] == 1); CHECK(idx[2] == 2);
    CHECK(idx[9] == 3); CHECK(idx[10] == 4); CHECK(idx[11] == 5);
    CHECK(strip_indices(3).empty());
    CHECK(strip_indices(0).empty());
}

TEST_CASE("circle_points closes the loop", "[linemesh]") {
    auto pts = circle_points(5.0f, 7.0f, 3.0f, 32);
    REQUIRE(pts.size() == 33);
    CHECK(pts.front().x == Approx(pts.back().x).margin(1e-4));
    CHECK(pts.front().y == Approx(pts.back().y).margin(1e-4));
    // Every point sits on the radius.
    for (const auto& p : pts) {
        float d = std::sqrt((p.x - 5.0f) * (p.x - 5.0f) +
                            (p.y - 7.0f) * (p.y - 7.0f));
        CHECK(d == Approx(3.0f).margin(1e-3));
    }
    CHECK(circle_points(0, 0, 3.0f, 2).empty());
    CHECK(circle_points(0, 0, -1.0f, 16).empty());
}

// --- v3 Tier 7: per-point widths (tapered trail ribbons) -------------------

TEST_CASE("per-point build_ribbon tapers the half-offset per point", "[line]") {
    std::vector<P2> pts{{0.0f, 0.0f}, {10.0f, 0.0f}, {20.0f, 0.0f}};
    auto v = line_mesh::build_ribbon(pts, std::vector<float>{0.0f, 4.0f, 8.0f});
    REQUIRE(v.size() == 6);
    // Straight horizontal line -> normals are +/-y, so the offset IS half-width.
    CHECK(v[0].y == 0.0f);                    // tail: zero width, edges collapse
    CHECK(v[1].y == 0.0f);
    CHECK(v[2].y == 2.0f);                    // mid: 4/2
    CHECK(v[3].y == -2.0f);
    CHECK(v[4].y == 4.0f);                    // head: 8/2
    CHECK(v[5].y == -4.0f);
}

TEST_CASE("scalar build_ribbon still matches the per-point form", "[line]") {
    std::vector<P2> pts{{0.0f, 0.0f}, {3.0f, 4.0f}, {10.0f, 4.0f}};
    auto a = line_mesh::build_ribbon(pts, 6.0f);
    auto b = line_mesh::build_ribbon(pts, std::vector<float>(3, 6.0f));
    REQUIRE(a.size() == b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        CHECK(a[i].x == b[i].x);
        CHECK(a[i].y == b[i].y);
        CHECK(a[i].u == b[i].u);
    }
}

TEST_CASE("per-point build_ribbon rejects a widths/points size mismatch", "[line]") {
    std::vector<P2> pts{{0.0f, 0.0f}, {10.0f, 0.0f}};
    CHECK(line_mesh::build_ribbon(pts, std::vector<float>{4.0f}).empty());
    CHECK(line_mesh::build_ribbon(pts, std::vector<float>{4.0f, 4.0f, 4.0f}).empty());
}

TEST_CASE("per-point build_ribbon rejects an all-zero taper", "[line]") {
    std::vector<P2> pts{{0.0f, 0.0f}, {10.0f, 0.0f}};
    CHECK(line_mesh::build_ribbon(pts, std::vector<float>{0.0f, 0.0f}).empty());
}
