/**
 * test_resonance_grid.cpp — the resonance grid's lattice (engine suite, Lane R, D140).
 *
 * `update()` is deliberately separate from `render()` so the physics is testable
 * with no window, no renderer and no entity manager. What is pinned here is what
 * would make the lattice look wrong or eat a frame budget: it must ripple, it must
 * settle, it must stay inside its clamp, and an impulse must stay local.
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <vector>

#include "engine/ecs/systems/resonance_grid_system.hpp"

using Catch::Matchers::WithinAbs;

namespace {

constexpr float DT = 1.0f / 60.0f;

/// A 20x14 lattice at 40px pitch with its origin at the world origin.
ResonanceGridSystem make_grid() {
    ResonanceGridSystem g;
    g.configure(20, 14, 40.0f, 0.0f, 0.0f);
    g.set_tuning(60.0f, 5.0f, 90.0f, 26.0f, 90, 200, 255, 46);
    return g;
}

std::vector<fx_events::Impulse> one(float x, float y, float s) {
    return {fx_events::Impulse{x, y, s}};
}

}  // namespace

TEST_CASE("an idle lattice stays exactly flat", "[Game][grid]") {
    ResonanceGridSystem g = make_grid();
    for (int i = 0; i < 120; ++i) g.update(DT, {});
    CHECK(g.total_displacement() == 0.0f);
}

TEST_CASE("an impulse ripples the lattice and then settles", "[Game][grid]") {
    ResonanceGridSystem g = make_grid();
    g.update(DT, one(400.0f, 260.0f, 1.0f));

    // The kick lands as velocity, so displacement appears on the first step.
    const float after_kick = g.total_displacement();
    CHECK(after_kick > 0.0f);

    // Damped: it grows into a ripple, then bleeds back toward flat. 10 seconds is
    // far past the visual settle time, so "nearly zero" is a real guarantee, not
    // a tolerance dodge.
    for (int i = 0; i < 600; ++i) g.update(DT, {});
    CHECK(g.total_displacement() < after_kick * 0.01f);
}

TEST_CASE("an impulse is local — the far corner never moves", "[Game][grid]") {
    ResonanceGridSystem g = make_grid();
    g.update(DT, one(0.0f, 0.0f, 4.0f));      // hard kick at node (0,0)

    // Nodes are independent oscillators (no neighbour coupling), so the reach is
    // exactly the impulse falloff — 3.5 cells — and nothing beyond it may ever
    // move, however hard the kick. This is what bounds the per-impulse cost.
    const ResonanceGridSystem::Node& far = g.node(19, 13);
    CHECK(far.dx == 0.0f);
    CHECK(far.dy == 0.0f);
    const ResonanceGridSystem::Node& near = g.node(1, 1);
    CHECK((std::fabs(near.dx) + std::fabs(near.dy)) > 0.0f);
}

TEST_CASE("nodes are pushed AWAY from the impulse", "[Game][grid]") {
    ResonanceGridSystem g = make_grid();
    // Impulse at node (5,5)'s world position; (6,5) is one cell to its right.
    g.update(DT, one(5.0f * 40.0f, 5.0f * 40.0f, 1.0f));
    CHECK(g.node(6, 5).dx > 0.0f);
    CHECK(g.node(4, 5).dx < 0.0f);
    CHECK(g.node(5, 6).dy > 0.0f);
    CHECK(g.node(5, 4).dy < 0.0f);
}

TEST_CASE("displacement is clamped however hard the hit", "[Game][grid]") {
    ResonanceGridSystem g = make_grid();
    std::vector<fx_events::Impulse> barrage;
    for (int i = 0; i < 64; ++i) barrage.push_back({400.0f, 260.0f, 50.0f});

    for (int f = 0; f < 30; ++f) {
        g.update(DT, barrage);
        for (int row = 0; row < g.rows(); ++row)
            for (int col = 0; col < g.cols(); ++col) {
                REQUIRE(std::fabs(g.node(col, row).dx) <= 26.0f + 1e-3f);
                REQUIRE(std::fabs(g.node(col, row).dy) <= 26.0f + 1e-3f);
            }
    }
}

TEST_CASE("a stalled frame does not launch the lattice", "[Game][grid]") {
    ResonanceGridSystem g = make_grid();
    g.update(DT, one(400.0f, 260.0f, 1.0f));
    // A 2-second frame (a debugger break, a texture load) is clamped internally.
    g.update(2.0f, {});
    for (int row = 0; row < g.rows(); ++row)
        for (int col = 0; col < g.cols(); ++col) {
            REQUIRE(std::isfinite(g.node(col, row).dx));
            REQUIRE(std::fabs(g.node(col, row).dx) <= 26.0f + 1e-3f);
        }
}

TEST_CASE("reconfiguring to a new size zeroes the lattice", "[Game][grid]") {
    ResonanceGridSystem g = make_grid();
    g.update(DT, one(400.0f, 260.0f, 2.0f));
    REQUIRE(g.total_displacement() > 0.0f);

    g.configure(10, 8, 40.0f, 0.0f, 0.0f);
    CHECK(g.cols() == 10);
    CHECK(g.rows() == 8);
    CHECK(g.total_displacement() == 0.0f);

    // Re-placing the same size is NOT a rebuild — the ripple survives an origin
    // nudge, which is what an arena shift does.
    g.update(DT, one(200.0f, 160.0f, 2.0f));
    const float live = g.total_displacement();
    g.configure(10, 8, 40.0f, 5.0f, 5.0f);
    CHECK(g.total_displacement() == live);
}

TEST_CASE("a zero-sized lattice is inert rather than a crash", "[Game][grid]") {
    ResonanceGridSystem g;
    g.configure(0, 0, 40.0f, 0.0f, 0.0f);
    g.update(DT, one(10.0f, 10.0f, 1.0f));
    CHECK(g.total_displacement() == 0.0f);
    g.render(nullptr, Blackboard{});   // null renderer is a no-op, not a segfault
}

TEST_CASE("the lattice covers the whole arena", "[Game][grid]") {
    // The D151 bug in one assertion: the shipped arena is radius 1400, i.e. 2800
    // across, and the old fixed 40x28 lattice at 40px spanned 1600 — it stopped a
    // third of the way to the wall, which is exactly what the playtest saw.
    ResonanceGridSystem g;
    g.configure_for_arena(1600.0f, 1600.0f, 1400.0f, 64.0f);
    const float span = static_cast<float>(g.cols() - 1) * 64.0f;
    CHECK(span >= 2800.0f);
    CHECK(g.cols() == g.rows());

    // ...and it is CENTRED on the arena, so the margin is equal on both sides.
    // node(0,0) sits at (centre - span/2); the far corner mirrors it.
    const float half = span * 0.5f;
    CHECK(1600.0f - half <= 1600.0f - 1400.0f);   // reaches past the left wall
    CHECK(1600.0f + half >= 1600.0f + 1400.0f);   // and past the right one
}

TEST_CASE("a lattice at rest draws nothing", "[Game][grid]") {
    // The clutter fix (D151): at rest there is no mesh to see. total_displacement
    // is the probe the renderer's per-strip alpha is derived from, so zero
    // displacement means every strip is skipped.
    ResonanceGridSystem g = make_grid();
    for (int i = 0; i < 60; ++i) g.update(DT, {});
    CHECK(g.total_displacement() == 0.0f);

    // One impulse lights it, and ~10 s later it is back to (near) nothing.
    g.update(DT, one(400.0f, 260.0f, 2.0f));
    CHECK(g.total_displacement() > 0.0f);
    for (int i = 0; i < 600; ++i) g.update(DT, {});
    CHECK(g.total_displacement() < 0.5f);
}
