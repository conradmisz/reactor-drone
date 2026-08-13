/**
 * test_flight_report.cpp — the run-as-artifact recorder (engine suite, Lane S, D143).
 *
 * The recorder is passive, so what matters is that it records the right things,
 * that its buffers are genuinely bounded (this runs for a whole 30-wave run), and
 * that a report never bleeds onto a screen it does not belong on — including the
 * next run's.
 */
#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/destruction.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/fx_events.hpp"
#include "game/flight_report.hpp"
#include "game/player_components.hpp"

namespace {

constexpr int kPlaying = 1, kGameOver = 2, kTitle = 0;

FlightReportConfig live_config() {
    FlightReportConfig c;
    c.enabled = true;
    c.sample_every_n = 1;      // every frame, so the tests are short
    c.max_samples = 64;        // small ring, so wrap-around is reachable
    c.x = 250.0f; c.y = 150.0f; c.size = 300.0f;
    return c;
}

ArenaConfig arena() {
    ArenaConfig a;
    a.center_x = 480.0f; a.center_y = 330.0f; a.radius = 320.0f;
    return a;
}

Entity make_player(EntityManager& em, ComponentStorage& cs) {
    Entity p = em.create_entity();
    cs.add_component<PlayerTag>(p, PlayerTag{});
    cs.add_component<Position>(p, Position{480.0f, 330.0f});
    cs.add_component<Size>(p, Size{40.0f, 40.0f});
    cs.add_component<Health>(p, Health{100.0f, 100.0f});
    return p;
}

/// Every live (non-parked) mark rect currently in the pool.
std::vector<UIRect> live_marks(ComponentStorage& cs) {
    std::vector<UIRect> out;
    for (Entity e : cs.entities_with_component<UIElement>()) {
        const UIElement& el = cs.get_component<UIElement>(e)->get();
        if (el.rect.w > 0.0f && el.rect.h > 0.0f) out.push_back(el.rect);
    }
    return out;
}

}  // namespace

TEST_CASE("a disabled report records nothing and creates no widgets",
          "[Game][flight_report]") {
    EntityManager em; ComponentStorage cs; Blackboard bb;
    make_player(em, cs);
    FlightReport fr;
    FlightReportConfig cfg = live_config();
    cfg.enabled = false;
    fr.set_config(cfg, arena());

    bb.set<int>("phase", kPlaying);
    for (int i = 0; i < 100; ++i) fr.update(cs, em, bb);
    CHECK(fr.path_samples() == 0);
    CHECK(fr.pool_size() == 0);
    CHECK(cs.entities_with_component<UIElement>().empty());
}

TEST_CASE("the path is sampled while playing, at the configured cadence",
          "[Game][flight_report]") {
    EntityManager em; ComponentStorage cs; Blackboard bb;
    make_player(em, cs);
    FlightReport fr;
    FlightReportConfig cfg = live_config();
    cfg.sample_every_n = 5;
    fr.set_config(cfg, arena());
    bb.set<int>("phase", kPlaying);

    for (int i = 0; i < 50; ++i) fr.update(cs, em, bb);
    CHECK(fr.path_samples() == 10);
}

TEST_CASE("the ring buffers are bounded and keep the RECENT history",
          "[Game][flight_report]") {
    EntityManager em; ComponentStorage cs; Blackboard bb;
    Entity p = make_player(em, cs);
    FlightReport fr;
    fr.set_config(live_config(), arena());   // max_samples 64
    bb.set<int>("phase", kPlaying);

    // 500 frames of flying: five times past the cap.
    for (int i = 0; i < 500; ++i) {
        cs.get_component<Position>(p)->get().x = 300.0f + static_cast<float>(i % 200);
        fr.update(cs, em, bb);
    }
    CHECK(fr.path_samples() == 64);

    // The report draws the LAST 64 positions, so the most recent frame's position
    // must be somewhere in the buffer. Drawn on the game-over screen, at least one
    // mark lands inside the report square.
    bb.set<int>("phase", kGameOver);
    fr.update(cs, em, bb);
    const auto marks = live_marks(cs);
    REQUIRE(!marks.empty());
    for (const UIRect& r : marks) {
        CHECK(r.x >= 250.0f - 1.0f);
        CHECK(r.y >= 150.0f - 1.0f);
        CHECK(r.x + r.w <= 250.0f + 300.0f + 1.0f);
        CHECK(r.y + r.h <= 150.0f + 300.0f + 1.0f);
    }
}

