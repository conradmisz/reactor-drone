#include "game_hud_system.hpp"
#include "player_components.hpp"   // PlayerTag, ShipState
#include "enemy_components.hpp"    // Health
#include "engine/ecs/systems/ui_render_math.hpp"   // ui_canvas_transform
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

    // The gauges are widgets, authored in the 800x600 DESIGN CANVAS and drawn
    // through ui_canvas_transform; these text rows go straight to HUDSystem in
    // window coordinates. Authoring them in window coordinates too is what let
    // the two halves of one HUD drift apart (the canvas is scaled 1.1 and pushed
    // right by 50px at the 980x660 logical surface, so a "left margin of 20"
    // meant two different columns). Both halves are authored in the design canvas
    // now and this lambda applies the same transform the renderer does.
    const UICanvasTransform xf = ui_canvas_transform(win_w, win_h);
    auto make = [&](float dx, float dy, float size, SDL_Color color,
                    const std::string& content) {
        Entity e = entity_manager.create_entity();
        component_storage.add_component<ScreenPosition>(e,
            ScreenPosition{xf.offset_x + dx * xf.scale, xf.offset_y + dy * xf.scale});
        component_storage.add_component<Text>(e,
            Text{content, "default.ttf", size * xf.scale, color});
        return e;
    };

    const SDL_Color white  = {235, 235, 245, 255};
    const SDL_Color yellow = {255, 220, 90, 255};
    const SDL_Color cyan   = {120, 225, 255, 255};

    // DESIGN-CANVAS layout (bottom-left origin, 800x600), one 16px left margin
    // shared with the gauge widgets above it. Reading order top-down:
    //   HULL label 552..582 / hull bar 526..548 / shield bar 504..522  (widgets)
    //   battery bar 482..500                                          (widget, D192)
    //   caption 460 / score 428 / units 398 / gear 372                (text)
    // Rows are 26-30 apart: one line plus a half-line of air, which is what makes
    // a stack of readouts scannable instead of a block. The text stack moved down
    // 20px when the battery gauge joined the bar stack above it (D192 #9).
    score_entity_   = make(16.0f, 428.0f, 22.0f, white,  "Score: 0");
    health_entity_  = make(16.0f, 460.0f, 17.0f, white,  "100 / 100");
    credits_entity_ = make(16.0f, 398.0f, 22.0f, yellow, "Units: 0");
    slots_entity_   = make(16.0f, 372.0f, 18.0f, cyan,   "");
    wave_entity_    = make(520.0f, 556.0f, 22.0f, white, "Wave: 0/0");
    status_entity_  = make(180.0f, 320.0f, 34.0f, yellow, "");
    message_entity_ = make(180.0f, 272.0f, 24.0f, cyan,   "");
    initialized_ = true;
}

void GameHUDSystem::resolve_bars(ComponentStorage& cs, const Blackboard& blackboard) {
    if (bars_resolved_) return;
    // The loader publishes each named widget as a double under this prefix. A
    // missing key means the "gameplay" screen was not authored — leave the ids at
    // 0 and let set_bar no-op, so the game still runs on a data file without a HUD.
    auto id = [&](const char* name) -> Entity {
        const double v = blackboard.get_or<double>(std::string("ui.widget_id.") + name, -1.0);
        return v < 0.0 ? 0 : static_cast<Entity>(v);
    };
    static const char* kGaugeNames[GAUGE_WIDGETS] = {
        "hud_hp_label", "hud_hp_bg", "hud_hp_chip",
        "hud_hp_fill",  "hud_sh_bg", "hud_sh_fill",
        "hud_bat_bg",   "hud_bat_fill",
        "hud_boss_bg",  "hud_boss_fill", "hud_boss_label",
        "hud_dash_frame", "hud_dash_key"
    };
    for (int i = 0; i < GAUGE_WIDGETS; ++i) gauge_[i] = id(kGaugeNames[i]);
    hp_chip_ = gauge_[2];
    hp_fill_ = gauge_[3];
    sh_fill_ = gauge_[5];
    bat_fill_ = gauge_[7];
    boss_bg_ = gauge_[8];
    boss_fill_ = gauge_[9];
    boss_label_ = gauge_[10];
    // Cache the AUTHORED geometry before anything narrows a fill bar, so hiding
    // and re-showing the HUD is lossless.
    for (int i = 0; i < GAUGE_WIDGETS; ++i) {
        if (gauge_[i] == 0) continue;
        if (auto el = cs.get_component<UIElement>(gauge_[i]); el.has_value())
            gauge_rect_[i] = el->get().rect;
    }
    boss_bg_rect_ = gauge_rect_[8];
    boss_label_rect_ = gauge_rect_[10];
    // Resolve once: widget ids are load-time and survive spawn_world, so retrying
    // every frame would only cost lookups. If the screen is absent the ids stay 0
    // and this still latches — the gauges are simply never drawn.
    bars_resolved_ = true;
}

