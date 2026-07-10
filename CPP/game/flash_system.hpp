#ifndef FLASH_SYSTEM_HPP
#define FLASH_SYSTEM_HPP

#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"

/**
 * FlashSystem — drives the per-entity hit flash (v2, Phase 4).
 *
 * Producers (player-hit, enemy-hit) attach a Flash. Each frame this ticks
 * time_left down by delta_time and writes a Tint = feedback::flash_tint(flash)
 * onto the entity, so its colour fades from the flash colour back to identity.
 * When the flash expires it removes both the Flash and the Tint, leaving the
 * entity's normal appearance untouched (nothing else owns a persistent Tint).
 *
 * All state is per-entity component data — no RNG, no side channels — so replay
 * stays deterministic.
 */
class FlashSystem {
public:
    void update(ComponentStorage& storage, const Blackboard& blackboard);
};

#endif // FLASH_SYSTEM_HPP
