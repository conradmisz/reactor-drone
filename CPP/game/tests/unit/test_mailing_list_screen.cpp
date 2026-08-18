/**
 * test_mailing_list_screen.cpp — D238 contract test for the standalone
 * mailing-list signup. Same shape as test_cosmetics' routing test: load the
 * REAL shipped GameData.json and pin the seams main.cpp matches as string
 * literals, so a rename in the data can never silently orphan the screen.
 */
#include <catch2/catch_test_macros.hpp>

#include <string>

#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/components.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/gamedata_loader.hpp"
#include "engine/project_paths.hpp"

namespace {
struct World {
    EntityManager em;
    ComponentStorage cs;
    Blackboard bb;
    World() { load_game_data(project_paths::assets_dir() + "/GameData.json", em, cs, bb); }
    Entity by_name(const std::string& n) const {
        const double v = bb.get_or<double>("ui.widget_id." + n, -1.0);
        return v < 0.0 ? 0 : static_cast<Entity>(v);
    }
    const UIElement& el(Entity e) const { return cs.get_component<UIElement>(e)->get(); }
};
}  // namespace

TEST_CASE("the mailing-list screen is authored and routed", "[Game][ui][mail]") {
    World w;

    // The door: a main-menu button, next to LEADERBOARD.
    const Entity door = w.by_name("menu_mail");
    REQUIRE(door != 0);
    CHECK(w.el(door).on_click_fn == "on_mail_click");
    CHECK(w.el(door).label_text == "MAILING LIST");

    // The screen exists, starts INACTIVE (it must not cover the title at boot).
    bool found = false;
    for (Entity e : w.cs.entities_with_component<UIScreen>()) {
        auto sc = w.cs.get_component<UIScreen>(e);
        if (!sc.has_value() || sc->get().screen_name != "mailing_list") continue;
        found = true;
        CHECK_FALSE(sc->get().active);
    }
    REQUIRE(found);

    // The three seams main.cpp matches by string.
    const Entity box = w.by_name("mail_box");
    const Entity submit = w.by_name("mail_submit");
    const Entity back = w.by_name("mail_back");
    REQUIRE(box != 0);
    REQUIRE(submit != 0);
    REQUIRE(back != 0);
    CHECK(w.el(box).on_click_fn == "on_mail_focus");
    CHECK(w.el(submit).on_click_fn == "on_mail_submit");
    CHECK(w.el(back).on_click_fn == "on_mail_back");

    // The two labels main.cpp rewrites every frame.
    REQUIRE(w.by_name("mail_field") != 0);
    REQUIRE(w.by_name("mail_msg") != 0);

    // The typed address must be visible: the field label sits inside the box.
    const UIRect& b = w.el(box).rect;
    const UIRect& f = w.el(w.by_name("mail_field")).rect;
    CHECK(f.x >= b.x);
    CHECK(f.x + f.w <= b.x + b.w);
}
