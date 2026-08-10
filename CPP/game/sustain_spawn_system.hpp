#ifndef SUSTAIN_SPAWN_SYSTEM_HPP
#define SUSTAIN_SPAWN_SYSTEM_HPP

#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "arena_config.hpp"   // SustainConfig, ArenaConfig, EconomyConfig

/**
 * sustain_spawn — periodic health / shield pickups scattered around the arena
 * (#10, D56).
 *
 * On a fixed interval, place one Pickup of kind Health or Shield somewhere in the
 * arena and away from the drone, up to `max_live` at once. Collection is
 * PickupSystem's job — these ride the exact same component and the same
 * collection path as a currency drop, they simply are not dropped by anything.
 *
 * A free function rather than a class, following the `tick_shields` idiom: it
 * owns no state at all. The interval countdown and the placement counter live on
 * the Blackboard, which means there is no object to construct in main.cpp, no
 * seed to re-plumb, and nothing that can survive a restart in the wrong state.
 *
 * DETERMINISM (D56): there is no RNG here. Placements walk a golden-angle spiral
 * indexed by a plain integer counter, so the n-th placement of a run is at the
 * same point for every run, and no draw can be skipped, reordered or made
 * conditional — the failure mode the drop path has to be careful about (R2)
 * simply cannot occur. The counter resets when the wave number goes DOWN, which
 * is the one thing that only ever happens when spawn_world starts a fresh run.
 */

/// Blackboard keys this owns. Named constants so a test can drive the cadence
/// directly instead of sleeping through it.
namespace sustain_keys {
constexpr const char* TIMER = "sustain.timer";   // float, seconds to next placement
constexpr const char* COUNT = "sustain.count";   // int, placements so far this run
constexpr const char* WAVE  = "sustain.wave";    // int, last wave seen (restart probe)
}

void sustain_spawn(ComponentStorage& component_storage,
                   EntityManager& entity_manager,
                   Blackboard& blackboard,
                   const SustainConfig& cfg,
                   const ArenaConfig& arena,
                   const EconomyConfig& economy);

/**
 * The n-th placement's world point, before the "not in the player's lap" retry.
 * Exposed for the tests: it is the whole of the placement's determinism story and
 * is a pure function of (n, arena).
 */
void sustain_placement_point(int n, const ArenaConfig& arena, float& out_x, float& out_y);

/// True when the n-th placement should be a Shield rather than a Health pickup.
/// An exact Bresenham-style split: over N placements exactly floor(N*weight) are
/// shields, with no run of one kind longer than the weight implies.
bool sustain_is_shield(int n, float shield_weight);

#endif  // SUSTAIN_SPAWN_SYSTEM_HPP
