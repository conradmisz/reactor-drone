#ifndef DIRECTOR_SYSTEM_HPP
#define DIRECTOR_SYSTEM_HPP

#include <algorithm>
#include <cmath>

#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/components.hpp"
#include "arena_config.hpp"
#include "player_components.hpp"

/**
 * director_system — the Adaptive Director, an invisible pacing hand (#8, Lane Q, D142).
 *
 * A stress scalar integrated from what just happened to the player — damage
 * taken, kills landed, hull remaining — mapped onto ONE bounded multiplier over
 * wave-spawn *spacing*. After a near-death scramble the next spawns hold off a
 * beat; when the player is cruising untouched, pressure arrives early.
 *
 * WHAT IT DELIBERATELY CANNOT DO. It never changes a wave's enemy count, never
 * skips or reorders the authored table, and never chooses a type: the wave table
 * stays the single authority on what a run contains, and the director only moves
 * the seconds between spawns inside the authored bounds. That is what keeps it
 * *invisible* rather than a difficulty setting nobody asked for — and it is why
 * the whole feature is one multiply at the consumer.
 *
 * Determinism: a pure function of sim state and dt, no RNG. Spacing changes
 * *when* a spawn happens, never how many RNG draws a spawn takes, so the R2
 * discipline in `WaveSpawnerSystem::spawn_enemy` is untouched and a replay of a
 * seed is identical (verified against the canary).
 *
 * A free function in a header (the `tick_shields` idiom): three EMAs and a clamp,
 * with the whole state in a struct the hook block owns. Free on an MCU.
 */

/// One run's director state. Owned by the `director` hook block in main.cpp.
struct DirectorState {
    float damage_ema = 0.0f;   // recent hull lost, as a fraction of max, per second
    float kill_ema = 0.0f;     // recent kills per second
    float last_hull = -1.0f;   // hull last frame; -1 = no reading yet
    int   last_kills = 0;
    float stress = 0.0f;       // the published scalar, in [0,1]
    float mult = 1.0f;         // the published spacing multiplier
};

/**
 * Advance the director and return this frame's spawn-spacing multiplier.
 *
 * @param dt      the frame's dt as the sim sees it (already dilated by Lane P if
 *                bullet time is on — the director paces in *game* seconds, which
 *                is the same clock the spawner counts in).
 * @param active  false outside PHASE_PLAYING: a frozen shop must not decay the
 *                stress the player earned on the way in.
 */
inline float tick_director(ComponentStorage& cs, Blackboard& bb,
                           const DirectorConfig& cfg, DirectorState& st,
                           float dt, bool active) {
    if (!cfg.enabled) {
        st.mult = 1.0f;       // exact identity: the consumer's multiply is a no-op
        st.stress = 0.0f;
        bb.set<float>("director.stress", 0.0f);
        return 1.0f;
    }

    // --- read the sim ------------------------------------------------------
    float hull_frac = 1.0f;
    float hull_now = -1.0f;
    for (Entity p : cs.entities_with_component<PlayerTag>()) {
        auto h = cs.get_component<Health>(p);
        if (h.has_value() && h->get().max_hp > 0.0f) {
            hull_frac = std::min(1.0f, std::max(0.0f, h->get().current / h->get().max_hp));
            hull_now = hull_frac;
        }
        break;
    }
    const int kills = bb.get_or<int>("sim.kills", 0);

    if (!active || dt <= 0.0f) {
        // Hold, do not decay: the stress that opened the shop is the stress the
        // next wave should start from. The readings are still refreshed so a run
        // resumed at a different hull does not read as one huge instant hit.
        st.last_hull = hull_now;
        st.last_kills = kills;
        bb.set<float>("director.stress", st.stress);
        return st.mult;
    }

    // Hull LOST since last frame, as a fraction of max. A heal or a fresh run
    // (last_hull < 0, or hull going up) contributes zero rather than a negative
    // spike — relief is the kill term's job, not this one's.
    float lost = 0.0f;
    if (st.last_hull >= 0.0f && hull_now >= 0.0f && st.last_hull > hull_now)
        lost = st.last_hull - hull_now;
    st.last_hull = hull_now;

    const int fresh_kills = kills > st.last_kills ? kills - st.last_kills : 0;
    st.last_kills = kills;

    // --- EMAs --------------------------------------------------------------
    // Rate per second, smoothed. The 1-e^-kt form is frame-rate independent, so
    // the director paces the same at any dt — including under bullet time, where
    // dt shrinks but the game-seconds it represents are what the player feels.
    const float k = 1.0f - std::exp(-cfg.ema_per_sec * dt);
    st.damage_ema += (lost / dt - st.damage_ema) * k;
    st.kill_ema += (static_cast<float>(fresh_kills) / dt - st.kill_ema) * k;

    // --- stress ------------------------------------------------------------
    // Three terms, each already in "per unit" shape: damage taken is a fraction
    // of hull per second, missing hull is a fraction, kills are per second scaled
    // down to the same order. Weights are data so a playtest can retune the mix
    // without a rebuild.
    const float damage_term = cfg.damage_weight * std::min(1.0f, st.damage_ema * 2.0f);
    const float hull_term = cfg.hull_weight * (1.0f - hull_frac);
    const float relief = cfg.kill_weight * std::min(1.0f, st.kill_ema * 0.25f);
    st.stress = std::min(1.0f, std::max(0.0f, damage_term + hull_term - relief));

    // High stress => MORE spacing (the game breathes); low stress => less.
    st.mult = cfg.min_mult + (cfg.max_mult - cfg.min_mult) * st.stress;
    st.mult = std::min(std::max(st.mult, std::min(cfg.min_mult, cfg.max_mult)),
                       std::max(cfg.min_mult, cfg.max_mult));

    // Published for the systems that want the game's own read on how it is going:
    // the music intensity (Lane Z) and the grid's ambient hum are the intended
    // consumers. Render-side reads are free; a sim-side reader would have to hold
    // the same determinism line this function does.
    bb.set<float>("director.stress", st.stress);
    return st.mult;
}

#endif  // DIRECTOR_SYSTEM_HPP
