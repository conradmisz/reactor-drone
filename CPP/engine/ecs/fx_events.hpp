#ifndef FX_EVENTS_HPP
#define FX_EVENTS_HPP

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "blackboard.hpp"

/**
 * fx_events — the shared render-FX event vocabulary (engine-suite Phase 0, D138;
 * revised by the playtest batch, D151).
 *
 * Sim-side systems PUBLISH combat moments as entries in two per-frame Blackboard
 * lists; render-only and passive consumers READ them while drawing.
 *
 * `impulses` are the BIG events only — a bomb going off, a pillar coming down, a
 * boss doing something — because the resonance grid is now an event display
 * rather than an ambient one (D151). `kill_marks` are every kill, consumed by the
 * flight report. The contract that keeps the replay canary honest:
 *
 *   - publishers append during the sim half of the frame;
 *   - consumers only read — nothing sim-side may ever read these lists back;
 *   - clear_frame() wipes both lists once at the top of every frame, whether or
 *     not any consumer is enabled, so a disabled feature cannot leak memory.
 *
 * Engine-side because the consumers are engine systems; the entries carry world
 * coordinates and magnitudes only — nothing Reactor-Drone-specific.
 */
namespace fx_events {

/// One radial impulse into the resonance grid (world coords, bottom-left origin).
struct Impulse {
    float x = 0.0f, y = 0.0f;
    float strength = 1.0f;   // publisher-scaled; the grid config maps it to force
};

/// One kill, recorded where it happened. Was the battle-scar layer's stamp
/// (D145); that layer was cut after the playtest — the arena floats in space, so
/// scorch marks on "the floor" never made sense — and the flight report is now
/// the only consumer. Kept as a list rather than folded into the report because
/// the deferred ghost/replay family wants the same events.
struct Mark {
    float x = 0.0f, y = 0.0f;
    int kind = 0;            // 0 = enemy kill; room for player deaths later
    float scale = 1.0f;
};

inline const std::string GRID_IMPULSES = "fx.grid_impulses";  // std::vector<Impulse>
inline const std::string KILL_MARKS    = "fx.kill_marks";     // std::vector<Mark>

/**
 * Hard per-frame cap on each list, and it is load-bearing twice over.
 *
 * MCU headroom: a bounded buffer is the rule for everything in this suite, and
 * the consumers are bounded work per entry (a lattice kick, a blit).
 *
 * Cost: the Blackboard stores `std::any` by value, so each push is a copy-out,
 * append, copy-in — O(n) per push, O(n^2) per frame. Bounded at 64 that is
 * nothing; unbounded it would grow with the worst frame in the game (the 30 s
 * stall force-kill wipes a whole wave in one frame, ~96 deaths at wave 20).
 *
 * Overflow is DROPPED, not queued: one frame's worth of ripples is a visual
 * moment, and a queued backlog would ring the lattice for explosions that already
 * finished.
 */
inline constexpr size_t MAX_PER_FRAME = 64;

inline void clear_frame(Blackboard& bb) {
    bb.set(GRID_IMPULSES, std::vector<Impulse>{});
    bb.set(KILL_MARKS, std::vector<Mark>{});
}

inline void push_impulse(Blackboard& bb, float x, float y, float strength) {
    auto v = bb.get_or<std::vector<Impulse>>(GRID_IMPULSES, {});
    if (v.size() >= MAX_PER_FRAME) return;
    v.push_back({x, y, strength});
    bb.set(GRID_IMPULSES, std::move(v));
}

inline void push_mark(Blackboard& bb, float x, float y, int kind,
                      float scale = 1.0f) {
    auto v = bb.get_or<std::vector<Mark>>(KILL_MARKS, {});
    if (v.size() >= MAX_PER_FRAME) return;
    v.push_back({x, y, kind, scale});
    bb.set(KILL_MARKS, std::move(v));
}

}  // namespace fx_events

#endif  // FX_EVENTS_HPP
