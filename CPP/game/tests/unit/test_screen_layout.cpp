/**
 * test_screen_layout.cpp — screen-wide overlap gate (playtest #2, bug 013).
 *
 * The inventory screen shipped with its PROJECTILE COLOR row under the BACK
 * button because nothing checked authored rects against each other. This
 * loads the REAL shipped GameData.json and fails if any two widgets on one
 * screen PARTIALLY overlap. Full containment is allowed — that is the panel /
 * rule / child-widget pattern every screen uses.
 */
#include <catch2/catch_test_macros.hpp>

#include <map>
#include <string>
#include <vector>

#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/components.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/gamedata_loader.hpp"
#include "engine/project_paths.hpp"

namespace {

bool contains(const UIRect& a, const UIRect& b) {
    return a.x <= b.x && a.y <= b.y && a.x + a.w >= b.x + b.w && a.y + a.h >= b.y + b.h;
}

bool intersects(const UIRect& a, const UIRect& b) {
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

}  // namespace

TEST_CASE("no two widgets on one screen partially overlap", "[screens][layout]") {
    EntityManager em;
    ComponentStorage cs;
    Blackboard bb;
    load_game_data(project_paths::assets_dir() + "/GameData.json", em, cs, bb);

    std::map<std::string, std::vector<Entity>> by_screen;
    for (Entity e : cs.entities_with_component<ScreenMembership>())
        by_screen[cs.get_component<ScreenMembership>(e)->get().screen_name].push_back(e);
    REQUIRE(!by_screen.empty());

    for (const auto& [screen, widgets] : by_screen) {
        for (size_t i = 0; i < widgets.size(); ++i) {
            for (size_t j = i + 1; j < widgets.size(); ++j) {
                const UIElement& a = cs.get_component<UIElement>(widgets[i])->get();
                const UIElement& b = cs.get_component<UIElement>(widgets[j])->get();
                // D232: z_order >= 70 marks a floating overlay (the tags
                // dropdown) — it covers other widgets by design.
                if (a.z_order >= 70 || b.z_order >= 70) continue;
                if (!intersects(a.rect, b.rect)) continue;
                if (contains(a.rect, b.rect) || contains(b.rect, a.rect)) continue;
                INFO(screen << ": '" << (a.label_text.empty() ? a.element_type : a.label_text)
                            << "' {" << a.rect.x << "," << a.rect.y << "," << a.rect.w << ","
                            << a.rect.h << "} partially overlaps '"
                            << (b.label_text.empty() ? b.element_type : b.label_text) << "' {"
                            << b.rect.x << "," << b.rect.y << "," << b.rect.w << "," << b.rect.h
                            << "}");
                FAIL();
            }
        }
    }
}
