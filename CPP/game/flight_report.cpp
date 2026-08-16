#include "flight_report.hpp"

#include <algorithm>
#include <string>

#include "engine/ecs/fx_events.hpp"
#include "minimap_math.hpp"
#include "player_components.hpp"   // PlayerTag

namespace {

// Terminal phases: Phase::PHASE_GAMEOVER / PHASE_VICTORY in main.cpp. Restated as
// constants here for the same reason hud_visible_in_phase does it — this file has
// no business including main.cpp's enum.
constexpr int kPlaying  = 1;
constexpr int kGameOver = 2;
constexpr int kVictory  = 3;

/// Mark edge length in design-canvas units, derived from the report size so the
/// report reads the same at any configured size.
float mark_edge(float size, float divisor) {
    return std::max(2.0f, size / divisor);
}

}  // namespace

void FlightReport::reset() {
    path_.clear();
    kills_.clear();
    hits_.clear();
    path_cursor_ = kill_cursor_ = hit_cursor_ = 0;
    frame_counter_ = 0;
    last_hull_ = -1.0f;
    // The pool is deliberately NOT torn down: the widgets survive spawn_world and
    // are re-pointed, never re-created (the D58 lesson about recycled entity ids).
}

void FlightReport::push_ring(std::vector<Mark>& ring, std::size_t& cursor,
                             std::size_t cap, float x, float y) {
    if (cap == 0) return;
    if (ring.size() < cap) {
        ring.push_back({x, y});
        cursor = ring.size() % cap;
        return;
    }
    ring[cursor] = {x, y};
    cursor = (cursor + 1) % cap;
}

void FlightReport::record(ComponentStorage& component_storage, Blackboard& blackboard) {
    const std::size_t cap = static_cast<std::size_t>(std::max(1, cfg_.max_samples));

    // --- the flight path, every Nth frame ---------------------------------
    ++frame_counter_;
    const int every = std::max(1, cfg_.sample_every_n);
    float px = 0.0f, py = 0.0f;
    bool have_player = false;
    float hull = -1.0f;
    for (Entity p : component_storage.entities_with_component<PlayerTag>()) {
        auto pos = component_storage.get_component<Position>(p);
        auto sz = component_storage.get_component<Size>(p);
        if (pos.has_value()) {
            px = pos->get().x + (sz.has_value() ? sz->get().width * 0.5f : 0.0f);
            py = pos->get().y + (sz.has_value() ? sz->get().height * 0.5f : 0.0f);
            have_player = true;
        }
        if (auto h = component_storage.get_component<Health>(p);
            h.has_value() && h->get().max_hp > 0.0f)
            hull = h->get().current / h->get().max_hp;
        break;
    }
    if (have_player && frame_counter_ % every == 0)
        push_ring(path_, path_cursor_, cap, px, py);

    // --- hits taken, wherever the drone was standing ----------------------
    // Detected from the hull dropping rather than from a new publisher at the
    // damage site: one fewer shared-file edit, and it catches every damage source
    // (contact, shots, hazards, surges) including ones not written yet.
    if (hull >= 0.0f && last_hull_ >= 0.0f && hull < last_hull_ - 1e-4f && have_player)
        push_ring(hits_, hit_cursor_, cap, px, py);
    if (hull >= 0.0f) last_hull_ = hull;

    // --- kills, off the shared render-FX vocabulary ------------------------
    // Kill marks are already published at every death (Phase 0, D138) and this is
    // a render-side consumer, so the kill positions cost nothing new. The list is
    // capped per frame at the publisher, so this loop is bounded. (D151: the marks
    // outlived the battle-scar layer that first needed them — this is now their
    // only consumer.)
    for (const fx_events::Mark& m :
         blackboard.get_or<std::vector<fx_events::Mark>>(fx_events::KILL_MARKS, {})) {
        push_ring(kills_, kill_cursor_, cap, m.x, m.y);
    }
}

