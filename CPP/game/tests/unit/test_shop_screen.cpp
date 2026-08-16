/**
 * test_shop_screen.cpp — contract test for the clickable shop menu (Lane C, D61).
 *
 * Mirrors test_intermission_screen.cpp, and exists for the same reason: the
 * `shop` screen is authored entirely in assets/GameData.json, but ShopSystem
 * reacts to it by comparing the clicked widget's `on_click_fn` against string
 * literals and by resolving widgets through "ui.widget_id.<name>". Both seams
 * are invisible to the compiler — rename a widget in the JSON and you get a
 * shop that builds, opens, and then ignores every click forever.
 *
 * So these load the REAL shipped GameData.json and pin:
 *   - the screen exists and boots inactive;
 *   - all eight cards, three tabs and the LAUNCH button carry exactly the
 *     callback names shop_system.cpp compares to;
 *   - every widget ShopSystem::menu_build looks up by name is published;
 *   - every style_id resolves in the shipped ui_styles table;
 *   - cards are on-canvas, comfortably clickable, and do not overlap.
 *
 * Keep the kExpected* constants below in sync with shop_system.cpp.
 */

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <vector>

#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/components.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/systems/ui_render_math.hpp"
#include "engine/gamedata_loader.hpp"
#include "engine/project_paths.hpp"
#include "engine/ui_style.hpp"
#include "game/shop_system.hpp"

namespace {

constexpr const char* kScreenName = "shop";
constexpr int kCards = 8;

// Every name shop_system.cpp resolves via "ui.widget_id.<name>".
const std::vector<std::string> kNamedWidgets = {
    "shop_panel", "shop_title", "shop_credits",
    // Tier 8 (D221/D225): only the UPGRADES tab survives.
    "shop_tab_0",
    "shop_card_0", "shop_card_1", "shop_card_2", "shop_card_3",
    "shop_card_4", "shop_card_5", "shop_card_6", "shop_card_7",
    "shop_leave", "shop_preview_label",
    "shop_tip_panel", "shop_tip_name", "shop_tip_desc",
};

struct LoadedWorld {
    EntityManager em;
    ComponentStorage cs;
    Blackboard bb;

    LoadedWorld() {
        load_game_data(project_paths::assets_dir() + "/GameData.json", em, cs, bb);
    }

    std::vector<Entity> widgets() const {
        std::vector<Entity> out;
        for (Entity e : cs.entities_with_component<UIElement>()) {
            auto m = cs.get_component<ScreenMembership>(e);
            if (m.has_value() && m->get().screen_name == kScreenName) out.push_back(e);
        }
        return out;
    }

    bool find_by_click(const std::string& fn, UIElement& out) const {
        for (Entity e : widgets()) {
            auto el = cs.get_component<UIElement>(e);
            if (el.has_value() && el->get().on_click_fn == fn) { out = el->get(); return true; }
        }
        return false;
    }

    Entity by_name(const std::string& name) const {
        const double v = bb.get_or<double>("ui.widget_id." + name, -1.0);
        return v < 0.0 ? 0 : static_cast<Entity>(v);
    }
};

}  // namespace

TEST_CASE("The shop screen exists, is named what ShopSystem pushes, and boots inactive",
          "[Game][ui][shop]") {
    LoadedWorld w;

    REQUIRE(std::string(ShopSystem::SCREEN_NAME) == kScreenName);

    bool found = false;
    for (Entity e : w.cs.entities_with_component<UIScreen>()) {
        auto s = w.cs.get_component<UIScreen>(e);
        if (!s.has_value() || s->get().screen_name != kScreenName) continue;
        found = true;
        // Inactive at load, or it would sit over the title screen from frame zero.
        REQUIRE_FALSE(s->get().active);
    }
    REQUIRE(found);
    REQUIRE_FALSE(w.widgets().empty());
}

TEST_CASE("Every widget ShopSystem resolves by name is published", "[Game][ui][shop]") {
    LoadedWorld w;
    for (const std::string& name : kNamedWidgets) {
        INFO("missing widget name: " << name);
        REQUIRE(w.by_name(name) != 0);
    }
}

