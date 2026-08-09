/**
 * Iteration 3, Lane E / #2 — arena-transition VFX (D76-D79).
 *
 * Three things are worth proving here and none of them need a window:
 *   1. the stagger/scale curve, including the property everything else leans on
 *      (every prop is finished at t == 1);
 *   2. the ordering rule — colliders are gone on the shift frame, long before
 *      the props stop being drawn;
 *   3. the particle cost of a full-arena destruction, measured against the real
 *      ParticleSystem rather than estimated.
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/destruction.hpp"
#include "engine/ecs/systems/particle_system.hpp"

#include "game/arena_vfx.hpp"
#include "game/collision_layers.hpp"

using Catch::Matchers::WithinAbs;

namespace {

// The worst real arena in GameData.json: 20 obstacles + 8 hazards, all of which
// carry a Collider, plus ~97 decorative wall segments that do not.
constexpr int SOLID_PROPS = 28;
constexpr int WALL_PROPS = 97;
constexpr float SHIFT_SECONDS = 5.0f;

// Mirrors spawn_arena_props: walls have Position/Size only, solids also have a
// Collider. That Collider is the debris filter arena_vfx keys off.
std::vector<Entity> make_arena(EntityManager& em, ComponentStorage& cs,
                               int solids, int walls) {
    std::vector<Entity> props;
    for (int i = 0; i < walls; ++i) {
        Entity e = em.create_entity();
        cs.add_component<Position>(e, Position{static_cast<float>(i) * 10.0f, 0.0f});
        cs.add_component<Size>(e, Size{110.0f, 110.0f});
        props.push_back(e);
    }
    for (int i = 0; i < solids; ++i) {
        Entity e = em.create_entity();
        cs.add_component<Position>(e, Position{static_cast<float>(i) * 90.0f, 400.0f});
        cs.add_component<Size>(e, Size{120.0f, 80.0f});
        cs.add_component<Collider>(e,
            Collider{120.0f, 80.0f, layers::OBSTACLE, layers::OBSTACLE_MASK});
        props.push_back(e);
    }
    return props;
}

int live_particles(ComponentStorage& cs) {
    return static_cast<int>(cs.entities_with_component<Particle>().size());
}

}  // namespace

// ---------------------------------------------------------------------------
// 1. The curve
// ---------------------------------------------------------------------------

TEST_CASE("smoothstep is the crossfade curve, clamped", "[arena_vfx][curve]") {
    CHECK_THAT(arena_vfx::smoothstep(0.0f), WithinAbs(0.0f, 1e-6f));
    CHECK_THAT(arena_vfx::smoothstep(0.5f), WithinAbs(0.5f, 1e-6f));
    CHECK_THAT(arena_vfx::smoothstep(1.0f), WithinAbs(1.0f, 1e-6f));
    // Eased at both ends — the whole reason it is not a linear ramp.
    CHECK(arena_vfx::smoothstep(0.1f) < 0.1f);
    CHECK(arena_vfx::smoothstep(0.9f) > 0.9f);
    // Out of range is clamped, not extrapolated.
    CHECK_THAT(arena_vfx::smoothstep(-3.0f), WithinAbs(0.0f, 1e-6f));
    CHECK_THAT(arena_vfx::smoothstep(9.0f), WithinAbs(1.0f, 1e-6f));
}

TEST_CASE("staggered_t fans props out but lands them all by t=1", "[arena_vfx][curve]") {
    constexpr int N = 40;
    constexpr float SPAN = 0.55f;

    // At t=0 only the first prop has started.
    CHECK_THAT(arena_vfx::staggered_t(0.0f, 0, N, SPAN), WithinAbs(0.0f, 1e-6f));
    for (int i = 1; i < N; ++i)
        CHECK_THAT(arena_vfx::staggered_t(0.0f, i, N, SPAN), WithinAbs(0.0f, 1e-6f));
    CHECK(arena_vfx::staggered_t(0.1f, 0, N, SPAN) > 0.0f);
    CHECK_THAT(arena_vfx::staggered_t(0.1f, N - 1, N, SPAN), WithinAbs(0.0f, 1e-6f));

    // The property the destruction sweep depends on: everyone is done at t=1.
    for (int i = 0; i < N; ++i)
        CHECK_THAT(arena_vfx::staggered_t(1.0f, i, N, SPAN), WithinAbs(1.0f, 1e-6f));

    // Earlier props are never behind later ones, and progress never goes back.
    for (int i = 0; i + 1 < N; ++i)
        CHECK(arena_vfx::staggered_t(0.6f, i, N, SPAN) >=
              arena_vfx::staggered_t(0.6f, i + 1, N, SPAN));
    for (float t = 0.0f; t < 1.0f; t += 0.05f)
        CHECK(arena_vfx::staggered_t(t + 0.05f, 7, N, SPAN) >=
              arena_vfx::staggered_t(t, 7, N, SPAN));
}

TEST_CASE("staggered_t degenerate inputs cannot divide by zero", "[arena_vfx][curve]") {
    CHECK_THAT(arena_vfx::staggered_t(0.5f, 0, 1, 0.55f), WithinAbs(0.5f, 1e-6f));
    CHECK_THAT(arena_vfx::staggered_t(0.5f, 0, 0, 0.55f), WithinAbs(0.5f, 1e-6f));
    // span >= 1 would make the duration zero; it is clamped to 0.95.
    CHECK(arena_vfx::staggered_t(1.0f, 3, 10, 5.0f) == 1.0f);
    CHECK(arena_vfx::staggered_t(0.0f, 3, 10, 5.0f) == 0.0f);
    // Index out of range is clamped rather than reading past the ends.
    CHECK(arena_vfx::staggered_t(0.5f, 99, 10, 0.55f) ==
          arena_vfx::staggered_t(0.5f, 9, 10, 0.55f));
}

// ---------------------------------------------------------------------------
// 2. The ordering rule
// ---------------------------------------------------------------------------

TEST_CASE("teardown strips every collider on the shift frame", "[arena_vfx][teardown]") {
    EntityManager em;
    ComponentStorage cs;
    auto props = make_arena(em, cs, SOLID_PROPS, WALL_PROPS);
    REQUIRE(cs.entities_with_component<Collider>().size() == SOLID_PROPS);

    auto dying = arena_vfx::teardown_props(cs, props, 90, 200, 255, SHIFT_SECONDS);

    // Gameplay-visible state is clean immediately...
    CHECK(cs.entities_with_component<Collider>().empty());
    // ...while every prop is still on screen, at full size.
    CHECK(dying.size() == props.size());
    for (Entity e : props) {
        REQUIRE(cs.get_component<Size>(e).has_value());
        CHECK(cs.get_component<Size>(e)->get().width > 0.0f);
    }
    // Only the solids emit debris; the wall ring just shrinks.
    CHECK(cs.entities_with_component<ParticleEmitter>().size() == SOLID_PROPS);
    // Every prop carries the window as a Lifetime, so a stalled shift still
    // retires them.
    CHECK(cs.entities_with_component<Lifetime>().size() == props.size());
}

TEST_CASE("props shrink to nothing and are all destroyed by the end of the window",
          "[arena_vfx][teardown]") {
    EntityManager em;
    ComponentStorage cs;
    auto props = make_arena(em, cs, SOLID_PROPS, WALL_PROPS);
    auto dying = arena_vfx::teardown_props(cs, props, 90, 200, 255, SHIFT_SECONDS);

    for (int frame = 1; frame <= 300; ++frame) {   // 5s at 60fps
        const float t = static_cast<float>(frame) / 60.0f / SHIFT_SECONDS;
        arena_vfx::animate(cs, dying, t, 0.55f, /*shrink=*/true);
        if (t < 1.0f) {
            // Mid-window: shrinking, but nothing has popped out of existence.
            CHECK(cs.entities_with_component<Size>().size() == props.size());
        }
    }
    // Last frame of the window: every prop is at scale 0 before it is swept.
    for (Entity e : props)
        CHECK_THAT(cs.get_component<Size>(e)->get().width, WithinAbs(0.0f, 1e-4f));

    arena_vfx::destroy_all(em, cs, dying);
    CHECK(dying.empty());
    CHECK(cs.entities_with_component<Size>().empty());
    CHECK(cs.entities_with_component<ParticleEmitter>().empty());
}