void GameHUDSystem::set_widgets_visible(ComponentStorage& cs, bool visible) {
    for (int i = 0; i < GAUGE_WIDGETS; ++i) {
        if (gauge_[i] == 0) continue;
        auto el = cs.get_component<UIElement>(gauge_[i]);
        if (!el.has_value()) continue;
        el->get().rect = visible ? gauge_rect_[i]
                                 : UIRect{gauge_rect_[i].x, gauge_rect_[i].y, 0.0f, 0.0f};
    }
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
    resolve_bars(component_storage, blackboard);

    // The "gameplay" screen is always on the stack, so nothing else was ever
    // going to switch the arena furniture off: score, gauges, credits and the
    // minimap all rendered on the title screen and under the shop panel. One
    // gate, applied to both the widgets and the text rows, in the system that
    // owns them.
    const int phase = blackboard.get_or<int>("phase", 0);
    const bool show = hud_visible_in_phase(phase);
    set_widgets_visible(component_storage, show);
    if (!show) {
        // Blank the text rows (HUDSystem draws Text, which has no rect to
        // collapse) and leave only the phase banner, which IS the title /
        // game-over / victory message.
        for (Entity e : {score_entity_, health_entity_, credits_entity_,
                         slots_entity_, wave_entity_, message_entity_}) {
            if (auto t = component_storage.get_component<Text>(e); t.has_value())
                t->get().content.clear();
        }
        std::string banner;
        if (phase == 2) banner = "GAME OVER - click to retry";
        else if (phase == 3) banner = "VICTORY! - click to retry";
        // Deliberately nothing at the title: the main_menu screen already carries
        // a REACTOR DRONE heading, and the banner was a second copy of it drawn
        // across the whole window in a font that never fit.
        if (auto t = component_storage.get_component<Text>(status_entity_); t.has_value())
            t->get().content = banner;
        return;
    }
    if (auto t = component_storage.get_component<Text>(status_entity_); t.has_value())
        t->get().content.clear();

    // Score
    if (auto t = component_storage.get_component<Text>(score_entity_); t.has_value()) {
        t->get().content = "Score: " + int_str(blackboard.get_or<int>("score", 0));
    }

    // Health + economy (from the player entity)
    int hp = 0, credits = 0, keys = 0, shield = 0;
    float hp_f = 0.0f, max_hp = 0.0f, shield_f = 0.0f, shield_max = 0.0f;
    int item_id = -1, consumable_id = -1, buff_id = -1;
    float buff_timer = 0.0f;
    float battery = 1.0f;
    bool battery_locked = false;
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
            battery = s->get().battery;
            battery_locked = s->get().battery_locked;
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
    // #9: the battery. It goes red while the trigger is locked out, which is the
    // only feedback the player gets for "you emptied it, wait for full".
    set_bar(component_storage, bat_fill_, BAR_FULL_W, battery,
            battery_locked ? "hud_battery_low" : "hud_battery");

    // #8: the boss bar. Collapsed to nothing unless BossSystem is publishing a
    // fraction — the same zero-rect hide the phase gate uses, so a boss that dies
    // mid-frame takes its bar with it.
    const float boss_frac = blackboard.get_or<float>("boss.hp_frac", -1.0f);
    const bool boss_on = boss_frac >= 0.0f;
    for (Entity e : {boss_bg_, boss_fill_, boss_label_}) {
        if (e == 0) continue;
        if (auto el = component_storage.get_component<UIElement>(e); el.has_value() && !boss_on)
            el->get().rect.w = 0.0f;
    }
    if (boss_on) {
        if (auto el = component_storage.get_component<UIElement>(boss_bg_); el.has_value())
            el->get().rect.w = boss_bg_rect_.w;
        set_bar(component_storage, boss_fill_, boss_bg_rect_.w - 4.0f, boss_frac);
        if (auto el = component_storage.get_component<UIElement>(boss_label_); el.has_value()) {
            el->get().rect.w = boss_label_rect_.w;
            el->get().label_text =
                blackboard.get_or<std::string>("boss.name", std::string("Boss"));
        }
    }

    if (auto t = component_storage.get_component<Text>(health_entity_); t.has_value()) {
        t->get().content = int_str(hp) + " / " + int_str(static_cast<int>(max_hp + 0.5f)) +
                           (shield_max > 0.0f ? "    SHIELD " + int_str(shield) : std::string());
    }

    // Units (#11: the currency is a unit, not a credit or a coin), with the key
    // count only once one has actually dropped (D2 keys are rare enough that a
    // permanent "Keys: 0" would just be noise).
    if (auto t = component_storage.get_component<Text>(credits_entity_); t.has_value()) {
        t->get().content = "Units: " + int_str(credits) +
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

    // The status banner belongs to the non-playing phases only; it is written in
    // the early-out above and cleared here.

    // Transient message channel (timer ticks down in main via delta_time).
    std::string msg;
    float msg_timer = blackboard.get_or<float>("hud_message_timer", 0.0f);
    if (msg_timer > 0.0f) msg = blackboard.get_or<std::string>("hud_message", std::string());
    if (auto t = component_storage.get_component<Text>(message_entity_); t.has_value()) {
        t->get().content = msg;
    }
}