TEST_CASE("Card, tab and leave callbacks are exactly what shop_system.cpp compares to",
          "[Game][ui][shop]") {
    LoadedWorld w;

    for (int i = 0; i < kCards; ++i) {
        UIElement card{};
        const std::string fn = "on_shop_card_" + std::to_string(i);
        INFO("callback: " << fn);
        REQUIRE(w.find_by_click(fn, card));
        // Must be a button: UISystem only confirms clicks (and grants focus) to
        // interactive kinds — a "label" would draw and never fire.
        REQUIRE(card.element_type == "button");
    }
    // Tier 8 (D221/D225): GEAR/LEVELS retired — exactly one tab remains.
    {
        UIElement tab{};
        REQUIRE(w.find_by_click("on_shop_page_0", tab));
        REQUIRE(tab.element_type == "button");
        REQUIRE_FALSE(tab.label_text.empty());
        UIElement gone{};
        CHECK_FALSE(w.find_by_click("on_shop_page_1", gone));
        CHECK_FALSE(w.find_by_click("on_shop_page_2", gone));
    }

    UIElement leave{};
    REQUIRE(w.find_by_click("on_shop_leave", leave));
    REQUIRE(leave.element_type == "button");
    REQUIRE_FALSE(leave.label_text.empty());
    // The way out pulses, like the intermission's primary action.
    REQUIRE(leave.pulse_hz > 0.0f);

    // The card prefix test in menu_tick is rfind("on_shop_", 0) == 0, so no other
    // widget on this screen may start with that prefix without being handled.
    for (Entity e : w.widgets()) {
        auto el = w.cs.get_component<UIElement>(e);
        REQUIRE(el.has_value());
        const std::string& fn = el->get().on_click_fn;
        if (fn.rfind("on_shop_", 0) != 0) continue;
        INFO("unhandled on_shop_* callback: " << fn);
        REQUIRE((fn == "on_shop_leave" ||
                 fn.rfind("on_shop_page_", 0) == 0 ||
                 fn.rfind("on_shop_card_", 0) == 0));
    }
}

TEST_CASE("Every shop widget resolves a real style", "[Game][ui][shop]") {
    LoadedWorld w;

    auto table = w.bb.get_or<std::shared_ptr<StyleTable>>("ui_styles", nullptr);
    REQUIRE(table != nullptr);

    for (Entity e : w.widgets()) {
        auto el = w.cs.get_component<UIElement>(e);
        REQUIRE(el.has_value());
        INFO("widget style_id: " << el->get().style_id);
        REQUIRE_FALSE(el->get().style_id.empty());
        REQUIRE(table->contains(el->get().style_id));
    }

    // Playtest #5: the hover preview moved off the cards and onto the right-hand
    // stat sheet, which ShopSystem restyles by these names. Code-made labels are
    // invisible to the widget loop above, so pin them here.
    REQUIRE(table->contains("pip_gain"));
    REQUIRE(table->contains("pip_loss"));
    // Playtest #4: the hold-to-buy progress is the WHOLE card filling left to
    // right in light blue, so it has its own translucent style rather than
    // borrowing the opaque HUD gauge.
    REQUIRE(table->contains("hud_hold"));
}

TEST_CASE("Shop cards are on-canvas, clickable and disjoint", "[Game][ui][shop]") {
    LoadedWorld w;

    std::vector<UIRect> rects;
    for (int i = 0; i < kCards; ++i) {
        UIElement card{};
        REQUIRE(w.find_by_click("on_shop_card_" + std::to_string(i), card));
        INFO("card " << i << " rect authored in the " << UI_DESIGN_WIDTH << "x"
             << UI_DESIGN_HEIGHT << " design canvas");
        REQUIRE(card.rect.x >= 0.0f);
        REQUIRE(card.rect.y >= 0.0f);
        REQUIRE(card.rect.x + card.rect.w <= UI_DESIGN_WIDTH);
        REQUIRE(card.rect.y + card.rect.h <= UI_DESIGN_HEIGHT);
        REQUIRE(card.rect.w >= 100.0f);
        REQUIRE(card.rect.h >= 30.0f);
        rects.push_back(card.rect);
    }

    // Overlapping cards would let one steal the other's clicks — point_in_rect is
    // inclusive, so both would report the pointer inside.
    for (size_t a = 0; a < rects.size(); ++a) {
        for (size_t b = a + 1; b < rects.size(); ++b) {
            const bool dx = rects[a].x + rects[a].w <= rects[b].x ||
                            rects[b].x + rects[b].w <= rects[a].x;
            const bool dy = rects[a].y + rects[a].h <= rects[b].y ||
                            rects[b].y + rects[b].h <= rects[a].y;
            INFO("cards " << a << " and " << b << " overlap");
            REQUIRE((dx || dy));
        }
    }
}