TEST_CASE("a growing prop's collider tracks its sprite", "[arena_vfx][birth]") {
    EntityManager em;
    ComponentStorage cs;
    auto props = make_arena(em, cs, 4, 0);
    auto growing = arena_vfx::capture_props(cs, props);
    REQUIRE(growing.size() == 4);

    arena_vfx::animate(cs, growing, 0.0f, 0.55f, /*shrink=*/false);
    CHECK_THAT(cs.get_component<Collider>(props[0])->get().width, WithinAbs(0.0f, 1e-5f));

    arena_vfx::animate(cs, growing, 0.3f, 0.55f, /*shrink=*/false);
    const float w = cs.get_component<Collider>(props[0])->get().width;
    CHECK(w > 0.0f);
    CHECK(w < 120.0f);
    // The collider is never wider than the sprite — no invisible wall.
    CHECK_THAT(w, WithinAbs(cs.get_component<Size>(props[0])->get().width, 1e-5f));

    // Ends at exactly the authored geometry, about the authored centre.
    arena_vfx::animate(cs, growing, 1.0f, 0.55f, /*shrink=*/false);
    for (Entity e : props) {
        CHECK_THAT(cs.get_component<Size>(e)->get().width, WithinAbs(120.0f, 1e-4f));
        CHECK_THAT(cs.get_component<Size>(e)->get().height, WithinAbs(80.0f, 1e-4f));
    }
    CHECK_THAT(cs.get_component<Position>(props[0])->get().y, WithinAbs(400.0f, 1e-4f));
}

