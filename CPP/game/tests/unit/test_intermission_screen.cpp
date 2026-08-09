/**
 * test_intermission_screen.cpp — contract test for the wave-intermission menu.
 *
 * The prompt that replaced the shop's auto-open is authored entirely in
 * assets/GameData.json, but main.cpp reacts to it by comparing the clicked
 * widget's `on_click_fn` against two string literals. That seam is invisible to
 * the compiler: renaming a callback in the JSON, dropping a button, or typo'ing
 * a style id all leave a game that builds, runs, and then freezes forever on a
 * prompt whose buttons do nothing.
 *
 * So these tests load the REAL shipped GameData.json — not a fixture — and pin:
 *   - the screen exists and boots inactive (it must not cover the title screen);
 *   - both buttons exist, with exactly the callback names main.cpp compares to;
 *   - the shop button pulses and the continue button does not;
 *   - every widget's style_id resolves in the shipped ui_styles table;
 *   - the buttons are inside the design canvas and do not overlap.
 *
 * Keep the two kExpected* constants below in sync with main.cpp's comparisons.
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

namespace {

// The literals main.cpp compares UISystem::UI_CLICK_KEY against.
constexpr const char* kExpectedShopFn     = "on_shop_open_click";
constexpr const char* kExpectedContinueFn = "on_continue_click";
constexpr const char* kScreenName         = "wave_intermission";

struct LoadedWorld {
    EntityManager em;
    ComponentStorage cs;
    Blackboard bb;

    LoadedWorld() {
        load_game_data(project_paths::assets_dir() + "/GameData.json", em, cs, bb);
    }

    // Widgets belonging to the intermission screen, in entity order.
    std::vector<Entity> widgets() const {
        std::vector<Entity> out;
        for (Entity e : cs.entities_with_component<UIElement>()) {
            auto m = cs.get_component<ScreenMembership>(e);
            if (m.has_value() && m->get().screen_name == kScreenName) out.push_back(e);
        }
        return out;
    }

    // The single widget whose on_click_fn matches, or entity 0 with found=false.
    bool find_by_click(const std::string& fn, UIElement& out) const {
        for (Entity e : widgets()) {
            auto el = cs.get_component<UIElement>(e);
            if (el.has_value() && el->get().on_click_fn == fn) { out = el->get(); return true; }
        }
        return false;
    }
};

}  // namespace

TEST_CASE("The wave-intermission screen exists and boots inactive", "[Game][ui]") {
    LoadedWorld w;

    bool found = false;
    for (Entity e : w.cs.entities_with_component<UIScreen>()) {
        auto s = w.cs.get_component<UIScreen>(e);
        if (!s.has_value() || s->get().screen_name != kScreenName) continue;
        found = true;
        // Must NOT be active at load: the run opens on the title screen, and an
        // active intermission would sit over it from frame zero.
        REQUIRE_FALSE(s->get().active);
    }
    REQUIRE(found);
    REQUIRE_FALSE(w.widgets().empty());
}

TEST_CASE("Both intermission buttons carry the callback names main.cpp expects",
          "[Game][ui]") {
    LoadedWorld w;

    UIElement shop{};
    UIElement cont{};
    REQUIRE(w.find_by_click(kExpectedShopFn, shop));
    REQUIRE(w.find_by_click(kExpectedContinueFn, cont));

    // Both must be buttons: UISystem only grants keyboard focus to interactive
    // kinds, and a "label" would render but never confirm a click.
    REQUIRE(shop.element_type == "button");
    REQUIRE(cont.element_type == "button");
    REQUIRE_FALSE(shop.label_text.empty());
    REQUIRE_FALSE(cont.label_text.empty());
}

TEST_CASE("Only the shop button pulses", "[Game][ui]") {
    LoadedWorld w;

    UIElement shop{};
    UIElement cont{};
    REQUIRE(w.find_by_click(kExpectedShopFn, shop));
    REQUIRE(w.find_by_click(kExpectedContinueFn, cont));

    // The whole point of the prompt: the shop invites, the continue button waits.
    REQUIRE(shop.pulse_hz > 0.0f);
    REQUIRE(cont.pulse_hz == 0.0f);

    // A pulse slow enough to read as a glow, fast enough to notice.
    REQUIRE(shop.pulse_hz >= 0.4f);
    REQUIRE(shop.pulse_hz <= 4.0f);
}

TEST_CASE("Every intermission widget resolves a real style", "[Game][ui]") {
    LoadedWorld w;

    auto table = w.bb.get_or<std::shared_ptr<StyleTable>>("ui_styles", nullptr);
    REQUIRE(table != nullptr);

    for (Entity e : w.widgets()) {
        auto el = w.cs.get_component<UIElement>(e);
        REQUIRE(el.has_value());
        const UIElement& ui = el->get();
        INFO("widget style_id: " << ui.style_id);
        REQUIRE_FALSE(ui.style_id.empty());
        // contains() distinguishes "style is defined" from lookup()'s fallback,
        // which happily returns the magenta sentinel for an unknown id.
        REQUIRE(table->contains(ui.style_id));
    }
}

TEST_CASE("Intermission buttons are on-canvas and do not overlap", "[Game][ui]") {
    LoadedWorld w;

    UIElement shop{};
    UIElement cont{};
    REQUIRE(w.find_by_click(kExpectedShopFn, shop));
    REQUIRE(w.find_by_click(kExpectedContinueFn, cont));

    auto on_canvas = [](const UIRect& r) {
        return r.x >= 0.0f && r.y >= 0.0f &&
               r.x + r.w <= UI_DESIGN_WIDTH && r.y + r.h <= UI_DESIGN_HEIGHT;
    };
    INFO("rects are authored in the " << UI_DESIGN_WIDTH << "x" << UI_DESIGN_HEIGHT
         << " design canvas, not window pixels");
    REQUIRE(on_canvas(shop.rect));
    REQUIRE(on_canvas(cont.rect));

    // Overlapping rects would make one button steal the other's clicks, since
    // point_in_rect is inclusive and both would report the pointer inside.
    const bool disjoint_x = shop.rect.x + shop.rect.w <= cont.rect.x ||
                            cont.rect.x + cont.rect.w <= shop.rect.x;
    const bool disjoint_y = shop.rect.y + shop.rect.h <= cont.rect.y ||
                            cont.rect.y + cont.rect.h <= shop.rect.y;
    REQUIRE((disjoint_x || disjoint_y));

    // Both must be comfortably clickable after the canvas transform.
    REQUIRE(shop.rect.w >= 100.0f);
    REQUIRE(shop.rect.h >= 30.0f);
    REQUIRE(cont.rect.w >= 100.0f);
    REQUIRE(cont.rect.h >= 30.0f);
}
