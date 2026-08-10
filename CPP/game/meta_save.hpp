#ifndef META_SAVE_HPP
#define META_SAVE_HPP

#include <string>
#include <vector>

#include "arena_config.hpp"  // ShipDef

/**
 * MetaSave — the only state that outlives a run (Lane F, D80).
 *
 * Deliberately one number. This is NOT the run-state save/load from the older
 * plan: there is no entity snapshot, no SerializationRegistry and no
 * LoadSystem. Lifetime score is the whole progression currency, and which ships
 * are unlocked is *derived* from it against each ShipDef::unlock_score (D81) —
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

/// A ship is unlocked once lifetime score reaches its threshold (>=, so exactly
/// 4000 unlocks the 4000-point ship).
inline bool ship_unlocked(const ShipDef& s, long long lifetime_score) {
    return lifetime_score >= static_cast<long long>(s.unlock_score);
}

/// How many of `ships` are currently unlocked (the title menu shows a selector
/// only when this exceeds 1).
inline int unlocked_ship_count(const std::vector<ShipDef>& ships, long long lifetime_score) {
    int n = 0;
    for (const ShipDef& s : ships) if (ship_unlocked(s, lifetime_score)) ++n;
    return n;
}

/// Next unlocked ship index after `cur`, wrapping. Returns `cur` when nothing
/// else is unlocked, which is how a locked ship stays unselectable — the cycle
/// button simply cannot land on one.
inline int next_unlocked_ship(const std::vector<ShipDef>& ships, int cur, long long lifetime_score) {
    const int n = static_cast<int>(ships.size());
    if (n <= 0) return 0;
    for (int step = 1; step <= n; ++step) {
        const int i = (cur + step) % n;
        if (ship_unlocked(ships[static_cast<size_t>(i)], lifetime_score)) return i;
    }
    return cur;
}

#endif  // META_SAVE_HPP
