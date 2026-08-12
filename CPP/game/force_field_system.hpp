#ifndef FORCE_FIELD_SYSTEM_HPP
#define FORCE_FIELD_SYSTEM_HPP

#include <cstddef>
#include <vector>

#include "engine/ecs/component_storage.hpp"
#include "arena_config.hpp"

/**
 * force_field_system — attractors, repulsors and impulses (#3, Lane T, D144).
 *
 * A fixed-capacity array of field sources and ONE accumulation pass that turns
 * them into velocity deltas. Mass and force enter the game's vocabulary: a
 * gravity well that bends your flight line, a surge storm that drags a wave into
 * one killable clump.
 *
 * FRAME ORDER. The pass runs in the `forces` hook, immediately BEFORE
 * `movement.update`, so a field acts on the frame it exists. The arena circle
 * clamp and the obstacle push-out both run *after* movement and are unchanged, so
 * they still get the last word on position — no field can ever pull a body
 * through a wall, however strong it is.
 *
 * Determinism: pure math over sim state, no RNG, and a fixed iteration order
 * (sources in registration order, entities in storage order), so a replay of a
 * seed is identical. Inert by SHAPE rather than by a flag: with no registered
 * sources the pass iterates nothing, which is why there is no `enabled` bool.
 *
 * MCU headroom: capacity is data (`ForceConfig::max_sources`), the array is
 * allocated once and never grows, distances are compared squared, and the one
 * `sqrt` per (source, body) pair only happens inside a source's radius.
 */
class ForceFieldSystem {
public:
    /// One live field. `strength` is an acceleration in px/s^2: positive pulls
    /// toward the centre (a well), negative pushes away (a repulsor).
    struct Source {
        float x = 0.0f, y = 0.0f;
        float radius = 120.0f;
        float strength = 0.0f;
        float lifetime = 0.0f;      // seconds remaining; <= 0 expires this frame
        bool  affect_player = true; // a gravity storm bends the drone too
        bool  affect_enemies = true;
    };

    void set_capacity(int max_sources);

    /**
     * Register a field for `lifetime` seconds. Returns false when the array is
     * full — the caller's field simply does not exist this frame, which is the
     * bounded-buffer behaviour the MCU target wants. Never grows the array.
     */
    bool add_source(const Source& s);

    /// Ages every source and applies the accumulated acceleration to velocities.
    void update(ComponentStorage& component_storage, float dt);

    /// Drop every live field. Called at run start so a well cannot outlive a run.
    void clear() { sources_.clear(); }

    std::size_t live_sources() const { return sources_.size(); }
    std::size_t capacity() const { return capacity_; }

private:
    std::vector<Source> sources_;
    std::size_t capacity_ = 32;
};

#endif  // FORCE_FIELD_SYSTEM_HPP
