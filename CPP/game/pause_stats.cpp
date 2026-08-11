#include "pause_stats.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "active_items.hpp"        // actives::ids, active_def
#include "enemy_components.hpp"    // Health
#include "game_hud_system.hpp"     // hud_visible_in_phase — one visibility rule
#include "player_components.hpp"   // PlayerTag, ShipState, WeaponStats
#include "prestige.hpp"            // PRESTIGE_LEVEL_KEY, prestige_summary (Lane O)

namespace pause_stats {
namespace {

// The stat block's column, in the 800x600 design canvas. Shares the pause
// panel's 24px content inset and its 472px column with every authored widget on
// that screen (D88), so the sheet reads as part of the panel rather than as a
// second layout.
constexpr float COL_X = 164.0f;
constexpr float COL_W = 472.0f;
constexpr float TOP_Y = 480.0f;    // bottom edge of the first line
constexpr float STEP  = 22.0f;
constexpr float LINE_H = 20.0f;

std::string fmt(const char* f, double a) {
    char buf[96];
    std::snprintf(buf, sizeof(buf), f, a);
    return buf;
}

/// Five-glyph ownership meter for a shop row (D188): filled = round(5*owned/max).
/// Font is DejaVu, so the UTF-8 dots render as-is. max <= 0 reads as empty.
std::string pips(int owned, int max) {
    const int filled = max > 0 ? std::clamp<int>(static_cast<int>(std::lround(5.0 * owned / max)), 0, 5) : 0;
    std::string s;
    for (int i = 0; i < 5; ++i) s += i < filled ? "●" : "○";
    return s;
}

// Stack caps per shop row, in row order (hull, shield, speed, fire_rate, damage,
// extra_shot, range, bounce). GameData.json shop.upgrades is the source of truth
// (D188) — the fixed stat rows below have no ShopUpgradeDef in hand to read from.
constexpr int kMaxStacks[8] = {8, 5, 5, 6, 8, 2, 3, 3};

/// Pad `label` out to a fixed width so the values line up in a column. The font
/// is proportional, so this is an approximation — but a leading-space column is
/// still far more scannable than ragged "NAME value" pairs, and the alternative
/// is a second widget per row purely to hold the value.
std::string row(const char* label, const std::string& value) {
    std::string s = label;
    while (s.size() < 11) s += ' ';
    return s + value;
}

/// One purchased upgrade's cumulative effect, from its catalogue `effect` string
/// (D26 — never the row index) and the number of stacks bought. Stats with a
/// natural base also show the gain as a percent of it (D188); the base is
/// recovered as current-minus-added, so prestige and gear stay accounted for.
std::string effect_total(const std::string& effect, float amount, int count,
                         const Snapshot& s) {
    const double total = static_cast<double>(amount) * count;
    // "+N unit (+P%)" when the base is recoverable, bare "+N unit" when not
    // (a fresh shield's base is 0, and the raw number is the truth then).
    auto with_pct = [&](const char* f, double base) {
        std::string out = fmt(f, total);
        if (base > 0.0) out += fmt(" (+%.0f%%)", total / base * 100.0);
        return out;
    };
    if (effect == "hull")       return with_pct("+%.0f hull", s.hull_max - total);
    if (effect == "shield")     return with_pct("+%.0f shield", s.shield_max - total);
    if (effect == "speed")      return fmt("+%.0f%% speed", total * 100.0);
    if (effect == "fire_rate")  return with_pct("+%.1f/s fire rate", s.fire_rate - total);
    if (effect == "damage")     return with_pct("+%.0f damage", s.damage - total);
    if (effect == "extra_shot") return fmt("+%.0f shot", total);
    if (effect == "range")      return fmt("+%.0f%% range", total * 100.0);
    if (effect == "bounce")     return fmt("+%.0f bounce", total);
    // ponytail: a new catalogue effect reads as a bare number until someone adds
    // a phrase here. Wrong-but-visible beats a silently missing row.
    return fmt("+%.1f", total);
}

}  // namespace

UIRect line_rect(int i) {
    return UIRect{COL_X, TOP_Y - static_cast<float>(i) * STEP, COL_W, LINE_H};
}

std::string active_tag(int active_id) {
    switch (active_id) {
        case actives::ids::MISSILES:       return "MISSILES";
        case actives::ids::LASER:          return "LASER";
        case actives::ids::REPULSOR_FIELD: return "REPULSOR";
        default:                           return std::string();
    }
}

std::string active_key(int active_id) {
    // The repulsion device has no key by design (D71): it fires itself below 20%
    // hull, so printing "[E]" on it would be a lie the player acts on.
    return active_id == actives::ids::REPULSOR_FIELD ? "AUTO" : "[E]";
}

std::vector<std::string> stat_lines(const Snapshot& s,
                                    const std::vector<ShopUpgradeDef>& upgrades) {
    std::vector<std::string> out;
    out.reserve(MAX_LINES);

    // The summary already reads "PRESTIGE n  +x% HULL ...", so it is one whole
    // line rather than a label/value pair (#5 asks for the bonuses, not the level).
    if (s.prestige > 0) out.push_back(prestige_summary(s.prestige));

    // Each stat row ends in its shop row's pip meter (D188) — strings only, the
    // numbers and the backend math are untouched.
    out.push_back(row("HULL", fmt("%.0f", static_cast<double>(s.hull)) + " / " +
                              fmt("%.0f", static_cast<double>(s.hull_max))) +
                  "  " + pips(s.upg_counts[0], kMaxStacks[0]));
    out.push_back(row("SHIELD", s.shield_max > 0.0f
                                    ? fmt("%.0f", static_cast<double>(s.shield)) + " / " +
                                          fmt("%.0f", static_cast<double>(s.shield_max))
                                    : std::string("none")) +
                  "  " + pips(s.upg_counts[1], kMaxStacks[1]));
    out.push_back(row("SPEED", fmt("%.0f px/s", static_cast<double>(s.base_speed * s.speed_mult)) +
                                   fmt("   (base %.0f)", static_cast<double>(s.base_speed))) +
                  "  " + pips(s.upg_counts[2], kMaxStacks[2]));
    out.push_back(row("FIRE RATE", fmt("%.1f/s", static_cast<double>(s.fire_rate)) + " " +
                                       pips(s.upg_counts[3], kMaxStacks[3]) +
                                       fmt("   DAMAGE  %.0f", static_cast<double>(s.damage)) + " " +
                                       pips(s.upg_counts[4], kMaxStacks[4])));

    out.push_back(std::string());
    out.push_back("UPGRADES");
    std::size_t before = out.size();
    for (std::size_t i = 0; i < upgrades.size() && i < 8; ++i) {
        const int n = s.upg_counts[i];
        if (n <= 0) continue;
        out.push_back("  " + upgrades[i].name + " x" + std::to_string(n) + "   " +
                      effect_total(upgrades[i].effect, upgrades[i].amount, n, s) +
                      "  " + pips(n, upgrades[i].max_stacks));
    }
    if (out.size() == before) out.push_back("  none purchased");

    out.push_back(std::string());
    std::string gear;
    if (s.item_id >= 0) gear += s.item_name;
    if (s.consumable_id >= 0) {
        if (!gear.empty()) gear += "  /  ";
        gear += "[Q] " + s.consumable_name;
    }
    if (s.active_id >= 0) {
        if (!gear.empty()) gear += "  /  ";
        gear += active_key(s.active_id) + " " + s.active_name;
    }
    out.push_back(row("GEAR", gear.empty() ? std::string("none equipped") : gear));

    // The pool is fixed, so the sheet is capped rather than allowed to run off
    // the panel. It cannot actually be reached: 1 + 5 + 2 + 8 + 2 == MAX_LINES.
    if (out.size() > static_cast<std::size_t>(MAX_LINES))
        out.resize(static_cast<std::size_t>(MAX_LINES));
    return out;
}

}  // namespace pause_stats

