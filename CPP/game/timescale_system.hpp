#ifndef TIMESCALE_SYSTEM_HPP
#define TIMESCALE_SYSTEM_HPP

#include <algorithm>
#include <cmath>

#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/components.hpp"
#include "arena_config.hpp"
#include "player_components.hpp"

/**
 * timescale_system — Temporal Overload, the suite's bullet-time (#1, Lane P, D139).
 *
 * A free function in a header (the `tick_shields` idiom), not a class: its whole
 * per-run state is one small struct the hook block owns, so there is nothing to
 * construct in main.cpp and nothing that can survive a restart holding a stale
 * scale.
 *
 * WHY THIS IS SIM-SIDE AND STILL DETERMINISTIC. The scale is a pure function of
 * sim state (a kill counter, the player's hull fraction) and of the *real* frame
 * dt, which under `--seed` is the fixed deterministic step. Frames still advance
 * exactly one per loop iteration — only the seconds each frame represents shrink.
 * So:
 *   - anything counting FRAMES is unaffected by design: scripted `--keys` are
 *     frame-indexed, `--stopframe` is a frame number, and the pause path's
 *     `end_frame_no_advance` is untouched;
 *   - anything counting SECONDS (wave delays, spawn intervals, cooldowns,
 *     lifetimes) slows down together, which is the feature;
 *   - the two are never mixed in a way that changes an outcome — the audit is in
 *     the D139 entry.
 * `enabled == false` returns exactly 1.0f (the `pulse_hz == 0` exact-identity
 * idiom), so a data-disabled build is byte-identical.
 */

/// The counter Temporal Overload reads to find a kill chain. Published by
/// EnemyDeathSystem on every kill; monotonic within a run, reset at start_run.
inline constexpr const char* TIMESCALE_KILL_KEY = "sim.kills";

/// One dilation's scratch. Owned by the `timescale` hook block in main.cpp.
struct TimescaleState {
    float scale = 1.0f;        // the multiplier applied last frame
    float hold = 0.0f;         // seconds left on the current kill-chain beat
    float chain_timer = 0.0f;  // seconds left on the open chain window
    int   chain_count = 0;     // kills inside that window
    int   last_kills = 0;      // kill counter as of last frame
};

/**
 * Advance the dilation and return this frame's timescale multiplier.
 *
 * @param real_dt the UNSCALED frame dt (this function must never be fed its own
 *                output, or the ease rate would itself dilate).
 * @param active  false outside PHASE_PLAYING — menus, the shop and the
 *                intermission always run at 1.0f.
 */
inline float tick_timescale(ComponentStorage& cs, const Blackboard& bb,
                            const TimescaleConfig& cfg, TimescaleState& st,
                            float real_dt, bool active) {
    // Exact identity when the feature is data-disabled or the sim is not playing.
    if (!cfg.enabled || !active) {
        st = TimescaleState{};
        st.last_kills = bb.get_or<int>(TIMESCALE_KILL_KEY, 0);
        return 1.0f;
    }

    // --- kill chain -------------------------------------------------------
    const int kills = bb.get_or<int>(TIMESCALE_KILL_KEY, 0);
    // A restart rewinds the counter; treat any decrease as a fresh run rather
    // than as a negative chain.
    const int fresh = kills > st.last_kills ? kills - st.last_kills : 0;
    st.last_kills = kills;

    st.chain_timer = std::max(0.0f, st.chain_timer - real_dt);
    if (st.chain_timer <= 0.0f) st.chain_count = 0;
    if (fresh > 0) {
        if (st.chain_count == 0) st.chain_timer = cfg.chain_window;
        st.chain_count += fresh;
    }
    if (cfg.chain_kills > 0 && st.chain_count >= cfg.chain_kills) {
        st.hold = cfg.kill_hold;   // the beat fires...
        st.chain_count = 0;        // ...and the window is spent, not re-triggered
        st.chain_timer = 0.0f;
    }
    st.hold = std::max(0.0f, st.hold - real_dt);

    // --- target scale -----------------------------------------------------
    float target = 1.0f;
    if (st.hold > 0.0f) target = std::min(target, cfg.kill_scale);

    // Hull-critical: the last sliver of hull dilates for as long as it lasts.
    for (Entity p : cs.entities_with_component<PlayerTag>()) {
        auto h = cs.get_component<Health>(p);
        if (h.has_value() && h->get().max_hp > 0.0f) {
            const float frac = h->get().current / h->get().max_hp;
            if (frac > 0.0f && frac <= cfg.hull_frac)
                target = std::min(target, cfg.hull_scale);
        }
        break;
    }
    target = std::max(target, cfg.min_scale);   // the sim never fully stops

    // --- ease -------------------------------------------------------------
    // Exponential ease, clamped so a long frame cannot overshoot past the target.
    const float k = std::min(1.0f, std::max(0.0f, cfg.ease_per_sec * real_dt));
    st.scale += (target - st.scale) * k;
    // Snap the last sliver so a run that never leaves 1.0f stays exactly there.
    if (std::fabs(st.scale - 1.0f) < 1e-4f) st.scale = 1.0f;
    return st.scale;
}

#endif  // TIMESCALE_SYSTEM_HPP
