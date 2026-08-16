#ifndef HANGAR_STATS_HPP
#define HANGAR_STATS_HPP

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "arena_config.hpp"

/**
 * Hangar stat sheet (gameplay pack v2.3 tier 6, D221) — pure row builder, the
 * pause_stats shape: main.cpp owns the widgets, this header owns the numbers.
 *
 * Every row carries its pip meter (0..5) and every pip column is drawn by one
 * label at one x — which is the owner's "make sure the bubbles are aligned".
 * Pips are normalized against the CATALOG_* caps below: a first-pass tuning
 * table, not physics. Rescale them when the roster outgrows them.
 */
namespace hangar {

struct StatRow {
    std::string name;    // "HULL 140"
    int pips = 0;        // 0..5
};

// Normalization caps for the pip meters (>= the biggest authored value).
constexpr float CAP_HULL = 200.0f, CAP_SHIELD = 60.0f, CAP_SPEED = 340.0f,
                CAP_DASH = 220.0f, CAP_DAMAGE = 60.0f, CAP_RATE = 14.0f,
                CAP_RANGE = 900.0f, CAP_BATTERY = 16.0f;

inline int pip5(float value, float cap) {
    if (cap <= 0.0f) return 0;
    return std::clamp(static_cast<int>(std::lround(5.0 * value / cap)), 0, 5);
}

inline std::string pip_text(int filled) {
    std::string out;
    for (int i = 0; i < 5; ++i) out += (i < filled) ? "●" : "○";
    return out;
}

inline std::string num(float v) {
    // integers print bare; one decimal otherwise
    if (std::fabs(v - std::lround(v)) < 0.05f) return std::to_string(static_cast<long>(std::lround(v)));
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f", static_cast<double>(v));
    return std::string(buf);
}

/// The 8 rows: 4 drone stats (spec: hull/shield/speed/dash distance) then the
/// 4 installed-weapon stats (damage/fire rate/range/recharge).
inline std::vector<StatRow> rows(const ShipDef& ship, const WeaponDef& weapon,
                                 const DashConfig& dash) {
    const float dash_dist = dash.speed * ship.dash_mult * dash.duration;
    const float range = weapon.stats.projectile_speed * weapon.stats.projectile_lifetime;
    return {
        {"HULL " + num(ship.hull),                    pip5(ship.hull, CAP_HULL)},
        {"SHIELD " + num(ship.shield),                pip5(ship.shield, CAP_SHIELD)},
        {"SPEED " + num(ship.speed),                  pip5(ship.speed, CAP_SPEED)},
        {"DASH " + num(dash_dist) + "px",             pip5(dash_dist, CAP_DASH)},
        {"DAMAGE " + num(weapon.stats.damage),        pip5(weapon.stats.damage, CAP_DAMAGE)},
        {"FIRE RATE " + num(weapon.stats.fire_rate) + "/s",
                                                      pip5(weapon.stats.fire_rate, CAP_RATE)},
        {"RANGE " + num(range) + "px",                pip5(range, CAP_RANGE)},
        {"BATTERY " + num(weapon.fire_time) + "s",    pip5(weapon.fire_time, CAP_BATTERY)},
    };
}

}  // namespace hangar

#endif  // HANGAR_STATS_HPP
