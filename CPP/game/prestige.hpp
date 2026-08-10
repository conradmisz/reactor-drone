#ifndef PRESTIGE_HPP
#define PRESTIGE_HPP

#include <string>

#include "arena_config.hpp"  // PlayerConfig

/**
 * Prestige — the opt-in restart that trades this run's upgrades for a permanent
 * hull buff (Iteration 5, Lane O, #14, D126-D129).
 *
 * The level is the ONLY thing persisted (`MetaSave::prestige`, alongside
 * lifetime score). Everything a level implies — hull, speed and damage
 * multipliers — is computed here from the level and never written to disk, the
 * same discipline that keeps ship unlocks derived from lifetime score (D80/D81):
 * two records of one fact desync the first time a percentage is retuned.
 *
 * "Strips upgrades" needs no code: shop purchases live on the per-run ShipState
 * and `spawn_world()` rebuilds it, so a prestige run starts with an empty sheet
 * by construction.
 *
 * DETERMINISM: unlike lifetime score, this value DOES reach the simulation — it
 * scales `config.player` at run start. A replay is therefore reproducible at a
 * FIXED prestige level, not unconditionally. `start_run` prints "Prestige: N" so
 * a headless run states the level it flew at (see the canary note in the merge
 * report and ENGINE.md §6a).
 */

/// Levels beyond this are ignored, so a hand-edited save cannot mint a god ship.
constexpr int PRESTIGE_MAX_LEVEL = 5;

// ponytail: the three rates are constants, not a `prestige` block in
// GameData.json — a JSON knob costs a GameConfig field plus a parse in
// arena_config.cpp, which is shared-lane surface. Promote them when they need
// tuning without a rebuild.
constexpr float PRESTIGE_HEALTH_PER_LEVEL = 0.10f;
constexpr float PRESTIGE_SPEED_PER_LEVEL  = 0.05f;
constexpr float PRESTIGE_DAMAGE_PER_LEVEL = 0.08f;

/// Blackboard key carrying the level of the run in progress. Lane M's stat
/// overview reads this (and calls `prestige_summary`); nothing writes it but
/// `start_run`.
inline constexpr const char* PRESTIGE_LEVEL_KEY = "prestige.level";

inline int prestige_clamp(int level) {
    if (level < 0) return 0;
    return level > PRESTIGE_MAX_LEVEL ? PRESTIGE_MAX_LEVEL : level;
}

struct PrestigeBonus {
    float health_mult = 1.0f;
    float speed_mult  = 1.0f;
    float damage_mult = 1.0f;
};

/// Linear, not compounding: level 5 is +50% hull, not 1.1^5. A player reading
/// "+10% per prestige" off the menu can then predict the number.
inline PrestigeBonus prestige_bonus(int level) {
    const float l = static_cast<float>(prestige_clamp(level));
    return PrestigeBonus{1.0f + PRESTIGE_HEALTH_PER_LEVEL * l,
                         1.0f + PRESTIGE_SPEED_PER_LEVEL * l,
                         1.0f + PRESTIGE_DAMAGE_PER_LEVEL * l};
}

/// Overlay the buff onto the player block. Applied at the ONE `start_run` site
/// where `apply_ship` and `apply_difficulty` already run, from the pristine
/// `base_config` — like both of those, this is NOT idempotent (D50).
inline void apply_prestige(PlayerConfig& player, int level) {
    const PrestigeBonus b = prestige_bonus(level);
    player.start_health   *= b.health_mult;
    player.move_speed     *= b.speed_mult;
    player.weapon.damage  *= b.damage_mult;
}

/// One line for a menu or a stat sheet, e.g.
/// "PRESTIGE 2  +20% HULL  +10% SPEED  +16% DAMAGE".
inline std::string prestige_summary(int level) {
    const int l = prestige_clamp(level);
    std::string s = "PRESTIGE " + std::to_string(l);
    if (l == 0) return s + "  (no bonus yet)";
    const PrestigeBonus b = prestige_bonus(l);
    auto pct = [](float mult) {
        return std::to_string(static_cast<int>((mult - 1.0f) * 100.0f + 0.5f));
    };
    return s + "  +" + pct(b.health_mult) + "% HULL  +" + pct(b.speed_mult)
             + "% SPEED  +" + pct(b.damage_mult) + "% DAMAGE";
}

#endif  // PRESTIGE_HPP
