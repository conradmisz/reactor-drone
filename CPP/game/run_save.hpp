#ifndef RUN_SAVE_HPP
#define RUN_SAVE_HPP

#include <string>

#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/component_storage.hpp"

/**
 * RunSave — save & quit mid-run (Lane K, D100).
 *
 * This is the SECOND save file and a different concern from `meta_save.*`
 * (D80: `saves/meta.json`, one lifetime-score number that outlives every run).
 * This one lives at `saves/run.json` and holds a run *in progress*.
 *
 * RUN STATE ONLY — there is no entity-graph snapshot, no SerializationRegistry
 * and no LoadSystem (D100). What is stored is the handful of numbers a run is
 * built from: which wave, which difficulty, which hull, what the drone is
 * carrying and how hurt it is. Resuming re-runs the ordinary run-start path and
 * then overlays these numbers onto the freshly built world, so there is exactly
 * one place that knows how to construct a run.
 *
 * DETERMINISM: nothing here may reach a *fresh* run's simulation. The file is
 * read once at startup, purely to decide whether the title screen offers
 * CONTINUE, and is applied only on the resume path (the same discipline the
 * meta-save keeps, D80-D83). A save file that merely exists cannot move a single
 * RNG draw.
 */
struct RunSave {
    static constexpr int CURRENT_VERSION = 1;

    bool present = false;      ///< false = no usable save (missing/corrupt/wrong version)
    int  version = CURRENT_VERSION;

    // How the run was started — everything `start_run` needs.
    unsigned int seed = 0;
    int  difficulty = 0;       ///< index into GameConfig::difficulties
    std::string difficulty_name;
    int  ship_id = 0;
    int  wave = 0;             ///< 0-based wave index to resume at
    int  score = 0;

    // What the drone is, right now.
    float hull = 0.0f, hull_max = 0.0f;
    float shield = 0.0f, shield_max = 0.0f, shield_regen = 0.0f, shield_delay = 0.0f;
    int   credits = 0, keys = 0;
    float speed_mult = 1.0f;
    int   item_id = -1, consumable_id = -1;
    int   active_id = -1;
    int   extra_shots = 0;
    int   upg_counts[8] = {0};
    int   gear_levels[8] = {0};

    // The gun, stored as the numbers it ended up at rather than as a purchase
    // history to replay: the shop's apply-an-upgrade path is another lane's file,
    // and a stat is what the run actually has.
    float fire_rate = 0.0f, damage = 0.0f;
    float projectile_speed = 0.0f, projectile_lifetime = 0.0f, spread = -1.0f;
};

/// Absolute path of the run save (`<project root>/saves/run.json`). Deliberately
/// a different filename from `meta.json`, which shares the directory.
std::string run_save_path();

/// Load, tolerating everything: missing file, unreadable file, malformed JSON,
/// wrong shape, wrong types, negative numbers, a future version. Any failure
/// yields `present == false` and a defaulted struct — a bad save must never be
/// able to stop the game from starting or from running.
RunSave run_save_load(const std::string& path);

/// Write, creating `saves/` if needed. False on any failure; callers may ignore
/// it, because failing to save must not take the run down.
bool run_save_write(const std::string& path, const RunSave& s);

/// Delete the run save, if any. Called when a run *ends* (death or victory) so
/// CONTINUE can never resurrect a finished run. Quitting from the pause menu
/// deliberately does NOT clear it — the player may have just pressed SAVE.
void run_save_clear(const std::string& path);

/// Read the live run out of the world. `wave` is the spawner's 0-based index.
RunSave run_save_capture(ComponentStorage& storage, const Blackboard& blackboard,
                         int wave, int difficulty, const std::string& difficulty_name,
                         int ship_id, unsigned int seed);

/// Overlay a loaded run onto a world that `spawn_world()` has just rebuilt.
/// Defensive by construction: a field that is missing or nonsense in the file
/// leaves the freshly-spawned value alone, so a half-written save resumes as a
/// slightly generous run rather than as a dead drone.
void run_save_apply(const RunSave& s, ComponentStorage& storage, Blackboard& blackboard);

#endif  // RUN_SAVE_HPP
