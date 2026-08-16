/**
 * test_hangar_screen.cpp — contract test for the hangar (the expanded
 * run_setup) and the end-of-run flight report (gameplay pack v2.3 tier 6,
 * D221). Same shape as test_intermission_screen: load the REAL shipped
 * GameData.json and pin the seams main.cpp compares as string literals —
 * widget names the refresh lambdas resolve, callback names the click router
 * matches, and the geometry promises from the owner's spec (aligned pips,
 * big green LAUNCH bottom-left).
 */
#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/components.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/gamedata_loader.hpp"
#include "engine/project_paths.hpp"

namespace {

struct LoadedWorld {
    EntityManager em;
    ComponentStorage cs;
    Blackboard bb;
    LoadedWorld() {
        load_game_data(project_paths::assets_dir() + "/GameData.json", em, cs, bb);
    }
    Entity by_name(const std::string& name) const {
        const double v = bb.get_or<double>("ui.widget_id." + name, -1.0);
        return v < 0.0 ? 0 : static_cast<Entity>(v);
    }
    const UIElement& el(Entity e) const {
        return cs.get_component<UIElement>(e)->get();
    }
};

}  // namespace

TEST_CASE("the hangar carries every widget main.cpp rewrites or routes", "[Game][hangar][ui]") {
    LoadedWorld w;
    struct Want { const char* name; const char* fn; };
    for (const Want want : {Want{"menu_ship", "on_ship_cycle"},
                            Want{"menu_weapon", "on_weapon_cycle"},
                            Want{"menu_buy", "on_buy_ship"},
                            Want{"menu_normal", "on_pick_normal"},
                            Want{"menu_hard", "on_pick_hard"},
                            Want{"menu_launch", "on_launch_click"},
                            Want{"menu_back", "on_back_click"}}) {
        INFO("widget: " << want.name);
        Entity e = w.by_name(want.name);
        REQUIRE(e != 0);
        CHECK(w.el(e).on_click_fn == want.fn);
    }
    for (const char* label : {"hangar_scrap", "hangar_hint"}) {
        INFO("label: " << label);
        CHECK(w.by_name(label) != 0);
    }
}

TEST_CASE("the 5-bubble pip column is one aligned x for all 8 rows", "[Game][hangar][ui]") {
    LoadedWorld w;
    float pip_x = -1.0f, name_x = -1.0f, prev_y = 1e9f;
    for (int i = 0; i < 8; ++i) {
        Entity n = w.by_name("hs_name_" + std::to_string(i));
        Entity p = w.by_name("hs_pips_" + std::to_string(i));
        INFO("row " << i);
        REQUIRE(n != 0);
        REQUIRE(p != 0);
        if (pip_x < 0.0f) { pip_x = w.el(p).rect.x; name_x = w.el(n).rect.x; }
        // The owner's ask, verbatim: "Make sure that the bubbles are all aligned."
        CHECK(w.el(p).rect.x == pip_x);
        CHECK(w.el(n).rect.x == name_x);
        // Rows on one shared row line, descending.
        CHECK(w.el(n).rect.y == w.el(p).rect.y);
        CHECK(w.el(n).rect.y < prev_y);
        prev_y = w.el(n).rect.y;
    }
}

TEST_CASE("LAUNCH is the big green button in the bottom-left", "[Game][hangar][ui]") {
    LoadedWorld w;
    Entity launch = w.by_name("menu_launch");
    REQUIRE(launch != 0);
    const UIElement& el = w.el(launch);
    CHECK(el.style_id == "launch_green");
    // Bottom-left of the 800x600 bottom-up design canvas.
    CHECK(el.rect.x < 200.0f);
    CHECK(el.rect.y < 200.0f);
    // Bigger than every other hangar button.
    for (const char* other : {"menu_ship", "menu_weapon", "menu_buy", "menu_back"}) {
        INFO("vs " << other);
        const UIElement& o = w.el(w.by_name(other));
        CHECK(el.rect.w * el.rect.h > o.rect.w * o.rect.h);
    }
}

TEST_CASE("the flight report exists, boots inactive, and its button routes", "[Game][hangar][ui]") {
    LoadedWorld w;
    // Inactive at boot: the run opens on the title screen, and an active
    // flight report would sit over it from frame zero.
    bool found = false;
    for (Entity e : w.cs.entities_with_component<UIScreen>()) {
        auto sc = w.cs.get_component<UIScreen>(e);
        if (!sc.has_value() || sc->get().screen_name != "run_stats") continue;
        found = true;
        REQUIRE_FALSE(sc->get().active);
    }
    REQUIRE(found);
    REQUIRE(w.by_name("rs_title") != 0);
    for (int i = 0; i < 5; ++i) {
        INFO("line " << i);
        CHECK(w.by_name("rs_line_" + std::to_string(i)) != 0);
    }
    Entity cont = w.by_name("rs_continue");
    REQUIRE(cont != 0);
    CHECK(w.el(cont).on_click_fn == "on_run_stats_continue");
}