void PauseStatsSystem::ensure_pool(ComponentStorage& cs, EntityManager& em) {
    if (pool_.size() == static_cast<std::size_t>(pause_stats::MAX_LINES)) return;
    pool_.reserve(static_cast<std::size_t>(pause_stats::MAX_LINES));
    while (pool_.size() < static_cast<std::size_t>(pause_stats::MAX_LINES)) {
        Entity e = em.create_entity();
        UIElement el;
        el.element_type = "label";
        el.rect = pause_stats::line_rect(static_cast<int>(pool_.size()));
        el.style_id = "caption";
        // 40, matching the re-authored pause screen (D113): UIRenderSystem sorts
        // z_order GLOBALLY across every active screen, and "gameplay" is always
        // active, so a pause widget below 21 is drawn under the hull gauge.
        el.z_order = 40;
        cs.add_component<UIElement>(e, el);
        cs.add_component<UIState>(e, UIState{});
        cs.add_component<ScreenMembership>(e, ScreenMembership{std::string(PAUSE_SCREEN)});
        pool_.push_back(e);
    }
}

void PauseStatsSystem::resolve_slot(ComponentStorage& cs, const Blackboard& bb) {
    if (slot_resolved_) return;
    static const char* kNames[3] = {SLOT_FRAME, SLOT_NAME, SLOT_KEY};
    for (int i = 0; i < 3; ++i) {
        const double v = bb.get_or<double>(std::string("ui.widget_id.") + kNames[i], -1.0);
        slot_[i] = v < 0.0 ? 0 : static_cast<Entity>(v);
        if (slot_[i] == 0) continue;
        if (auto el = cs.get_component<UIElement>(slot_[i]); el.has_value())
            slot_rect_[i] = el->get().rect;
    }
    slot_resolved_ = true;   // load-time ids; a data file without the slot no-ops
}

