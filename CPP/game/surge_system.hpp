#ifndef SURGE_SYSTEM_HPP
#define SURGE_SYSTEM_HPP

#include <random>
#include <string>
#include <vector>

#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "arena_config.hpp"
#include "force_field_system.hpp"

/**
 * surge_system — arena weather (#7, Lane X, D149).
 *
 * Mid-wave, the reactor *does something*: coolant floods a slice of the arena and
 * everything in it slows; a plasma arc sweeps a rotating line you must not touch;
 * a vent erupts; a gravity storm bends every flight line at once (through Lane
 * T's force layer). The wave loop's rhythm breaks and the player has to
 * reposition — the genre's monotony killer.
 *
 * DETERMINISM. The scheduler owns a PRIVATE `mt19937`, seeded from `config.seed`
 * at `start_run` — it never touches the spawner's or the loot table's stream, so
 * authoring a surge table cannot shift a single spawn or drop. Within its own
 * stream it still follows R2: **exactly three draws every time it ticks a wave**,
 * taken before any conditional, whether or not an event fires and whatever the
 * arena's table says. The conditional decides which draws are USED, never how
 * many are TAKEN (Invariant 4 / D18/D19), so retuning a `chance` value cannot
 * desync the surges of a later wave either.
 *
 * A surge that FIRES does spawn a carrier entity, which moves the entity-id
 * cursor — that is a real sim change, and it is exactly why the whole feature
 * stays inert until an arena authors a table.
 *
 * MCU headroom: at most `MAX_LIVE` events at once, region tests are circle and
 * half-plane checks, and the damage carriers are ordinary entities the existing
 * ContactDamage path already handles — no new damage system.
 */
class SurgeSystem {
public:
    /// Effects, resolved from the `effect` string — never a row index (D26).
    enum class Effect { SlowField, SweepLine, Eruption, GravityStorm, Unknown };

    static Effect effect_for(const std::string& s);

    /// Live events are bounded; the cap is code, not data, because it bounds the
    /// per-frame region tests as much as the fiction.
    static constexpr int MAX_LIVE = 2;

    struct Live {
        Effect effect = Effect::Unknown;
        float x = 0.0f, y = 0.0f;    // region centre (world)
        float radius = 160.0f;
        float magnitude = 1.0f;
        float remaining = 0.0f;      // seconds of ACTIVE life left
        float telegraph = 0.0f;      // seconds of warning left before it goes live
        float angle = 0.0f;          // sweep line's current angle (radians)
        // The damage carrier, when this effect has one. `has_carrier` is a
        // separate flag rather than `carrier == 0` meaning "none": EntityManager
        // hands out 0 as a perfectly good entity id, so the sentinel would make
        // the FIRST entity of a run invisible to the teardown and leave a hazard
        // box on the floor forever.
        Entity carrier = 0;
        bool has_carrier = false;
    };

    void set_config(const GameConfig* cfg) { cfg_ = cfg; }
    void set_seed(unsigned int seed) { rng_.seed(seed); }

    /// Drop every live event and its carrier. Called at run start and on an arena
    /// shift — a coolant flood must not survive the arena it flooded.
    void clear(ComponentStorage& storage, EntityManager& entity_manager);

    /**
     * One frame. `arena_index` selects the surge table; `wave` gates which rows
     * are eligible; `wave_changed` is the one edge the scheduler rolls on.
     * `forces` is where a gravity storm registers itself (Lane T's API).
     */
    void update(ComponentStorage& storage, EntityManager& entity_manager,
                Blackboard& blackboard, ForceFieldSystem& forces,
                int arena_index, int wave, bool wave_changed, float dt);

    int live_count() const { return static_cast<int>(live_.size()); }
    const std::vector<Live>& live() const { return live_; }
    /// Scheduler rolls taken this run — the R2 probe the tests assert on.
    int draws_taken() const { return draws_; }

private:
    void fire(ComponentStorage& storage, EntityManager& entity_manager,
              const SurgeDef& def, float roll_a, float roll_b);

    const GameConfig* cfg_ = nullptr;
    std::mt19937 rng_{1234u};
    std::vector<Live> live_;
    int draws_ = 0;
};

#endif  // SURGE_SYSTEM_HPP
