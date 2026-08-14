// Unit tests for particle_mesh.hpp — the pure quad/UV geometry of the v3 Tier 9
// batched additive particle renderer (D202). No SDL, no window.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>

#include "engine/ecs/systems/particle_mesh.hpp"

using namespace particle_mesh;
using Catch::Approx;

TEST_CASE("one particle becomes four corner verts centred on its position",
          "[particlemesh]") {
    std::vector<Quad> in{Quad{100.0f, 50.0f, 10.0f, {255, 128, 64, 255}}};
    auto verts = build_mesh(in);
    REQUIRE(verts.size() == 4);
    // Corners sit at +/- half the size on each axis.
    CHECK(verts[0].x == Approx(95.0f));
    CHECK(verts[0].y == Approx(45.0f));
    CHECK(verts[3].x == Approx(105.0f));
    CHECK(verts[3].y == Approx(55.0f));
    // Every vert carries the particle's colour unchanged.
    for (const auto& v : verts) {
        CHECK(v.r == 255);
        CHECK(v.g == 128);
        CHECK(v.b == 64);
        CHECK(v.a == 255);
    }
}

TEST_CASE("quad UVs span the full glow texture", "[particlemesh]") {
    std::vector<Quad> in{Quad{0.0f, 0.0f, 4.0f, {255, 255, 255, 255}}};
    auto verts = build_mesh(in);
    REQUIRE(verts.size() == 4);
    // Top-left (0,0) through bottom-right (1,1): the whole disc, so the soft
    // radial falloff reaches transparent at every edge. THIS is what stops the
    // box halo (bugs/004).
    CHECK(verts[0].u == Approx(0.0f));
    CHECK(verts[0].v == Approx(0.0f));
    CHECK(verts[3].u == Approx(1.0f));
    CHECK(verts[3].v == Approx(1.0f));
}

TEST_CASE("indices wind two triangles per particle", "[particlemesh]") {
    std::vector<Quad> in{
        Quad{0.0f, 0.0f, 4.0f, {255, 255, 255, 255}},
        Quad{20.0f, 20.0f, 4.0f, {255, 255, 255, 255}},
    };
    auto idx = quad_indices(in.size());
    // 2 quads -> 2 triangles each -> 12 indices.
    REQUIRE(idx.size() == 12);
    // First quad: 0,1,2 and 1,3,2 over its own four verts.
    CHECK(idx[0] == 0); CHECK(idx[1] == 1); CHECK(idx[2] == 2);
    CHECK(idx[3] == 1); CHECK(idx[4] == 3); CHECK(idx[5] == 2);
    // Second quad is offset by four verts, never reindexing the first.
    CHECK(idx[6] == 4); CHECK(idx[11] == 6);
}

TEST_CASE("a zero-size particle emits no NaN and stays at its centre",
          "[particlemesh]") {
    std::vector<Quad> in{Quad{7.0f, 9.0f, 0.0f, {255, 255, 255, 255}}};
    auto verts = build_mesh(in);
    REQUIRE(verts.size() == 4);
    for (const auto& v : verts) {
        CHECK(std::isfinite(v.x));
        CHECK(std::isfinite(v.y));
        CHECK(v.x == Approx(7.0f));
        CHECK(v.y == Approx(9.0f));
    }
}

TEST_CASE("an empty particle set builds an empty mesh", "[particlemesh]") {
    CHECK(build_mesh({}).empty());
    CHECK(quad_indices(0).empty());
}