void PauseStatsSystem::update(ComponentStorage& cs, EntityManager& em, Blackboard& bb,
                              const GameConfig& cfg) {
    ensure_pool(cs, em);
    resolve_slot(cs, bb);

    // --- one query of the player, feeding both readouts ---
    pause_stats::Snapshot s;
    s.base_speed = cfg.player.move_speed;
    // Lane O publishes the level as a double under its own key (D131).
    s.prestige = static_cast<int>(bb.get_or<double>(PRESTIGE_LEVEL_KEY, 0.0));
    float active_cd = 0.0f;
    bool have_player = false;
    for (Entity p : cs.entities_with_component<PlayerTag>()) {
        have_player = true;
        if (auto h = cs.get_component<Health>(p); h.has_value()) {
            s.hull = h->get().current;
            s.hull_max = h->get().max_hp;
        }
        if (auto w = cs.get_component<WeaponStats>(p); w.has_value()) {
            s.fire_rate = w->get().fire_rate;
            s.damage = w->get().damage;
        }
        if (auto st = cs.get_component<ShipState>(p); st.has_value()) {
            const ShipState& v = st->get();
            s.shield = v.shield;
            s.shield_max = v.shield_max;
            s.speed_mult = v.speed_mult;
            s.item_id = v.item_id;
            s.consumable_id = v.consumable_id;
            s.active_id = v.active_id;
            active_cd = v.active_cd;
            for (int i = 0; i < 8; ++i) s.upg_counts[i] = v.upg_counts[i];
        }
        break;
    }
    s.item_name = bb.get_or<std::string>("ship.item_name", std::string("Item"));
    s.consumable_name = bb.get_or<std::string>("ship.consumable_name", std::string("Consumable"));
    if (const ActiveItemDef* d = actives::active_def(cfg.actives, s.active_id))
        s.active_name = d->name;

    // --- pause screen (#5) ---
    // Written every frame rather than on the open edge: the pause screen freezes
    // the sim, so nothing changes underneath it, but the shop and the boss reward
    // can both change the answer between two opens and an edge trigger is one
    // more thing to get wrong for no saving.
    const std::vector<std::string> lines =
        have_player ? pause_stats::stat_lines(s, cfg.shop.upgrades)
                    : std::vector<std::string>();
    for (std::size_t i = 0; i < pool_.size(); ++i) {
        auto el = cs.get_component<UIElement>(pool_[i]);
        if (!el.has_value()) continue;
        el->get().label_text = i < lines.size() ? lines[i] : std::string();
    }

    // --- HUD active-item slot (#13) ---
    // Same phase rule as the rest of the arena furniture (D86), and the same
    // hide: a zero-size rect, because UIElement has no visibility flag.
    const bool show = hud_visible_in_phase(bb.get_or<int>("phase", 0)) && s.active_id >= 0;
    for (int i = 0; i < 3; ++i) {
        if (slot_[i] == 0) continue;
        auto el = cs.get_component<UIElement>(slot_[i]);
        if (!el.has_value()) continue;
        el->get().rect = show ? slot_rect_[i]
                              : UIRect{slot_rect_[i].x, slot_rect_[i].y, 0.0f, 0.0f};
    }
    if (!show) return;
    if (slot_[1] != 0) {
        if (auto el = cs.get_component<UIElement>(slot_[1]); el.has_value())
            el->get().label_text = pause_stats::active_tag(s.active_id);
    }
    if (slot_[2] != 0) {
        if (auto el = cs.get_component<UIElement>(slot_[2]); el.has_value()) {
            char cd[16];
            std::snprintf(cd, sizeof(cd), "%.0fs", std::ceil(static_cast<double>(active_cd)));
            el->get().label_text = pause_stats::active_key(s.active_id) + " " +
                                   (active_cd > 0.0f ? cd : "READY");
        }
    }
}
