#ifndef FLASH_SYSTEM_HPP
#define FLASH_SYSTEM_HPP

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/components.hpp"   // Color, Tint
#include "engine/ecs/blackboard.hpp"
#include "enemy_components.hpp"        // EnemyTag, PathFollower (tint_phase)
#include "player_components.hpp"       // Flash

/**
 * FlashSystem — drives the per-entity hit flash (v2, Phase 4).
 *
 * Producers (player-hit, enemy-hit) attach a Flash. Each frame this ticks
 * time_left down by delta_time and writes a Tint = feedback::flash_tint(flash,
 * base) onto the entity, so its colour fades from the flash colour back to the
 * entity's resting tint.
 *
 * v2 Phase 5a: that resting tint is the entity's Color, for entities that own a
 * persistent one (enemies, which are luminance sprites coloured per arena).
 * Everything else keeps the pre-5a behaviour — fade to identity, then drop the
 * Tint entirely on expiry.
 *
 * All state is per-entity component data — no RNG, no side channels — so replay
 * stays deterministic.
 */
class FlashSystem {
public:
    void update(ComponentStorage& storage, const Blackboard& blackboard);
};

/**
 * hue_to_rgb — fully-saturated, full-value HSV->RGB for hue in [0,1).
 *
 * Only the tie-dye cycle needs it, and only at S=V=1, so this is the six-segment
 * ramp rather than a general colour-space conversion. Out-of-range hues wrap.
 */
inline void hue_to_rgb(float h, uint8_t& r, uint8_t& g, uint8_t& b) {
    h -= std::floor(h);
    float x = h * 6.0f;
    int seg = static_cast<int>(x) % 6;
    float f = x - std::floor(x);
    auto q = [](float v) -> uint8_t {
        return static_cast<uint8_t>(std::max(0.0f, std::min(1.0f, v)) * 255.0f + 0.5f);
    };
    uint8_t up = q(f), down = q(1.0f - f), full = 255, none = 0;
    switch (seg) {
        case 0: r = full; g = up;   b = none; break;
        case 1: r = down; g = full; b = none; break;
        case 2: r = none; g = full; b = up;   break;
        case 3: r = none; g = down; b = full; break;
        case 4: r = up;   g = none; b = full; break;
        default: r = full; g = none; b = down; break;
    }
}

/**
 * tick_enemy_tint — advance the tie-dye hue cycle (v2, Phase 5b).
 *
 * In the tie-dye arena every enemy's colour rotates through the spectrum at
 * 1/cycle_seconds Hz, offset by the per-enemy PathFollower::tint_phase that the
 * spawner rolled, so the swarm is a moving rainbow rather than one marching
 * colour. Writes both Color (the resting tint of record, which FlashSystem
 * restores) and Tint (what the renderer actually modulates by).
 *
 * A free function following the tick_shields() / tick_buff() idiom rather than a
 * system class: it is one loop over one component and owns no state.
 *
 * Call it *before* FlashSystem::update, so a hit flash written this frame still
 * wins for its 0.12s and then fades back to the freshly-cycled hue.
 */
inline void tick_enemy_tint(ComponentStorage& storage, float dt, float cycle_seconds) {
    if (cycle_seconds <= 0.0f) return;
    for (Entity e : storage.entities_with_component<EnemyTag>()) {
        auto pf = storage.get_component<PathFollower>(e);
        if (!pf.has_value()) continue;
        float& phase = pf->get().tint_phase;
        phase += dt / cycle_seconds;
        phase -= std::floor(phase);

        uint8_t r = 255, g = 255, b = 255;
        hue_to_rgb(phase, r, g, b);
        storage.add_component<Color>(e, Color{r, g, b, 255});
        // Don't stomp a live flash — FlashSystem owns Tint while one is running,
        // and will fade back to the Color just written.
        if (!storage.has_component<Flash>(e))
            storage.add_component<Tint>(e, Tint{r, g, b, 255, false});
    }
}

#endif // FLASH_SYSTEM_HPP
