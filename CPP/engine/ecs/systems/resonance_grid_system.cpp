#include "engine/ecs/systems/resonance_grid_system.hpp"

#include <algorithm>
#include <cmath>

namespace {

/// A frame longer than this is a stall (a debugger break, a texture load); the
/// lattice integrates it as a normal frame instead of launching itself.
constexpr float MAX_STEP = 1.0f / 30.0f;

/// Impulse falloff reach, in lattice spacings. Beyond it a node is untouched, so
/// the injection loop is O(affected nodes), not O(lattice) per impulse.
constexpr float IMPULSE_REACH_CELLS = 3.5f;

}  // namespace

void ResonanceGridSystem::configure(int cols, int rows, float spacing,
                                    float origin_x, float origin_y) {
    cols = std::max(0, cols);
    rows = std::max(0, rows);
    if (cols != cols_ || rows != rows_) {
        cols_ = cols;
        rows_ = rows;
        nodes_.assign(static_cast<size_t>(cols_) * static_cast<size_t>(rows_), Node{});
        strip_.resize(static_cast<size_t>(std::max(cols_, rows_)));
    }
    spacing_ = spacing;
    origin_x_ = origin_x;
    origin_y_ = origin_y;
}

void ResonanceGridSystem::configure_for_arena(float center_x, float center_y,
                                              float radius, float spacing) {
    if (!(spacing > 0.0f) || !(radius > 0.0f)) return;
    // +2 nodes of margin so the lattice reaches past the boundary ring rather
    // than stopping visibly short of it, which is how the undersized version
    // read in the playtest.
    const int n = static_cast<int>(std::ceil(2.0f * radius / spacing)) + 2;
    const float span = static_cast<float>(n - 1) * spacing;
    configure(n, n, spacing, center_x - span * 0.5f, center_y - span * 0.5f);
}

void ResonanceGridSystem::set_tuning(float stiffness, float damping,
                                     float impulse_scale, float max_offset,
                                     uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    stiffness_ = stiffness;
    damping_ = damping;
    impulse_scale_ = impulse_scale;
    max_offset_ = max_offset;
    r_ = r; g_ = g; b_ = b; a_ = a;
}

void ResonanceGridSystem::update(float dt,
                                 const std::vector<fx_events::Impulse>& impulses) {
    if (nodes_.empty() || spacing_ <= 0.0f) return;
    dt = std::min(std::max(dt, 0.0f), MAX_STEP);
    if (dt <= 0.0f) return;

    // --- inject this frame's impulses -------------------------------------
    // Each impulse kicks nearby nodes radially outward, falling off linearly to
    // zero at IMPULSE_REACH_CELLS. Linear rather than inverse-square: no divide,
    // no sqrt in the common case, and a hard zero at the edge means the loop
    // bounds are exact instead of "small enough to ignore".
    const float reach = IMPULSE_REACH_CELLS * spacing_;
    for (const fx_events::Impulse& imp : impulses) {
        const float lx = (imp.x - origin_x_) / spacing_;
        const float ly = (imp.y - origin_y_) / spacing_;
        const int c0 = static_cast<int>(std::floor(lx - IMPULSE_REACH_CELLS));
        const int c1 = static_cast<int>(std::ceil(lx + IMPULSE_REACH_CELLS));
        const int r0 = static_cast<int>(std::floor(ly - IMPULSE_REACH_CELLS));
        const int r1 = static_cast<int>(std::ceil(ly + IMPULSE_REACH_CELLS));
        for (int row = std::max(0, r0); row <= std::min(rows_ - 1, r1); ++row) {
            for (int col = std::max(0, c0); col <= std::min(cols_ - 1, c1); ++col) {
                const float nx = origin_x_ + static_cast<float>(col) * spacing_;
                const float ny = origin_y_ + static_cast<float>(row) * spacing_;
                float ox = nx - imp.x;
                float oy = ny - imp.y;
                const float d2 = ox * ox + oy * oy;
                if (d2 >= reach * reach) continue;
                const float d = std::sqrt(d2);
                // Dead centre: no direction to push, so the node is left to its
                // neighbours rather than given an arbitrary one.
                if (d < 1e-4f) continue;
                const float falloff = 1.0f - d / reach;
                const float kick = imp.strength * impulse_scale_ * falloff;
                ox /= d; oy /= d;
                Node& n = nodes_[index(col, row)];
                n.vx += ox * kick;
                n.vy += oy * kick;
            }
        }
    }

    // --- integrate --------------------------------------------------------
    // Semi-implicit Euler with a damping factor applied as a multiplicative decay
    // (exact for the frame, and unconditionally stable) rather than as a force.
    const float decay = 1.0f / (1.0f + damping_ * dt);
    for (Node& n : nodes_) {
        n.vx = (n.vx - stiffness_ * n.dx * dt) * decay;
        n.vy = (n.vy - stiffness_ * n.dy * dt) * decay;
        n.dx += n.vx * dt;
        n.dy += n.vy * dt;
        // Clamp per axis: a node may never leave its cell far enough for the
        // lattice to read as torn rather than as rippling.
        n.dx = std::min(max_offset_, std::max(-max_offset_, n.dx));
        n.dy = std::min(max_offset_, std::max(-max_offset_, n.dy));
    }
}