void FlightReport::ensure_pool(ComponentStorage& component_storage,
                               EntityManager& entity_manager) {
    const std::size_t want = PATH_MARKS + KILL_MARKS + HIT_MARKS;
    if (pool_.size() == want) return;
    pool_.reserve(want);
    while (pool_.size() < want) {
        const std::size_t i = pool_.size();
        Entity e = entity_manager.create_entity();
        UIElement el;
        el.element_type = "panel";
        el.rect = UIRect{0.0f, 0.0f, 0.0f, 0.0f};   // zero size = drawn as nothing
        el.style_id = i < PATH_MARKS ? STYLE_PATH
                    : (i < PATH_MARKS + KILL_MARKS ? STYLE_KILL : STYLE_HIT);
        // Above the HUD's 10-21 band and above a modal's panel (30/40) would be
        // wrong — the report is content, not chrome, so it sits at 31: over the
        // game-over panel's background, under its buttons.
        el.z_order = 31;
        component_storage.add_component<UIElement>(e, el);
        component_storage.add_component<UIState>(e, UIState{});
        component_storage.add_component<ScreenMembership>(e,
            ScreenMembership{std::string(SCREEN_NAME)});
        pool_.push_back(e);
    }
}

void FlightReport::park_all(ComponentStorage& component_storage) {
    for (Entity e : pool_) {
        if (auto el = component_storage.get_component<UIElement>(e); el.has_value()) {
            el->get().rect.w = 0.0f;
            el->get().rect.h = 0.0f;
        }
    }
}

void FlightReport::draw(ComponentStorage& component_storage,
                        EntityManager& entity_manager, Blackboard& blackboard) {
    ensure_pool(component_storage, entity_manager);

    const minimap_math::Rect frame{cfg_.x, cfg_.y, cfg_.size, cfg_.size};
    std::size_t slot = 0;

    // One helper for all three families: map a world point onto the report square
    // with the minimap's proven mapping (an off-arena point clamps to the RIM, so
    // the direction stays honest) and write it into the next pool slot.
    auto place = [&](const std::vector<Mark>& marks, std::size_t first,
                     std::size_t count, float divisor) {
        const float edge = mark_edge(cfg_.size, divisor);
        // Decimate rather than truncate: a long run's path is sampled ACROSS the
        // whole run, so the report shows the shape of the flight instead of its
        // first thirty seconds.
        const std::size_t n = marks.size();
        for (std::size_t i = 0; i < count; ++i) {
            Entity e = pool_[first + i];
            auto el = component_storage.get_component<UIElement>(e);
            if (!el.has_value()) continue;
            if (n == 0 || i >= n) { el->get().rect.w = 0.0f; el->get().rect.h = 0.0f; continue; }
            const std::size_t src = n <= count ? i : (i * n) / count;
            const Mark& m = marks[src];
            const minimap_math::Rect r = minimap_math::blip_rect(
                m.x, m.y, arena_.center_x, arena_.center_y, arena_.radius, frame, edge);
            el->get().rect = UIRect{r.x, r.y, r.w, r.h};
        }
    };

    place(path_, slot, PATH_MARKS, 60.0f);            slot += PATH_MARKS;
    place(kills_, slot, KILL_MARKS, 34.0f);           slot += KILL_MARKS;
    place(hits_, slot, HIT_MARKS, 22.0f);
    (void)blackboard;
}

void FlightReport::update(ComponentStorage& component_storage,
                          EntityManager& entity_manager,
                          Blackboard& blackboard) {
    if (!cfg_.enabled) return;
    const int phase = blackboard.get_or<int>("phase", 0);

    if (phase == kPlaying) {
        record(component_storage, blackboard);
        // The report is not drawn while flying — the arena itself is the display.
        if (!pool_.empty()) park_all(component_storage);
        return;
    }
    if (phase == kGameOver || phase == kVictory) {
        draw(component_storage, entity_manager, blackboard);
        return;
    }
    // Title, shop, intermission: neither record nor draw. Parking is what keeps a
    // report from bleeding onto the main menu of the NEXT run.
    if (!pool_.empty()) park_all(component_storage);
}
