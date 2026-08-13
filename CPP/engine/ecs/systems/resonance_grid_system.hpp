#ifndef RESONANCE_GRID_SYSTEM_HPP
#define RESONANCE_GRID_SYSTEM_HPP

#include <cstdint>
#include <vector>

#include <SDL3/SDL.h>

#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/fx_events.hpp"

/**
 * ResonanceGridSystem — the arena as a physics display (engine suite, Lane R,
 * D140; revised after the first playtest, D151).
 *
 * A spring-mass lattice under the entities: a big event — a bomb, a collapsing
 * pillar, a boss volley — injects a radial impulse (`fx_events::Impulse`,
 * published sim-side) and the lattice ripples with damped integration, then pulls
 * itself flat and DISAPPEARS.
 *
 * The D151 revision is three changes, all from the same playtest note:
 *   1. **It covers the arena.** The lattice was a fixed 40x28 at 40 px = 1600 px
 *      across, against an arena 2800 px across — it stopped a third of the way to
 *      the wall. `cols`/`rows` of 0 now mean "derive from the arena span".
 *   2. **It is invisible at rest.** Resting alpha is zero and a strip with no
 *      displacement is not drawn at all, so there is no permanent mesh over the
 *      backdrop. This is what fixes the clutter, and it also disposes of the
 *      parallax-alignment complaint: the backdrops move at 0.25-0.9x the camera
 *      while the lattice is anchored in world space, so the two can only ever
 *      line up at one camera position — but you never see a resting lattice next
 *      to the backdrop grid to compare them against.
 *   3. **Only big events ring it.** An impulse per kill made it a permanent
 *      shimmer; ordinary deaths no longer publish one.
 *
 * RENDER-ONLY, and that is a hard contract: the grid *reads* sim events and
 * writes pixels. No sim system may read grid state, so it can never feed back
 * into the simulation and the replay canary cannot see it. It owns no RNG.
 *
 * MCU headroom: the lattice is one flat `std::vector` sized once in `configure()`
 * and never reallocated; the update is one pass of adds and multiplies with no
 * allocation, no transcendental per node, and no branch on entity data. Drawing
 * is one `SDL_RenderLines` strip per row and per column (68 calls at the default
 * 40x28), not one call per segment.
 */
class ResonanceGridSystem {
public:
    /// One lattice node: displacement from rest, and its velocity.
    struct Node { float dx = 0.0f, dy = 0.0f, vx = 0.0f, vy = 0.0f; };

    /**
     * Size the lattice and place it in the world. `origin_x/y` is the world
     * position of node (0,0); the lattice spans `(cols-1) * spacing` across.
     * Re-callable — a changed size rebuilds and zeroes the lattice.
     */
    void configure(int cols, int rows, float spacing, float origin_x, float origin_y);

    /**
     * Cover a circular arena, centred on it: picks the node counts from the span
     * and places the origin so the lattice is centred. This is the caller main.cpp
     * uses, so "how big is the grid" is answered by the arena rather than by two
     * numbers in a config block that silently stop matching it (D151).
     */
    void configure_for_arena(float center_x, float center_y, float radius,
                             float spacing);

    /// Spring/damping/impulse tuning and the line colour. Cheap; call per frame.
    void set_tuning(float stiffness, float damping, float impulse_scale,
                    float max_offset, uint8_t r, uint8_t g, uint8_t b, uint8_t a);

    /**
     * Advance the lattice one frame and inject this frame's impulses.
     *
     * Semi-implicit (symplectic) Euler: velocity is integrated first and position
     * from the *new* velocity, which is what keeps a stiff spring stable at the
     * frame rate rather than blowing up the way plain explicit Euler does here.
     * `dt` is clamped internally — a stalled frame must not launch the lattice.
     */
    void update(float dt, const std::vector<fx_events::Impulse>& impulses);

    /**
     * Draw the lattice. Reads `camera.lookat.x/y`, `camera.zoom`,
     * `window_width/height` for the same world→screen transform
     * `RenderSystem::render` uses, so the grid sits in the world, under the
     * entities, and shakes with the camera.
     */
    void render(SDL_Renderer* renderer, const Blackboard& blackboard) const;

    // --- inspection, for tests (no renderer required) ---
    int cols() const { return cols_; }
    int rows() const { return rows_; }
    const Node& node(int col, int row) const { return nodes_[index(col, row)]; }
    /// Sum of |displacement| over the lattice — the cheap "is anything moving" probe.
    float total_displacement() const;

private:
    size_t index(int col, int row) const {
        return static_cast<size_t>(row) * static_cast<size_t>(cols_) +
               static_cast<size_t>(col);
    }

    std::vector<Node> nodes_;
    mutable std::vector<SDL_FPoint> strip_;  // scratch for one row/column of points
    int cols_ = 0, rows_ = 0;
    float spacing_ = 40.0f;
    float origin_x_ = 0.0f, origin_y_ = 0.0f;
    float stiffness_ = 60.0f, damping_ = 5.0f;
    float impulse_scale_ = 90.0f, max_offset_ = 26.0f;
    uint8_t r_ = 90, g_ = 200, b_ = 255, a_ = 46;
};

#endif  // RESONANCE_GRID_SYSTEM_HPP
