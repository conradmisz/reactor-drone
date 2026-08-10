#include "minimap_system.hpp"

#include <algorithm>
#include <iostream>
#include <string>

#include "minimap_math.hpp"
#include "enemy_components.hpp"    // EnemyTag, EnemyBehavior, behavior_kinds
#include "player_components.hpp"   // PlayerTag, Pickup
#include "game_hud_system.hpp"     // hud_visible_in_phase — one visibility rule

namespace {

/// Blip edge length in design-canvas units, derived from the frame size so the
/// map reads the same at any configured size. Floored so it never vanishes.
float blip_edge(float frame_size) {
    // Divisor loosened from 28 to 24 when the radar shrank from 140 to 96: the
    // blip has to stay a readable mark, and at /28 a 96px radar drew 3.4px specks.
    return std::max(3.0f, frame_size / 24.0f);
}

/// World centre of an entity that has Position (+ optional Size).
bool world_centre(const ComponentStorage& cs, Entity e, float& cx, float& cy) {
    auto pos = cs.get_component<Position>(e);
    if (!pos.has_value()) return false;
    auto sz = cs.get_component<Size>(e);
    cx = pos->get().x + (sz.has_value() ? sz->get().width * 0.5f : 0.0f);
    cy = pos->get().y + (sz.has_value() ? sz->get().height * 0.5f : 0.0f);
    return true;
}

}  // namespace

void MinimapSystem::ensure_pool(ComponentStorage& component_storage,
                                EntityManager& entity_manager) {
    const std::size_t cap = static_cast<std::size_t>(std::max(0, cfg_.max_blips));
    if (pool_.size() == cap) return;

    pool_.reserve(cap);
    while (pool_.size() < cap) {
        Entity e = entity_manager.create_entity();
        UIElement el;
        el.element_type = "panel";
        el.rect = UIRect{0.0f, 0.0f, 0.0f, 0.0f};   // zero-size = drawn as nothing
        el.style_id = STYLE_ENEMY;
        el.z_order = 21;                            // above the frame panel (20)
        component_storage.add_component<UIElement>(e, el);
        component_storage.add_component<UIState>(e, UIState{});
        component_storage.add_component<ScreenMembership>(e,
            ScreenMembership{std::string(SCREEN_NAME)});
        pool_.push_back(e);
    }
}

void MinimapSystem::update(ComponentStorage& component_storage,
                           EntityManager& entity_manager,
                           Blackboard& blackboard) {
    if (!cfg_.enabled || cfg_.max_blips <= 0 || cfg_.size <= 0.0f) return;
    ensure_pool(component_storage, entity_manager);

    // The frame panel is authored in GameData's `gameplay` screen for its style
    // and screen membership; its GEOMETRY comes from the `minimap` config block,
    // which is written over the authored rect here. One authority for where the
    // map is, rather than two that can silently disagree. Written every frame
    // rather than once at pool time, because hiding the radar collapses it.
    const bool show = hud_visible_in_phase(blackboard.get_or<int>("phase", 0));
    const double frame_id =
        blackboard.get_or<double>(std::string("ui.widget_id.") + FRAME_WIDGET, -1.0);
    if (frame_id >= 0.0) {
        Entity frame = static_cast<Entity>(frame_id);
        if (auto el = component_storage.get_component<UIElement>(frame); el.has_value()) {
            el->get().rect = show ? UIRect{cfg_.x, cfg_.y, cfg_.size, cfg_.size}
                                  : UIRect{cfg_.x, cfg_.y, 0.0f, 0.0f};
        }
    }
    if (!show) {
        // The radar is arena furniture: it has no business on the title screen or
        // under the shop panel. Park every blip, same zero-size hide as an unused
        // pool slot, and skip the whole scan.
        for (Entity e : pool_) {
            if (auto el = component_storage.get_component<UIElement>(e); el.has_value()) {
                el->get().rect.w = 0.0f;
                el->get().rect.h = 0.0f;
            }
        }
        return;
    }

    const minimap_math::Rect frame{cfg_.x, cfg_.y, cfg_.size, cfg_.size};
    const float edge = blip_edge(cfg_.size);

    // Collected in PRIORITY order, so that when the world outruns the cap it is
    // the least informative blips (the far tail of the swarm) that are dropped,
    // never the player or the boss.
    struct Blip { float x, y, edge; const char* style; };
    std::vector<Blip> blips;
    blips.reserve(pool_.size() + 16);

    for (Entity p : component_storage.entities_with_component<PlayerTag>()) {
        float cx, cy;
        if (!world_centre(component_storage, p, cx, cy)) continue;
        blips.push_back({cx, cy, edge * 1.4f, STYLE_PLAYER});
        break;
    }

    int wanted = static_cast<int>(blips.size());

    // Boss first, then ordinary enemies, in two passes over the same set.
    for (int pass = 0; pass < 2; ++pass) {
        for (Entity e : component_storage.entities_with_component<EnemyTag>()) {
            if (component_storage.has_component<DestroyRequest>(e)) continue;
            auto beh = component_storage.get_component<EnemyBehavior>(e);
            const bool boss = beh.has_value() && beh->get().kind == behavior_kinds::BOSS;
            if (boss != (pass == 0)) continue;
            ++wanted;
            if (blips.size() >= pool_.size()) continue;
            float cx, cy;
            if (!world_centre(component_storage, e, cx, cy)) { --wanted; continue; }
            blips.push_back({cx, cy, boss ? edge * 2.5f : edge, STYLE_ENEMY});
        }
    }

    for (Entity e : component_storage.entities_with_component<Pickup>()) {
        if (component_storage.has_component<DestroyRequest>(e)) continue;
        ++wanted;
        if (blips.size() >= pool_.size()) continue;
        float cx, cy;
        if (!world_centre(component_storage, e, cx, cy)) { --wanted; continue; }
        // Lane M (#4, D114): a health pack is green, not loot-amber. It is the one
        // pickup the player crosses the arena for, and it was indistinguishable
        // from a coin on a 96px radar. Read off Pickup.kind, so it costs one
        // branch and no second scan.
        auto pk = component_storage.get_component<Pickup>(e);
        const bool health = pk.has_value() &&
                            pk->get().kind == static_cast<int>(PickupKind::Health);
        blips.push_back({cx, cy, edge, health ? STYLE_HEALTH : STYLE_PICKUP});
    }

    peak_requested_ = std::max(peak_requested_, wanted);

    // The cap is a real loss of information, so it is reported — but only when it
    // gets WORSE, or a capped frame would print the same line 60 times a second
    // and bury everything else in the log.
    const int overflow = wanted - static_cast<int>(pool_.size());
    if (overflow > logged_overflow_) {
        logged_overflow_ = overflow;
        std::cout << "  [Minimap] blip cap " << pool_.size() << " reached: "
                  << wanted << " wanted, " << overflow << " not shown\n";
    }

    for (std::size_t i = 0; i < pool_.size(); ++i) {
        auto el = component_storage.get_component<UIElement>(pool_[i]);
        if (!el.has_value()) continue;
        if (i >= blips.size()) {
            el->get().rect.w = 0.0f;      // parked: a zero-size panel draws nothing
            el->get().rect.h = 0.0f;
            continue;
        }
        const Blip& b = blips[i];
        const minimap_math::Rect r = minimap_math::blip_rect(
            b.x, b.y, arena_.center_x, arena_.center_y, arena_.radius, frame, b.edge);
        el->get().rect = UIRect{r.x, r.y, r.w, r.h};
        el->get().style_id = b.style;
    }
}