TEST_CASE("hits taken are recorded from the hull dropping",
          "[Game][flight_report]") {
    EntityManager em; ComponentStorage cs; Blackboard bb;
    Entity p = make_player(em, cs);
    FlightReport fr;
    fr.set_config(live_config(), arena());
    bb.set<int>("phase", kPlaying);

    fr.update(cs, em, bb);                    // establish the baseline hull
    CHECK(fr.hit_samples() == 0);

    auto& hp = cs.get_component<Health>(p)->get();
    hp.current = 80.0f;   fr.update(cs, em, bb);
    hp.current = 60.0f;   fr.update(cs, em, bb);
    CHECK(fr.hit_samples() == 2);

    // A heal is not a hit, and a steady hull is not a hit.
    hp.current = 100.0f;  fr.update(cs, em, bb);
    for (int i = 0; i < 10; ++i) fr.update(cs, em, bb);
    CHECK(fr.hit_samples() == 2);
}

TEST_CASE("kills come off the shared kill-mark list", "[Game][flight_report]") {
    EntityManager em; ComponentStorage cs; Blackboard bb;
    make_player(em, cs);
    FlightReport fr;
    fr.set_config(live_config(), arena());
    bb.set<int>("phase", kPlaying);

    fx_events::clear_frame(bb);
    fx_events::push_mark(bb, 400.0f, 300.0f, 0);
    fx_events::push_mark(bb, 500.0f, 350.0f, 0);
    fr.update(cs, em, bb);
    CHECK(fr.kill_samples() == 2);

    // A frame with no deaths adds nothing — the list is cleared per frame by
    // main.cpp, which is the contract this consumer relies on.
    fx_events::clear_frame(bb);
    fr.update(cs, em, bb);
    CHECK(fr.kill_samples() == 2);
}

TEST_CASE("the report is drawn ONLY on the terminal screens",
          "[Game][flight_report]") {
    EntityManager em; ComponentStorage cs; Blackboard bb;
    make_player(em, cs);
    FlightReport fr;
    fr.set_config(live_config(), arena());

    bb.set<int>("phase", kPlaying);
    for (int i = 0; i < 30; ++i) fr.update(cs, em, bb);
    CHECK(live_marks(cs).empty());            // not while flying

    bb.set<int>("phase", kGameOver);
    fr.update(cs, em, bb);
    CHECK(!live_marks(cs).empty());

    bb.set<int>("phase", kTitle);
    fr.update(cs, em, bb);
    CHECK(live_marks(cs).empty());            // and not on the main menu
}

TEST_CASE("reset drops the run but keeps the widget pool",
          "[Game][flight_report]") {
    EntityManager em; ComponentStorage cs; Blackboard bb;
    make_player(em, cs);
    FlightReport fr;
    fr.set_config(live_config(), arena());
    bb.set<int>("phase", kPlaying);
    for (int i = 0; i < 30; ++i) fr.update(cs, em, bb);
    bb.set<int>("phase", kGameOver);
    fr.update(cs, em, bb);
    const std::size_t pool = fr.pool_size();
    REQUIRE(pool > 0);

    fr.reset();
    CHECK(fr.path_samples() == 0);
    CHECK(fr.kill_samples() == 0);
    CHECK(fr.hit_samples() == 0);
    // The pool is re-pointed, never re-created: re-creating it every run would
    // churn entity ids, which is the ghost-component trap this codebase has been
    // bitten by before (D58).
    CHECK(fr.pool_size() == pool);
    fr.update(cs, em, bb);
    CHECK(fr.pool_size() == pool);
}