TEST_CASE("animating a destroyed prop is a no-op, not a crash", "[arena_vfx][teardown]") {
    EntityManager em;
    ComponentStorage cs;
    auto props = make_arena(em, cs, 3, 0);
    auto dying = arena_vfx::teardown_props(cs, props, 90, 200, 255, SHIFT_SECONDS);
    // Lifetime can beat the animation to the entity; the ids go stale in place.
    cs.add_component<DestroyRequest>(props[1], DestroyRequest{});
    destroy_marked_entities(em, cs);
    arena_vfx::animate(cs, dying, 0.5f, 0.55f, true);
    arena_vfx::destroy_all(em, cs, dying);
    CHECK(cs.entities_with_component<Size>().empty());
}

// ---------------------------------------------------------------------------
// 3. The particle budget (ENGINE.md §5 — measure, never assume)
// ---------------------------------------------------------------------------

TEST_CASE("full-arena destruction stays inside the 2000-particle budget",
          "[arena_vfx][particles]") {
    EntityManager em;
    ComponentStorage cs;
    auto props = make_arena(em, cs, SOLID_PROPS, WALL_PROPS);
    auto dying = arena_vfx::teardown_props(cs, props, 90, 200, 255, SHIFT_SECONDS);

    ParticleSystem particles(DEFAULT_MAX_PARTICLES, 0xC0FFEEu);
    int peak = 0;
    for (int frame = 1; frame <= 300; ++frame) {
        const float t = static_cast<float>(frame) / 60.0f / SHIFT_SECONDS;
        arena_vfx::animate(cs, dying, t, 0.55f, true);
        particles.update(cs, em, 1.0f / 60.0f, /*emit=*/true);
        destroy_marked_entities(em, cs);
        peak = std::max(peak, live_particles(cs));
    }
    WARN("peak live particles from a full-arena destruction: " << peak);

    // Measured 336 for the worst arena (28 solid props x 14/s x 0.8s). The
    // headroom matters: the shift shockwave (~250) and the wave-20 mass-death
    // case share the same 2000, so this must stay small. Tighten the number, not
    // the cap — DEFAULT_MAX_PARTICLES is an engine change owned by Phase 10.
    CHECK(peak > 200);                          // the effect actually fired
    CHECK(peak < 500);                          // and did not run away
    CHECK(peak < DEFAULT_MAX_PARTICLES / 4);    // room for everything else
}
