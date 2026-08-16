#ifndef META_SAVE_HPP
#define META_SAVE_HPP

#include <map>
#include <string>
#include <vector>

#include "arena_config.hpp"  // ShipDef

/**
 * MetaSave — the only state that outlives a run (Lane F, D80).
 *
 * Deliberately one number. This is NOT the run-state save/load from the older
 * plan: there is no entity snapshot, no SerializationRegistry and no
 * LoadSystem. Lifetime score is the whole progression currency, and which ships
 * are owned is *derived* from purchases against each ShipDef (D81, D221) —
 * storing an unlock list as well would let the file disagree with GameData.json
 * the first time a threshold is retuned.
 *
 * DETERMINISM: nothing in here may reach the simulation. The file is read once
 * at startup to decide what the title menu offers, and written once at run end.
 * The chosen ship is deliberately *not* persisted, so a replay of a given seed
 * is identical whether or not a save file exists.
 */
struct MetaSave {
    long long lifetime_score = 0;
    /// Iteration 5 (Lane O, D127): how many times the player has finished the arc
    /// and chosen to restart stronger. The level is stored because it records a
    /// CHOICE and nothing else can derive it; what it *implies* — the hull/speed/
    /// damage multipliers in prestige.hpp — is computed, never stored, so the two
    /// can never disagree (D81's rule, applied to the one value that needs it).
    ///
    /// This one DOES reach the simulation, unlike lifetime score: it scales
    /// `config.player` at run start, so a replay is reproducible at a fixed level.
    int prestige = 0;

    /// Main-menu-suite Phase C: the records screen. Pure bookkeeping, updated
    /// where scores bank, read only to write labels — never reaches the sim.
    int best_wave = 0;      ///< 1-based highest wave reached across all runs
    long long runs_played = 0;

    /// Distribution/leaderboard (Task 7): identity rides this same file rather
    /// than a separate profile — one garbage-tolerant load path, not two.
    /// `player_id` is generated once at first startup and persisted immediately
    /// (even before a name is ever registered) so it is stable across relaunches.
    /// None of the three reach the simulation — same DETERMINISM rule as the
    /// rest of this struct.
    std::string player_id;
    std::string player_name;
    bool registered = false;

    /// Gameplay pack (D221/D222): persistent progression currency and the
    /// hangar loadout. `scrap` and `owned_ships` record purchases — CHOICES,
    /// the prestige rule, so they are stored; everything derivable (which
    /// weapons/colors the player has) is derived from ownership instead
    /// (D81's rule). `equipped_ship`/`equipped_weapon` DO reach the sim, like
    /// prestige: a replay is reproducible at a fixed loadout, and headless
    /// canary runs clear saves/ first so they always fly the defaults.
    int scrap = 0;
    std::vector<std::string> owned_ships;    // bought ships, by ShipDef::name
    std::string equipped_ship;               // "" = first non-locked cost-0 ship
    std::string equipped_weapon;             // "" = equipped ship's default weapon

    /// Tier 7 (D221): cosmetics. `owned_cosmetics` records shop purchases only
    /// (ship-granted paints derive). The three maps are per-item slots — the
    /// spec's "each drone has 2 cosmetic slots, each weapon 1": key = ship or
    /// weapon name, value = color name; an absent key = the item's own paint.
    std::vector<std::string> owned_cosmetics;
    std::map<std::string, std::string> ship_colors;
    std::map<std::string, std::string> trail_colors;
    std::map<std::string, std::string> proj_colors;
};

/// Absolute path of the meta-save (`<project root>/saves/meta.json`).
std::string meta_save_path();

/// Load, tolerating everything: missing file, unreadable file, malformed JSON,
/// wrong types. Any failure yields a default MetaSave — a bad save never kills
/// a run.
MetaSave meta_load(const std::string& path);

/// Write, creating the `saves/` directory if needed. False on any failure;
/// callers ignore it, because failing to record progress must not stop the game.
bool meta_write(const std::string& path, const MetaSave& m);

/// A fresh player id: 16 std::random_device bytes, formatted as hex 8-4-4-4-12
/// (36 chars incl. dashes). Not claimed to be a spec-compliant v4 UUID — just
/// unique enough to key a player row, generated once and persisted forever.
std::string generate_uuid();

/// Gameplay pack (D221 call #1): ownership replaces the lifetime-score unlock.
/// A cost-0 ship is always owned; anything else must have been bought; a
/// `locked` ship (the unreleased 4th drone) is owned by no one.
inline bool ship_owned(const MetaSave& m, const ShipDef& s) {
    if (s.locked) return false;
    if (s.scrap_cost <= 0) return true;
    for (const std::string& n : m.owned_ships) if (n == s.name) return true;
    return false;
}

/// How many of `ships` the player owns (the selector shows only when > 1).
inline int owned_ship_count(const std::vector<ShipDef>& ships, const MetaSave& m) {
    int n = 0;
    for (const ShipDef& s : ships) if (ship_owned(m, s)) ++n;
    return n;
}

/// Next owned ship index after `cur`, wrapping. Returns `cur` when nothing else
/// is owned — the cycle button simply cannot land on a ship you don't own.
inline int next_owned_ship(const std::vector<ShipDef>& ships, int cur, const MetaSave& m) {
    const int n = static_cast<int>(ships.size());
    if (n <= 0) return 0;
    for (int step = 1; step <= n; ++step) {
        const int i = (cur + step) % n;
        if (ship_owned(m, ships[static_cast<size_t>(i)])) return i;
    }
    return cur;
}

/// A weapon is owned iff some owned ship grants it (derived, never stored —
/// D81). Standalone weapon purchases don't exist yet; add storage when they do.
inline bool weapon_owned(const MetaSave& m, const std::vector<ShipDef>& ships,
                         const std::string& weapon_name) {
    for (const ShipDef& s : ships)
        if (ship_owned(m, s) && s.default_weapon == weapon_name) return true;
    return false;
}

/// Tier 7 (D221): a paint is owned when its granting ship is owned or it was
/// bought in the cosmetic shop (derive-what-you-can, D81).
inline bool color_owned(const MetaSave& m, const std::vector<ShipDef>& ships,
                        const CosmeticColorDef& c) {
    if (!c.granted_by.empty()) {
        for (const ShipDef& s : ships)
            if (s.name == c.granted_by) return ship_owned(m, s);
        return false;
    }
    if (c.price <= 0) return true;
    for (const std::string& n : m.owned_cosmetics) if (n == c.name) return true;
    return false;
}

/// The equipped color for `item` in one slot map, or -1 (item's own paint).
inline int equipped_color(const MetaSave& m,
                          const std::map<std::string, std::string>& slot,
                          const std::vector<ShipDef>& ships,
                          const std::vector<CosmeticColorDef>& colors,
                          const std::string& item) {
    auto it = slot.find(item);
    if (it == slot.end()) return -1;
    const int i = find_color(colors, it->second);
    if (i < 0 || !color_owned(m, ships, colors[static_cast<size_t>(i)])) return -1;
    return i;
}

#endif  // META_SAVE_HPP
