/**
 * test_scar_system.cpp — the battle-scar layer (engine suite, Lane V, D145).
 *
 * The scar layer is a texture-target renderer, so most of it is only meaningful
 * against a live SDL_Renderer — which a unit test has no business creating (the
 * suite must run on a machine with no display, and a renderer per test case is
 * slow enough to matter). What IS pinned here is the part that has to be right on
 * a machine where texture creation FAILS: a cosmetic layer must degrade to
 * drawing nothing, never take a run down with it.
 *
 * The visual behaviour is verified headlessly instead — `--suite` runs the full
 * accumulate-and-draw path under SDL_VIDEODRIVER=dummy every gate.
 */
#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/fx_events.hpp"
#include "engine/ecs/systems/scar_system.hpp"

TEST_CASE("a scar layer with no renderer is inert, not a crash", "[Game][scars]") {
    ScarSystem scars;
    CHECK_FALSE(scars.ready());

    // configure() refuses a null renderer and a degenerate size...
    CHECK_FALSE(scars.configure(nullptr, 640, 640, 0.0f, 0.0f));
    CHECK_FALSE(scars.ready());

    // ...and every other entry point tolerates that state.
    scars.set_tuning(16, 0.55f);
    scars.clear(nullptr);

    std::vector<fx_events::Stamp> stamps{{100.0f, 100.0f, 0, 1.0f, 0.0f}};
    Blackboard bb;
    scars.update_and_render(nullptr, bb, stamps);
    CHECK(scars.stamps_applied() == 0);
}

TEST_CASE("the stamp budget is what bounds a mass-death frame", "[Game][scars]") {
    // Two independent caps stand between "a wave died at once" and the blit
    // count: fx_events drops past MAX_PER_FRAME at the publisher, and the scar
    // layer applies at most max_stamps_per_frame of what survives. This pins the
    // publisher half, which is testable without a renderer.
    Blackboard bb;
    fx_events::clear_frame(bb);
    for (int i = 0; i < 500; ++i)
        fx_events::push_stamp(bb, static_cast<float>(i), 0.0f, 0);

    const auto stamps =
        bb.get<std::vector<fx_events::Stamp>>(fx_events::SCAR_STAMPS);
    CHECK(stamps.size() == fx_events::MAX_PER_FRAME);
    // Oldest-wins: the first stamps of the frame are the ones kept, so a capped
    // frame still marks the floor where the first bodies fell.
    CHECK(stamps.front().x == 0.0f);
}
