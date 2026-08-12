#include "force_field_system.hpp"

#include <algorithm>
#include <cmath>

#include "engine/ecs/components.hpp"
#include "enemy_components.hpp"    // EnemyTag
#include "player_components.hpp"   // PlayerTag

namespace {

/// Acceleration ceiling per body per frame, in px/s. A field is meant to bend a
/// flight line, not to teleport a body across the arena on one long frame — and
/// an unbounded delta would let a strong well outrun the obstacle push-out's
/// ability to resolve the overlap it creates.
constexpr float MAX_DELTA_PER_FRAME = 900.0f;

}  // namespace

void ForceFieldSystem::set_capacity(int max_sources) {
    capacity_ = static_cast<std::size_t>(std::max(0, max_sources));
    if (sources_.size() > capacity_) sources_.resize(capacity_);
    sources_.reserve(capacity_);
}

bool ForceFieldSystem::add_source(const Source& s) {
    if (sources_.size() >= capacity_) return false;
    if (!(s.radius > 0.0f) || s.lifetime <= 0.0f) return false;
    sources_.push_back(s);
    return true;
}

void ForceFieldSystem::update(ComponentStorage& component_storage, float dt) {
    if (sources_.empty() || dt <= 0.0f) return;

    // One pass over the bodies, inner loop over the (few) sources: bodies are the
    // long list, and this way each body's Velocity is fetched once.
    auto apply = [&](Entity e, bool is_player) {
        auto vel = component_storage.get_component<Velocity>(e);
        auto pos = component_storage.get_component<Position>(e);
        if (!vel.has_value() || !pos.has_value()) return;
        auto sz = component_storage.get_component<Size>(e);
        const float half = sz.has_value() ? sz->get().width * 0.5f : 0.0f;
        const float bx = pos->get().x + half;
        const float by = pos->get().y + half;

        float ax = 0.0f, ay = 0.0f;
        for (const Source& s : sources_) {
            if (is_player ? !s.affect_player : !s.affect_enemies) continue;
            const float dx = s.x - bx;
            const float dy = s.y - by;
            const float d2 = dx * dx + dy * dy;
            if (d2 >= s.radius * s.radius) continue;   // squared compare, no sqrt
            const float d = std::sqrt(d2);
            if (d < 1e-3f) continue;                   // dead centre: no direction
            // Linear falloff to zero at the rim: no divide by d^2, and a hard
            // zero at the edge means a body leaving the field does not snap.
            const float falloff = 1.0f - d / s.radius;
            const float a = s.strength * falloff;
            ax += dx / d * a;
            ay += dy / d * a;
        }
        if (ax == 0.0f && ay == 0.0f) return;

        float ddx = ax * dt;
        float ddy = ay * dt;
        const float mag = std::sqrt(ddx * ddx + ddy * ddy);
        if (mag > MAX_DELTA_PER_FRAME) {
            ddx = ddx / mag * MAX_DELTA_PER_FRAME;
            ddy = ddy / mag * MAX_DELTA_PER_FRAME;
        }
        vel->get().dx += ddx;
        vel->get().dy += ddy;
    };

    for (Entity e : component_storage.entities_with_component<EnemyTag>())
        apply(e, /*is_player=*/false);
    for (Entity e : component_storage.entities_with_component<PlayerTag>())
        apply(e, /*is_player=*/true);

    // Age AFTER applying, so a source registered with a one-frame lifetime (an
    // impulse) acts on exactly the frame it was registered for.
    for (Source& s : sources_) s.lifetime -= dt;
    sources_.erase(std::remove_if(sources_.begin(), sources_.end(),
                                  [](const Source& s) { return s.lifetime <= 0.0f; }),
                   sources_.end());
}