float ResonanceGridSystem::total_displacement() const {
    float sum = 0.0f;
    for (const Node& n : nodes_) sum += std::fabs(n.dx) + std::fabs(n.dy);
    return sum;
}

void ResonanceGridSystem::render(SDL_Renderer* renderer,
                                 const Blackboard& blackboard) const {
    if (renderer == nullptr || nodes_.empty() || a_ == 0) return;

    // The same world→screen transform RenderSystem::render applies, so the grid
    // is genuinely in the world: it tracks the follow camera and the screen shake.
    const float lookat_x = blackboard.get_or<float>("camera.lookat.x", 0.0f);
    const float lookat_y = blackboard.get_or<float>("camera.lookat.y", 0.0f);
    const float zoom = blackboard.get_or<float>("camera.zoom", 1.0f);
    const int win_w = blackboard.get_or<int>("window_width", 800);
    const int win_h = blackboard.get_or<int>("window_height", 600);
    if (zoom <= 0.0f) return;

    const float cam_left = lookat_x - (static_cast<float>(win_w) / zoom) * 0.5f;
    const float cam_bottom = lookat_y - (static_cast<float>(win_h) / zoom) * 0.5f;
    const float win_hf = static_cast<float>(win_h);

    auto to_screen = [&](float wx, float wy) {
        SDL_FPoint p;
        p.x = (wx - cam_left) * zoom;
        p.y = win_hf - (wy - cam_bottom) * zoom;   // the world Y-flip, once
        return p;
    };

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Per-strip alpha, and the strip is SKIPPED entirely when nothing has moved
    // it (D151). `a_` is the peak alpha of a fully-displaced strip, not a resting
    // one — at rest the lattice does not exist, which is what stops it reading as
    // a permanent mesh laid over the backdrop art.
    //
    // The curve is deliberately not linear: sqrt lifts a small ripple into
    // visibility quickly and then flattens, so the leading edge of a blast wave
    // reads as an edge rather than as a slow gradient.
    auto strip_alpha = [&](float peak) -> uint8_t {
        if (max_offset_ <= 0.0f) return 0;
        const float lit = std::min(1.0f, peak / max_offset_);
        if (lit <= 0.004f) return 0;             // below one alpha step: not drawn
        return static_cast<uint8_t>(std::min(255.0f,
            static_cast<float>(a_) * std::sqrt(lit)));
    };

    std::vector<SDL_FPoint>& strip = strip_;   // mutable scratch; see the header

    for (int row = 0; row < rows_; ++row) {
        float peak = 0.0f;
        for (int col = 0; col < cols_; ++col) {
            const Node& n = nodes_[index(col, row)];
            peak = std::max(peak, std::fabs(n.dx) + std::fabs(n.dy));
            strip[static_cast<size_t>(col)] =
                to_screen(origin_x_ + static_cast<float>(col) * spacing_ + n.dx,
                          origin_y_ + static_cast<float>(row) * spacing_ + n.dy);
        }
        const uint8_t alpha = strip_alpha(peak);
        if (alpha == 0) continue;                // a flat row is not drawn at all
        SDL_SetRenderDrawColor(renderer, r_, g_, b_, alpha);
        SDL_RenderLines(renderer, strip.data(), cols_);
    }
    for (int col = 0; col < cols_; ++col) {
        float peak = 0.0f;
        for (int row = 0; row < rows_; ++row) {
            const Node& n = nodes_[index(col, row)];
            peak = std::max(peak, std::fabs(n.dx) + std::fabs(n.dy));
            strip[static_cast<size_t>(row)] =
                to_screen(origin_x_ + static_cast<float>(col) * spacing_ + n.dx,
                          origin_y_ + static_cast<float>(row) * spacing_ + n.dy);
        }
        const uint8_t alpha = strip_alpha(peak);
        if (alpha == 0) continue;
        SDL_SetRenderDrawColor(renderer, r_, g_, b_, alpha);
        SDL_RenderLines(renderer, strip.data(), rows_);
    }
}
