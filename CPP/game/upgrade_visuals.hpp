#ifndef UPGRADE_VISUALS_HPP
#define UPGRADE_VISUALS_HPP

#include <algorithm>
#include <cstdint>

#include "engine/ecs/component_storage.hpp"
#include "player_components.hpp"   // ShipState

/**
 * upgrade_visuals — "the purchase visibly changes the drone" (#7, D123).
 *
 * A shop purchase used to change only numbers. The drone now wears its upgrades
 * as its engine plume: every few purchases the exhaust steps from the stock cold
 * cyan toward a hot white-gold, and gets bigger, longer-lived and denser. The
 * shop's drone preview reads the same ramp, so the change is visible in the
 * frame the credits are spent as well as out in the arena.
 *
 * Pure free functions in a header (the `feedback.hpp` idiom): the ramp is
 * unit-testable with no renderer, and there is no state to own — the purchase
 * counts already live on ShipState.upg_counts.
 *
 * TIER 0 MUST MATCH the emitter main.cpp spawns, or a fresh drone would change
 * look on its first frame. That is asserted in test_lane_n.cpp.
 *
 * ponytail: a colour ramp on the existing emitter, not new art. The real ceiling
 * is a kitted-out hull sprite — that is Lane L's generator, and this lane must
 * not touch it. Swap `look_for` for a sprite index when that art exists.
 */
namespace upgrade_visuals {

/// Purchases per visible step, and the cap. 6 upgrade rows x a few buys each
/// means a full run lands around tier 3-4.
constexpr int PER_TIER = 3;
constexpr int MAX_TIER = 4;

/// Shop upgrades bought this run. Gear levels are excluded on purpose: fitted
/// gear already shows itself as the item aura (D65), so counting it twice would
/// make two different things drive one visual.
inline int bought(const ShipState& s) {
    int n = 0;
    for (int c : s.upg_counts) n += c;
    return n;
}

inline int tier(const ShipState& s) {
    return std::min(MAX_TIER, bought(s) / PER_TIER);
}

/// One step of the plume ramp.
struct Look {
    uint8_t start_r, start_g, start_b;
    uint8_t end_r, end_g, end_b;
    float emission_rate, start_size, particle_lifetime;
};

inline uint8_t mix(int a, int b, float t) {
    return static_cast<uint8_t>(static_cast<float>(a) +
                                (static_cast<float>(b - a)) * t);
}

/**
 * The plume at `t` = tier. Linear between the stock look (tier 0) and the fully
 * kitted one (MAX_TIER). Worst case is ~96/s at 0.62 s = ~60 live particles of
 * the 4000 budget (D84) — see ENGINE.md §5 before raising the rate.
 */
inline Look look_for(int tier_in) {
    const float t = static_cast<float>(std::clamp(tier_in, 0, MAX_TIER)) /
                    static_cast<float>(MAX_TIER);
    Look l{};
    l.start_r = mix(90, 255, t);  l.start_g = mix(220, 235, t); l.start_b = mix(255, 140, t);
    l.end_r   = mix(30, 200, t);  l.end_g   = mix(80,  90,  t); l.end_b   = mix(160, 20,  t);
    l.emission_rate     = 34.0f + 62.0f * t;
    l.start_size        = 5.0f  + 4.5f  * t;
    l.particle_lifetime = 0.4f  + 0.22f * t;
    return l;
}

/**
 * Write the current tier's plume onto the player's thruster emitter. Idempotent
 * and called every playing frame, so a resumed run (run_save restores
 * upg_counts) and a restart both land on the right look with no second
 * application site.
 *
 * The emission RATE is left alone while a dash is burning: tick_dash owns it for
 * the length of the burst and restores the value it captured, which is this one.
 */
inline void apply_to_player(ComponentStorage& storage, Entity player,
                            const ShipState& s) {
    auto em = storage.get_component<ParticleEmitter>(player);
    if (!em.has_value()) return;
    const Look l = look_for(tier(s));
    ParticleEmitter& e = em->get();
    e.start_r = l.start_r; e.start_g = l.start_g; e.start_b = l.start_b;
    e.end_r   = l.end_r;   e.end_g   = l.end_g;   e.end_b   = l.end_b;
    e.start_size = l.start_size;
    e.particle_lifetime = l.particle_lifetime;
    if (s.dash_timer <= 0.0f) e.emission_rate = l.emission_rate;
}

}  // namespace upgrade_visuals

#endif  // UPGRADE_VISUALS_HPP
