#include "game_hud_system.hpp"
#include "player_components.hpp"   // PlayerTag, ShipState
#include "enemy_components.hpp"    // Health
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace {
std::string int_str(int v) { return std::to_string(v); }

// Full width of a gauge, in the UI design canvas. Must match the widget rects in
// GameData.json's "gameplay" screen — the widgets are authored at full width, so
// this is the value a 100% bar is restored to.
constexpr float BAR_FULL_W = 240.0f;

// Seconds for the chip bar to catch up with a drop, and the hull fractions at
// which the fill changes colour.
constexpr float CHIP_DRAIN_PER_SEC = 0.55f;
constexpr float HP_WARN_FRAC = 0.55f;
constexpr float HP_CRIT_FRAC = 0.25f;
}

void GameHUDSystem::init(ComponentStorage& component_storage,
                         EntityManager& entity_manager,
                         const Blackboard& blackboard) {
    const float win_w = static_cast<float>(blackboard.get_or<int>("window_width", 980));
    const float win_h = static_cast<float>(blackboard.get_or<int>("window_height", 660));

    auto make = [&](float x, float y, float size, SDL_Color color,
                    const std::string& content) {
        Entity e = entity_manager.create_entity();
        component_storage.add_component<ScreenPosition>(e, ScreenPosition{x, y});
        component_storage.add_component<Text>(e, Text{content, "default.ttf", size, color});
        return e;
    };

    const SDL_Color white  = {235, 235, 245, 255};
    const SDL_Color yellow = {255, 220, 90, 255};
    const SDL_Color cyan   = {120, 225, 255, 255};

    // Row layout, top-down. The hull/shield GAUGES occupy the band between the
    // score line and the credits line (design-canvas y 534..594, i.e. window y
    // ~587..654); these text rows are placed around them, not over them.
    score_entity_   = make(20.0f, win_h - 34.0f, 24.0f, white, "Score: 0");
    health_entity_  = make(20.0f, win_h - 126.0f, 18.0f, white, "100 / 100");
    credits_entity_ = make(20.0f, win_h - 152.0f, 24.0f, yellow, "Credits: 0");
    slots_entity_   = make(20.0f, win_h - 178.0f, 20.0f, cyan, "");
    wave_entity_    = make(win_w - 200.0f, win_h - 36.0f, 24.0f, white, "Wave: 0/0");
    status_entity_  = make(win_w * 0.5f - 190.0f, win_h * 0.5f, 44.0f, yellow, "");
    message_entity_ = make(win_w * 0.5f - 170.0f, win_h * 0.5f - 60.0f, 28.0f, cyan, "");
    initialized_ = true;
}

void GameHUDSystem::resolve_bars(const Blackboard& blackboard) {
    if (bars_resolved_) return;
    // The loader publishes each named widget as a double under this prefix. A
    // missing key means the "gameplay" screen was not authored — leave the ids at
    // 0 and let set_bar no-op, so the game still runs on a data file without a HUD.
    auto id = [&](const char* name) -> Entity {
        const double v = blackboard.get_or<double>(std::string("ui.widget_id.") + name, -1.0);
        return v < 0.0 ? 0 : static_cast<Entity>(v);
    };
    hp_chip_ = id("hud_hp_chip");
    hp_fill_ = id("hud_hp_fill");
    sh_fill_ = id("hud_sh_fill");
    // Resolve once: widget ids are load-time and survive spawn_world, so retrying
    // every frame would only cost lookups. If the screen is absent the ids stay 0
    // and this still latches — the gauges are simply never drawn.
    bars_resolved_ = true;
}

void GameHUDSystem::set_bar(ComponentStorage& cs, Entity bar, float full_w,
                            float frac, const char* style_id) {
    if (bar == 0) return;
    auto el = cs.get_component<UIElement>(bar);
    if (!el.has_value()) return;
    // Clamp rather than trust the caller: overheal or a negative hull would
    // otherwise draw a bar past the end of its frame.
    el->get().rect.w = full_w * std::max(0.0f, std::min(1.0f, frac));
    if (style_id != nullptr) el->get().style_id = style_id;
}

