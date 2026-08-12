#ifndef FX_EVENTS_HPP
#define FX_EVENTS_HPP

#include <string>
#include <vector>

#include "blackboard.hpp"

/**
 * fx_events — the shared render-FX event vocabulary (engine-suite Phase 0, D138).
 *
 * Sim-side systems PUBLISH combat moments (deaths, dashes, blasts) as entries in
 * two per-frame Blackboard lists; render-only consumers (the resonance grid, the
 * battle-scar layer) READ them while drawing. The contract that keeps the replay
 * canary honest:
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

/// One permanent mark stamped into the battle-scar layer.
struct Stamp {
    float x = 0.0f, y = 0.0f;
    int kind = 0;            // index into the scar stamp table (data-defined)
    float scale = 1.0f;
    float angle = 0.0f;      // radians
};

inline const std::string GRID_IMPULSES = "fx.grid_impulses";  // std::vector<Impulse>
inline const std::string SCAR_STAMPS   = "fx.scar_stamps";    // std::vector<Stamp>

inline void clear_frame(Blackboard& bb) {
    bb.set(GRID_IMPULSES, std::vector<Impulse>{});
    bb.set(SCAR_STAMPS, std::vector<Stamp>{});
}

inline void push_impulse(Blackboard& bb, float x, float y, float strength) {
    auto v = bb.get_or<std::vector<Impulse>>(GRID_IMPULSES, {});
    v.push_back({x, y, strength});
    bb.set(GRID_IMPULSES, v);
}

inline void push_stamp(Blackboard& bb, float x, float y, int kind,
                       float scale = 1.0f, float angle = 0.0f) {
    auto v = bb.get_or<std::vector<Stamp>>(SCAR_STAMPS, {});
    v.push_back({x, y, kind, scale, angle});
    bb.set(SCAR_STAMPS, v);
}

}  // namespace fx_events

#endif  // FX_EVENTS_HPP
