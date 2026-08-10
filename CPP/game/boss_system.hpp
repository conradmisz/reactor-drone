#ifndef BOSS_SYSTEM_HPP
#define BOSS_SYSTEM_HPP

#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "arena_config.hpp"
#include "wave_spawner_system.hpp"
#include <vector>

/**
 * BossSystem — the every-10th-wave boss and its reward (#4, D70/D71/D72).
 *
 * A wave flagged `boss` spawns exactly one boss: big, slow, high HP, summoning
 * adds on a timer, themed to whichever arena is live — the arena's enemy tint and
 * one signature attack borrowed from that arena's specialty unit, so the Foundry
 * boss mines and the Bio-lab boss spits.
 *
 * **How the wave clears.** BossSystem raises WaveSpawnerSystem::set_clear_hold
 * the frame it spawns the boss and drops it only once the reward has been taken.
 * The spawner therefore keeps spawning the wave's ordinary enemies but never
 * *finishes* the wave, which is both "the wave clears when the boss dies" and the
 * reason the 30 s straggler force-kill cannot execute the boss. Without the hold,
 * the reward screen would be pushed one frame after the phase machine had already
 * switched to PHASE_INTERMISSION — where this system does not run, so the pick
 * could never be handled (D70).
 *
 * Every countdown here is a float, and the adds spawn on fixed angles: a boss
 * fight makes no RNG draws at all, so it cannot shift the replay stream.
 */
class BossSystem {
public:
    void set_config(const GameConfig* cfg) { cfg_ = cfg; }

    /// One frame, from `// === HOOK: boss ===` (straight after the spawner).
    void update(ComponentStorage& storage, EntityManager& entity_manager,
                Blackboard& blackboard, WaveSpawnerSystem& spawner);

    /// True while a boss is alive on the board.
    bool boss_alive() const { return boss_alive_; }
    /// True while the reward screen is up and the wave is held for it.
    bool reward_open() const { return reward_open_; }

    /**
     * SEAM (see boss_system.cpp): the wave-50 boss asks for the mid-fight arena
     * shift into the Singularity map once it drops below BossConfig::shift_hp_frac.
     * Latching, so a caller that polls it every frame acts exactly once.
     * NOTHING CONSUMES THIS YET — the crossfade is Lane E's and is not callable
     * mid-wave. The integrator wires it after Lane E merges.
     */
    bool wants_arena_shift() const { return shift_requested_; }
    void clear_arena_shift_request() { shift_requested_ = false; }

    /// The catalogue rows currently offered on the reward screen (indices into
    /// GameConfig::actives). Exposed for the contract test.
    const std::vector<int>& offer() const { return offer_; }

    void reset();

private:
    void spawn_boss(ComponentStorage& storage, EntityManager& entity_manager,
                    Blackboard& blackboard, int wave, int boss_index, bool final_boss);
    void open_reward(ComponentStorage& storage, Blackboard& blackboard);
    bool handle_reward_click(ComponentStorage& storage, Blackboard& blackboard);

    const GameConfig* cfg_ = nullptr;
    int spawned_wave_ = -1;        // the boss wave this system has already spawned
    Entity boss_ = 0;
    bool boss_alive_ = false;
    bool reward_open_ = false;
    bool shift_requested_ = false;
    bool final_boss_ = false;
    std::vector<int> offer_;
};

#endif  // BOSS_SYSTEM_HPP