void GameHUDSystem::update(ComponentStorage& component_storage, Blackboard& blackboard) {
    if (!initialized_) return;
    resolve_bars(blackboard);

    // Score
    if (auto t = component_storage.get_component<Text>(score_entity_); t.has_value()) {
        t->get().content = "Score: " + int_str(blackboard.get_or<int>("score", 0));
    }

    // Health + economy (from the player entity)
    int hp = 0, credits = 0, keys = 0, shield = 0;
    float hp_f = 0.0f, max_hp = 0.0f, shield_f = 0.0f, shield_max = 0.0f;
    int item_id = -1, consumable_id = -1, buff_id = -1;
    float buff_timer = 0.0f;
    for (Entity p : component_storage.entities_with_component<PlayerTag>()) {
        if (auto h = component_storage.get_component<Health>(p); h.has_value()) {
            hp_f   = h->get().current;
            max_hp = h->get().max_hp;
            hp = static_cast<int>(hp_f + 0.5f);
        }
        if (auto s = component_storage.get_component<ShipState>(p); s.has_value()) {
            credits = s->get().currency;
            keys    = s->get().keys;
            shield_f   = s->get().shield;
            shield_max = s->get().shield_max;
            shield  = static_cast<int>(shield_f + 0.5f);
            item_id = s->get().item_id;
            consumable_id = s->get().consumable_id;
            buff_id = s->get().buff_id;
            buff_timer = s->get().buff_timer;
        }
        break;
    }
    // --- Hull / shield gauges ---
    // The hull is now a bar, not a number. The numeric readout stays only as a
    // small "cur/max" caption, because "am I one hit from dying?" is a question a
    // bar answers at a glance and a number does not.
    float hp_frac = 0.0f, sh_frac = 0.0f;
    if (max_hp > 0.0f) hp_frac = std::max(0.0f, std::min(1.0f, hp_f / max_hp));
    if (shield_max > 0.0f) sh_frac = std::max(0.0f, std::min(1.0f, shield_f / shield_max));

    // Chip bar: snap UP instantly (a heal should not lag), drain DOWN slowly, so
    // damage leaves a red block behind the fill that visibly bleeds away.
    const float dt = static_cast<float>(blackboard.get_or<double>("delta_time", 0.0));
    if (hp_frac >= hp_chip_frac_) {
        hp_chip_frac_ = hp_frac;
    } else {
        hp_chip_frac_ = std::max(hp_frac, hp_chip_frac_ - CHIP_DRAIN_PER_SEC * dt);
    }

    const char* hp_style = hp_frac <= HP_CRIT_FRAC ? "hud_hp_crit"
                         : hp_frac <= HP_WARN_FRAC ? "hud_hp_warn"
                                                   : "hud_hp_ok";
    set_bar(component_storage, hp_chip_, BAR_FULL_W, hp_chip_frac_);
    set_bar(component_storage, hp_fill_, BAR_FULL_W, hp_frac, hp_style);
    // The shield gauge is always present and simply sits empty until a Shield
    // Capacitor is bought — an empty slot the player can see is a nudge to fill it,
    // where the old text readout just vanished.
    set_bar(component_storage, sh_fill_, BAR_FULL_W, sh_frac);

    if (auto t = component_storage.get_component<Text>(health_entity_); t.has_value()) {
        t->get().content = int_str(hp) + " / " + int_str(static_cast<int>(max_hp + 0.5f)) +
                           (shield_max > 0.0f ? "    SHIELD " + int_str(shield) : std::string());
    }

    // Credits, with the key count only once one has actually dropped (D2 keys are
    // rare enough that a permanent "Keys: 0" would just be noise).
    if (auto t = component_storage.get_component<Text>(credits_entity_); t.has_value()) {
        t->get().content = "Credits: " + int_str(credits) +
                           (keys > 0 ? "   Keys: " + int_str(keys) : std::string());
    }

    // Gameplay Phase 4: equipped gear and the live buff countdown. The names are
    // whatever ShopSystem published when the slot was filled, so the HUD needs no
    // catalogue. Empty line when nothing is equipped — same "no noise" rule as
    // the shield and key readouts.
    if (auto t = component_storage.get_component<Text>(slots_entity_); t.has_value()) {
        std::string line;
        if (item_id >= 0)
            line += blackboard.get_or<std::string>("ship.item_name", std::string("Item"));
        if (consumable_id >= 0) {
            if (!line.empty()) line += "   ";
            line += "[Q] " +
                blackboard.get_or<std::string>("ship.consumable_name", std::string("Consumable"));
        }
        if (buff_id >= 0) {
            if (!line.empty()) line += "   ";
            char buf[24];
            std::snprintf(buf, sizeof(buf), "OVERDRIVE %.1fs", static_cast<double>(buff_timer));
            line += buf;
        }
        t->get().content = line;
    }

    // Wave
    if (auto t = component_storage.get_component<Text>(wave_entity_); t.has_value()) {
        t->get().content = "Wave: " + int_str(blackboard.get_or<int>("wave", 0)) +
                           "/" + int_str(blackboard.get_or<int>("total_waves", 0));
    }

    // Status banner by phase (0=title,1=playing,2=gameover,3=victory).
    int phase = blackboard.get_or<int>("phase", 0);
    std::string status;
    if (phase == 0) status = "REACTOR DRONE - click to start";
    else if (phase == 2) status = "GAME OVER - click to retry";
    else if (phase == 3) status = "VICTORY! - click to retry";
    if (auto t = component_storage.get_component<Text>(status_entity_); t.has_value()) {
        t->get().content = status;
    }

    // Transient message channel (timer ticks down in main via delta_time).
    std::string msg;
    float msg_timer = blackboard.get_or<float>("hud_message_timer", 0.0f);
    if (msg_timer > 0.0f) msg = blackboard.get_or<std::string>("hud_message", std::string());
    if (auto t = component_storage.get_component<Text>(message_entity_); t.has_value()) {
        t->get().content = msg;
    }
}
