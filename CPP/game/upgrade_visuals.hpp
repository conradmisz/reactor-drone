#ifndef UPGRADE_VISUALS_HPP
#define UPGRADE_VISUALS_HPP

#include <algorithm>
#include <cmath>
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

/**
 * ===========================================================================
 * The upgrade kit (D133) and the shield field (D134)
 * ===========================================================================
 *
 * D123 shipped the plume ramp with the note "the real ceiling is a kitted-out
 * hull sprite". This is that ceiling: the chassis was redesigned with authored
 * hardpoints (empty flank rails and a tail socket), and each shop upgrade row
 * has an overlay that seats into one of them.
 *
 * The overlays are FOLLOWER ENTITIES, not baked variants: max_stacks runs 2..8
 * across the eight rows, so baking every combination is ~1.6M sprites. Each
 * part is a single-frame Images wearer authored in the chassis's own 128-space,
 * so it composites 1:1 at whatever size the player is drawn at.
 *
 * Everything here is a pure function of ShipState — no RNG, no accumulated
 * state of its own — so the kit cannot move the replay canary.
 */

/// One overlay, and the shop upgrade row that turns it on. Index-aligned with
/// GameData.json's shop.upgrades EXCEPT row 1 (Shield Capacitor), which has no
/// static part: a shield has live state, so it is the field ring below.
struct KitPart {
    int row;                 // index into ShipState.upg_counts
    const char* image;       // texture, relative to assets/images/
};

constexpr int KIT_COUNT = 7;

inline constexpr KitPart KIT[KIT_COUNT] = {
    {0, "v2/kit_plating.png"},     // Hull Plating   — flank rails
    {2, "v2/kit_thruster.png"},    // Aux Thruster   — tail corners
    {3, "v2/kit_heatsink.png"},    // Overclock      — tail socket
    {4, "v2/kit_drums.png"},       // Heavy Rounds   — spine + barrel collar
    {5, "v2/kit_twin.png"},        // Twin Barrel    — outboard barrels
    {6, "v2/kit_longbarrel.png"},  // Long Barrel    — centre barrel
    {7, "v2/kit_coils.png"},       // Ricochet Coils — muzzle rings
};

/// Is part `i` worn? One purchase in its row is enough — the part shows what you
/// own, and the plume ramp (above) already shows how much.
inline bool part_worn(const ShipState& s, int i) {
    if (i < 0 || i >= KIT_COUNT) return false;
    const int row = KIT[i].row;
    if (row < 0 || row >= 8) return false;
    return s.upg_counts[row] > 0;
}

// --- The shield field -------------------------------------------------------
//
// FRAME LAYOUT — must match shield_frames() in make_sprites.py:
//   0..7   hum     the living field, phase-looped
//   8..11  hit     impact bloom decaying
//   12     down    broken: dead emitter stubs
//   13..20 regen   rebuilding, indexed by FRACTION not by time
//
// There is deliberately no Animation component: four different behaviours (a
// loop, a one-shot, a static, and a progress bar) are one indexable strip and
// one picker, rather than four clips plus the machinery to switch between them.

constexpr int FIELD_HUM_START = 0,   FIELD_HUM_COUNT = 8;
constexpr int FIELD_HIT_START = 8,   FIELD_HIT_COUNT = 4;
constexpr int FIELD_DOWN_FRAME = 12;
constexpr int FIELD_REGEN_START = 13, FIELD_REGEN_COUNT = 8;
constexpr int FIELD_TOTAL = 21;

/// The field sprite is a 192px window on the chassis's 128-space (the extra 64
/// is the margin that stops the r=70 ring being clipped square by the frame),
/// so a 1.5x mult would draw 128-space at 1:1 and put the ring almost on the
/// hull. 2.25x reproduces the standoff the stretched art used to have.
constexpr float FIELD_SIZE_MULT = 288.0f / 128.0f;

/// How long after a hit the impact bloom plays.
constexpr float FIELD_HIT_TIME = 0.36f;

enum class FieldState { Hidden, Hum, Hit, Down, Regen };

/**
 * What the field is doing, from ShipState alone.
 *
 * `delay_total` is the configured quiet time (blackboard "ship.shield_regen_delay").
 * ShipState.shield_delay counts that DOWN, so a delay close to the total means
 * the hit landed a moment ago — which is exactly the bloom's window, with no
 * second timer to keep in sync.
 */
inline FieldState field_state(const ShipState& s, float delay_total) {
    if (s.shield_max <= 0.0f) return FieldState::Hidden;   // no capacitor bought
    if (delay_total > 0.0f && s.shield_delay > delay_total - FIELD_HIT_TIME)
        return FieldState::Hit;
    if (s.shield <= 0.0f) return FieldState::Down;
    if (s.shield < s.shield_max && s.shield_delay <= 0.0f) return FieldState::Regen;
    return FieldState::Hum;
}

/**
 * The frame to draw. `phase` is a free-running 0..1 loop (the hum and the bloom
 * read it); `frac` is shield/shield_max. Always returns a valid frame index.
 */
inline int field_frame(FieldState st, float frac, float phase) {
    const float p = phase - std::floor(phase);      // wrap, tolerate any input
    const float f = std::clamp(frac, 0.0f, 1.0f);
    switch (st) {
        case FieldState::Hidden:
            return FIELD_HUM_START;
        case FieldState::Down:
            return FIELD_DOWN_FRAME;
        case FieldState::Hit: {
            const int i = static_cast<int>(p * FIELD_HIT_COUNT);
            return FIELD_HIT_START + std::min(i, FIELD_HIT_COUNT - 1);
        }
        case FieldState::Regen: {
            const int i = static_cast<int>(f * FIELD_REGEN_COUNT);
            return FIELD_REGEN_START + std::min(i, FIELD_REGEN_COUNT - 1);
        }
        case FieldState::Hum:
        default: {
            const int i = static_cast<int>(p * FIELD_HUM_COUNT);
            return FIELD_HUM_START + std::min(i, FIELD_HUM_COUNT - 1);
        }
    }
}

}  // namespace upgrade_visuals

#endif  // UPGRADE_VISUALS_HPP
