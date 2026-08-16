/**
 * test_timescale.cpp — Temporal Overload (engine suite, Lane P, D139).
 *
 * The three things that would silently break the game if they were wrong:
 *   - disabled means EXACTLY 1.0f, not approximately (the canary depends on it);
 *   - a kill chain dilates and then recovers, and one chain fires one beat;
 *   - the scale never reaches zero, or a frame's seconds would be zero and every
 *     seconds-based timer in the sim would stop for good.
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "game/timescale_system.hpp"

using Catch::Matchers::WithinAbs;

namespace {

constexpr float DT = 1.0f / 60.0f;

TimescaleConfig live_config() {
    TimescaleConfig c;
    c.enabled = true;
    c.chain_kills = 3;
    c.chain_window = 1.2f;
    c.kill_hold = 0.22f;
    c.kill_scale = 0.45f;
    c.hull_frac = 0.15f;
    c.hull_scale = 0.6f;
    c.ease_per_sec = 6.0f;
    c.min_scale = 0.35f;
    return c;
}

/// A player with a hull fraction, for the hull-critical branch.
Entity make_player(EntityManager& em, ComponentStorage& cs, float frac) {
    Entity p = em.create_entity();
    cs.add_component<PlayerTag>(p, PlayerTag{});
    cs.add_component<Health>(p, Health{100.0f * frac, 100.0f});
    return p;
}

/// Run `frames` frames with no new kills, returning the final scale.
float coast(ComponentStorage& cs, Blackboard& bb, const TimescaleConfig& cfg,
            TimescaleState& st, int frames) {
    float scale = 1.0f;
    for (int i = 0; i < frames; ++i)
        scale = tick_timescale(cs, bb, cfg, st, DT, /*active=*/true);
    return scale;
}

}  // namespace

TEST_CASE("a disabled timescale is exactly 1.0f", "[Game][timescale]") {
    EntityManager em; ComponentStorage cs; Blackboard bb;
    make_player(em, cs, 0.05f);            // hull critical — would dilate if live
    bb.set<int>(TIMESCALE_KILL_KEY, 99);   // a chain, too

    TimescaleConfig cfg;                   // enabled defaults to false
    TimescaleState st;
    for (int i = 0; i < 30; ++i) {
        const float s = tick_timescale(cs, bb, cfg, st, DT, /*active=*/true);
        REQUIRE(s == 1.0f);                // bit-exact, not WithinAbs
    }
}

TEST_CASE("an inactive phase never dilates, whatever the sim says",
          "[Game][timescale]") {
    EntityManager em; ComponentStorage cs; Blackboard bb;
    make_player(em, cs, 0.02f);
    TimescaleConfig cfg = live_config();
    TimescaleState st;
    for (int i = 0; i < 30; ++i)
        REQUIRE(tick_timescale(cs, bb, cfg, st, DT, /*active=*/false) == 1.0f);
}

TEST_CASE("a kill chain dilates time and then recovers", "[Game][timescale]") {
    EntityManager em; ComponentStorage cs; Blackboard bb;
    make_player(em, cs, 1.0f);             // full hull: the chain is the only cause
    TimescaleConfig cfg = live_config();
    TimescaleState st;
    bb.set<int>(TIMESCALE_KILL_KEY, 0);

    CHECK(coast(cs, bb, cfg, st, 5) == 1.0f);

    // Three kills inside the window trip the beat.
    bb.set<int>(TIMESCALE_KILL_KEY, 3);
    const float first = tick_timescale(cs, bb, cfg, st, DT, true);
    CHECK(first < 1.0f);
    const float dilated = coast(cs, bb, cfg, st, 10);
    CHECK(dilated < 0.9f);
    CHECK(dilated >= cfg.kill_scale);      // eased toward it, never past it

    // The hold expires and the scale climbs back to exactly 1.0f (the snap in
    // tick_timescale is what makes "back to normal" bit-exact rather than 0.999).
    CHECK(coast(cs, bb, cfg, st, 240) == 1.0f);
}

TEST_CASE("two kills alone are not a chain", "[Game][timescale]") {
    EntityManager em; ComponentStorage cs; Blackboard bb;
    make_player(em, cs, 1.0f);
    TimescaleConfig cfg = live_config();
    TimescaleState st;
    bb.set<int>(TIMESCALE_KILL_KEY, 2);
    CHECK(coast(cs, bb, cfg, st, 20) == 1.0f);
}

TEST_CASE("kills spread past the chain window do not stack", "[Game][timescale]") {
    EntityManager em; ComponentStorage cs; Blackboard bb;
    make_player(em, cs, 1.0f);
    TimescaleConfig cfg = live_config();
    cfg.chain_window = 0.2f;               // 12 frames
    TimescaleState st;

    int kills = 0;
    for (int i = 0; i < 3; ++i) {
        bb.set<int>(TIMESCALE_KILL_KEY, ++kills);
        tick_timescale(cs, bb, cfg, st, DT, true);
        coast(cs, bb, cfg, st, 20);        // window closes between kills
    }
    CHECK(coast(cs, bb, cfg, st, 5) == 1.0f);
}

TEST_CASE("critical hull dilates for as long as it lasts", "[Game][timescale]") {
    EntityManager em; ComponentStorage cs; Blackboard bb;
    Entity p = make_player(em, cs, 0.10f);   // below hull_frac 0.15
    TimescaleConfig cfg = live_config();
    TimescaleState st;

    const float low = coast(cs, bb, cfg, st, 60);
    CHECK(low < 0.95f);
    CHECK(low >= cfg.hull_scale);

    // Healed above the threshold, it recovers — a standing condition, not a beat.
    cs.get_component<Health>(p)->get().current = 80.0f;
    CHECK(coast(cs, bb, cfg, st, 240) == 1.0f);
}

TEST_CASE("a dead player does not dilate the game-over screen", "[Game][timescale]") {
    EntityManager em; ComponentStorage cs; Blackboard bb;
    make_player(em, cs, 0.0f);               // current == 0: dead, not critical
    TimescaleConfig cfg = live_config();
    TimescaleState st;
    CHECK(coast(cs, bb, cfg, st, 30) == 1.0f);
}

TEST_CASE("the scale never falls below min_scale", "[Game][timescale]") {
    EntityManager em; ComponentStorage cs; Blackboard bb;
    make_player(em, cs, 0.01f);
    TimescaleConfig cfg = live_config();
    cfg.kill_scale = 0.0f;                   // ask for a full stop...
    cfg.hull_scale = 0.0f;
    TimescaleState st;
    bb.set<int>(TIMESCALE_KILL_KEY, 10);

    for (int i = 0; i < 600; ++i) {
        const float s = tick_timescale(cs, bb, cfg, st, DT, true);
        REQUIRE(s >= cfg.min_scale - 1e-4f);  // ...and never get it
        REQUIRE(s > 0.0f);
    }
}

TEST_CASE("a restart rewinding the kill counter is not a negative chain",
          "[Game][timescale]") {
    EntityManager em; ComponentStorage cs; Blackboard bb;
    make_player(em, cs, 1.0f);
    TimescaleConfig cfg = live_config();
    TimescaleState st;

    bb.set<int>(TIMESCALE_KILL_KEY, 40);
    coast(cs, bb, cfg, st, 300);
    bb.set<int>(TIMESCALE_KILL_KEY, 0);       // start_run resets it
    CHECK(coast(cs, bb, cfg, st, 30) == 1.0f);
    // ...and the first kills of the new run still chain normally.
    bb.set<int>(TIMESCALE_KILL_KEY, 3);
    CHECK(tick_timescale(cs, bb, cfg, st, DT, true) < 1.0f);
}
