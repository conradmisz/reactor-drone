/**
 * test_ui_text_fit.cpp — Lane H (D85-D87): text can never leave its widget.
 *
 * `fit_text_in_rect` is the one place a string is measured against its box, so
 * it is the one place worth pinning. It takes MEASURED metrics rather than a
 * font, which is what lets this whole file run with no window, no SDL_ttf and no
 * renderer — the property that mattered when the bug was "four labels overflow"
 * and the fix had to hold for every future label too.
 *
 * The invariant, asserted for every case below: the returned box is inside the
 * rect. Not "usually", not "when the caller authored it well".
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <limits>

#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/systems/ui_render_math.hpp"
#include "engine/project_paths.hpp"
#include "game/arena_config.hpp"
#include "game/enemy_components.hpp"
#include "game/game_hud_system.hpp"
#include "game/minimap_system.hpp"
#include "game/player_components.hpp"

using Catch::Matchers::WithinAbs;

namespace {
/// The contract, as an assertion: the fitted box never crosses the rect.
void require_inside(const TextFit& f, const UIRect& r) {
    REQUIRE(f.x >= r.x - 0.001f);
    REQUIRE(f.y >= r.y - 0.001f);
    REQUIRE(f.x + f.w <= r.x + r.w + 0.001f);
    REQUIRE(f.y + f.h <= r.y + r.h + 0.001f);
}
}  // namespace

TEST_CASE("text that already fits is not moved or resized", "[ui][textfit]") {
    const UIRect r{100.0f, 200.0f, 300.0f, 40.0f};
    const TextFit f = fit_text_in_rect(r, 120.0f, 28.0f, TextAlign::Left);
    REQUIRE(f.visible);
    // Never scaled UP: a short caption keeps its authored size, so every label
    // that was already correct renders exactly as it did before the fix.
    REQUIRE_THAT(f.w, WithinAbs(120.0f, 0.001f));
    REQUIRE_THAT(f.h, WithinAbs(28.0f, 0.001f));
    REQUIRE_THAT(f.x, WithinAbs(100.0f, 0.001f));   // left-aligned
    REQUIRE_THAT(f.y, WithinAbs(206.0f, 0.001f));   // vertically centered
    require_inside(f, r);
}

TEST_CASE("centered alignment centers only when it fits", "[ui][textfit]") {
    const UIRect r{0.0f, 0.0f, 200.0f, 50.0f};
    const TextFit fits = fit_text_in_rect(r, 100.0f, 30.0f, TextAlign::Center);
    REQUIRE_THAT(fits.x, WithinAbs(50.0f, 0.001f));
    require_inside(fits, r);

    // The button case from the report ("Shield Capacitor +30   90 cr"): the text
    // is wider than the button, so it shrinks and ends up flush, not spilling.
    const TextFit wide = fit_text_in_rect(r, 400.0f, 30.0f, TextAlign::Center);
    REQUIRE(wide.visible);
    REQUIRE_THAT(wide.w, WithinAbs(200.0f, 0.001f));
    REQUIRE_THAT(wide.x, WithinAbs(0.0f, 0.001f));
    require_inside(wide, r);
}

TEST_CASE("overflow shrinks uniformly on the binding axis", "[ui][textfit]") {
    const UIRect r{10.0f, 10.0f, 100.0f, 100.0f};

    // Width binds: scale 0.25, so the height comes down with it (aspect kept).
    const TextFit w = fit_text_in_rect(r, 400.0f, 40.0f, TextAlign::Left);
    REQUIRE_THAT(w.w, WithinAbs(100.0f, 0.001f));
    REQUIRE_THAT(w.h, WithinAbs(10.0f, 0.001f));
    require_inside(w, r);

    // Height binds — this is the "HULL clipped off the top of the screen" case:
    // a 16px-tall rect holding a 29px line box.
    const TextFit h = fit_text_in_rect(UIRect{0.0f, 0.0f, 500.0f, 16.0f},
                                       60.0f, 29.0f, TextAlign::Left);
    REQUIRE_THAT(h.h, WithinAbs(16.0f, 0.001f));
    require_inside(h, UIRect{0.0f, 0.0f, 500.0f, 16.0f});
}

TEST_CASE("padding is honoured and never inverts the box", "[ui][textfit]") {
    const UIRect r{0.0f, 0.0f, 100.0f, 60.0f};
    const TextFit f = fit_text_in_rect(r, 200.0f, 20.0f, TextAlign::Left, 10.0f);
    REQUIRE_THAT(f.x, WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(f.w, WithinAbs(80.0f, 0.001f));
    require_inside(f, r);

    // Padding wider than the rect is not a crash and not a negative box — it is
    // simply nothing to draw.
    REQUIRE_FALSE(fit_text_in_rect(r, 50.0f, 10.0f, TextAlign::Left, 80.0f).visible);
}

TEST_CASE("degenerate inputs draw nothing", "[ui][textfit]") {
    const UIRect r{5.0f, 5.0f, 100.0f, 40.0f};
    REQUIRE_FALSE(fit_text_in_rect(r, 0.0f, 20.0f, TextAlign::Left).visible);
    REQUIRE_FALSE(fit_text_in_rect(r, 20.0f, 0.0f, TextAlign::Left).visible);
    // A collapsed rect is how this codebase HIDES a widget (D61, D58): the
    // hidden state must not become "text drawn at the collapse point".
    REQUIRE_FALSE(fit_text_in_rect(UIRect{5.0f, 5.0f, 0.0f, 0.0f}, 20.0f, 20.0f,
                                   TextAlign::Center).visible);
    const float nan = std::numeric_limits<float>::quiet_NaN();
    REQUIRE_FALSE(fit_text_in_rect(r, nan, 20.0f, TextAlign::Left).visible);
    REQUIRE_FALSE(fit_text_in_rect(r, 20.0f, nan, TextAlign::Left).visible);
}

TEST_CASE("the invariant holds across a sweep of sizes", "[ui][textfit]") {
    const UIRect r{-30.0f, 12.5f, 137.0f, 41.0f};
    for (float tw = 1.0f; tw < 600.0f; tw += 7.0f) {
        for (float th = 1.0f; th < 120.0f; th += 5.0f) {
            require_inside(fit_text_in_rect(r, tw, th, TextAlign::Left, 4.0f), r);
            require_inside(fit_text_in_rect(r, tw, th, TextAlign::Center), r);
        }
    }
}

TEST_CASE("the HUD is arena furniture, not title-screen furniture", "[ui][hud]") {
    // The screen stack cannot answer this: "gameplay" is its base sentinel and is
    // on the stack in every phase. Values mirror Phase in main.cpp.
    REQUIRE(hud_visible_in_phase(1));        // PHASE_PLAYING
    REQUIRE(hud_visible_in_phase(5));        // PHASE_INTERMISSION — the drone flies
    REQUIRE_FALSE(hud_visible_in_phase(0));  // PHASE_TITLE
    REQUIRE_FALSE(hud_visible_in_phase(2));  // PHASE_GAMEOVER
    REQUIRE_FALSE(hud_visible_in_phase(3));  // PHASE_VICTORY
    REQUIRE_FALSE(hud_visible_in_phase(4));  // PHASE_SHOP — the panel sits over it
}

// ---------------------------------------------------------------------------
// The system half: the radar obeys the same visibility rule, and the shipped
// data file actually authors the smaller map.
// ---------------------------------------------------------------------------
TEST_CASE("the radar parks every blip outside a playing phase", "[ui][hud][minimap]") {
    ComponentStorage cs;
    EntityManager em;
    Blackboard bb;

    ArenaConfig arena;
    arena.center_x = 480.0f; arena.center_y = 330.0f; arena.radius = 320.0f;
    MinimapConfig cfg;
    cfg.enabled = true; cfg.x = 688.0f; cfg.y = 486.0f; cfg.size = 96.0f;
    cfg.max_blips = 16;

    MinimapSystem mm;
    mm.set_config(cfg, arena);

    for (int i = 0; i < 6; ++i) {
        Entity e = em.create_entity();
        cs.add_component<Position>(e, Position{480.0f + static_cast<float>(i), 330.0f});
        cs.add_component<Size>(e, Size{20.0f, 20.0f});
        cs.add_component<EnemyTag>(e, EnemyTag{});
    }

    auto drawn = [&]() {
        int n = 0;
        for (Entity e : cs.entities_with_component<UIElement>()) {
            const auto& el = cs.get_component<UIElement>(e)->get();
            if (el.rect.w > 0.0f && el.rect.h > 0.0f) ++n;
        }
        return n;
    };

    bb.set<int>("phase", 1);
    mm.update(cs, em, bb);
    CHECK(drawn() == 6);

    // Title, game over, victory and the shop: nothing on screen. The pool is not
    // torn down — it is allocated once per process (D58) — only parked.
    for (int phase : {0, 2, 3, 4}) {
        bb.set<int>("phase", phase);
        mm.update(cs, em, bb);
        CHECK(drawn() == 0);
        CHECK(mm.pool_size() == 16);
    }

    bb.set<int>("phase", 5);   // the intermission keeps it
    mm.update(cs, em, bb);
    CHECK(drawn() == 6);
}

TEST_CASE("the shipped radar is small and clear of the HUD column", "[ui][minimap]") {
    GameConfig cfg = load_arena_config(project_paths::assets_dir() + "/GameData.json");
    // #1: "make smaller, currently too distracting". 140 was a fifth of the
    // canvas width; this pins the reduction so it cannot creep back.
    CHECK(cfg.minimap.size <= 100.0f);
    // Lane M (#4, D115): the x bound moved off the design canvas and onto the
    // WINDOW, because the canvas is letterboxed 50px in from each side and the
    // player judges the gap against the window edge. Margin equality is pinned in
    // test_minimap.cpp; this only checks the map is still fully on screen.
    CHECK(cfg.minimap.x + cfg.minimap.size <= 845.0f);   // (50 + 845*1.1) < 980
    CHECK(cfg.minimap.y + cfg.minimap.size <= 600.0f);
    // Top-right, and never over the top-left gauge column (x < 260).
    CHECK(cfg.minimap.x > 400.0f);
}
