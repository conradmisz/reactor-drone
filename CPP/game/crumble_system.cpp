#include "crumble_system.hpp"

#include <algorithm>
#include <cmath>

#include "engine/ecs/components.hpp"
#include "collision_layers.hpp"
#include "engine/ecs/fx_events.hpp"

namespace {

/// Debris burst on a destroyed pillar. One short-lived emitter host, the same
/// one-shot pattern EnemyDeathSystem and the arena shockwave already use — so it
/// retires on its Lifetime and cannot leak into the shop (the D-phase-5 trap).
void spawn_debris(ComponentStorage& cs, EntityManager& em,
                  float cx, float cy, float size) {
    Entity burst = em.create_entity();
    cs.add_component<Position>(burst, Position{cx, cy});
    ParticleEmitter e;
    e.shape = EmitterShape::Point;
    e.additive = false;                       // rubble, not light
    e.emission_rate = 420.0f;
    e.particle_lifetime = 0.7f;
    e.min_speed = 60.0f;
    e.max_speed = 220.0f;
    e.cone_half_angle = 180.0f;
    e.start_size = std::max(3.0f, size * 0.10f);
    e.end_size = 0.0f;
    e.start_r = 120; e.start_g = 132; e.start_b = 150; e.start_a = 235;
    e.end_r = 60;    e.end_g = 66;    e.end_b = 78;    e.end_a = 0;
    cs.add_component<ParticleEmitter>(burst, e);
    cs.add_component<Lifetime>(burst, Lifetime{0.12f});
}

/// Is this entity a live arena obstacle?
bool is_obstacle(const ComponentStorage& cs, Entity e) {
    auto col = cs.get_component<Collider>(e);
    return col.has_value() && (col->get().layer & layers::OBSTACLE) != 0;
}

}  // namespace

void CrumbleSystem::set_arena(const std::vector<ObstacleDef>& defs) {
    live_ = defs;
}

bool CrumbleSystem::update(ComponentStorage& component_storage,
                           EntityManager& entity_manager,
                           Blackboard& blackboard) {
    bool dirty = false;

    for (Entity e : component_storage.entities_with_component<Health>()) {
        if (!is_obstacle(component_storage, e)) continue;
        auto hp = component_storage.get_component<Health>(e);
        auto pos = component_storage.get_component<Position>(e);
        auto sz = component_storage.get_component<Size>(e);
        if (!hp.has_value() || !pos.has_value() || !sz.has_value()) continue;
        if (hp->get().max_hp <= 0.0f) continue;

        const float frac = std::max(0.0f, hp->get().current / hp->get().max_hp);

        if (frac > 0.0f) {
            // Damage stages, as shading rather than as swapped art.
            // ponytail: the spec wanted cracked sprite variants from the offline
            // generator. A darkening multiply reads as "this one is failing" at
            // the sizes obstacles draw and costs no new committed assets; if the
            // art direction wants real cracks, this is the line that goes away.
            const uint8_t v = static_cast<uint8_t>(90 + 165 * frac);
            if (auto t = component_storage.get_component<Tint>(e); t.has_value()) {
                t->get() = Tint{v, v, v, 255, false};
            } else {
                component_storage.add_component<Tint>(e, Tint{v, v, v, 255, false});
            }
            continue;
        }

        // --- destroyed ----------------------------------------------------
        const float cx = pos->get().x + sz->get().width * 0.5f;
        const float cy = pos->get().y + sz->get().height * 0.5f;
        spawn_debris(component_storage, entity_manager, cx, cy, sz->get().width);

        // The collider goes with the entity: DestroyRequest sweeps every
        // component, so nothing can be left behind pretending to be a wall.
        component_storage.add_component<DestroyRequest>(e, DestroyRequest{});
        ++destroyed_;

        // Drop the matching row from the live list. Matched on geometry rather
        // than on an index: the entity order and the config order are the same
        // today, but an index would rot silently the first time either moves.
        auto it = std::find_if(live_.begin(), live_.end(), [&](const ObstacleDef& o) {
            return std::fabs(o.x - pos->get().x) < 0.5f &&
                   std::fabs(o.y - pos->get().y) < 0.5f;
        });
        if (it != live_.end()) live_.erase(it);
        dirty = true;

        // A pillar coming down is a real event: it shakes the room, rings the
        // lattice and scorches the floor it stood on. Trauma rides the same key
        // the rest of the game's feedback uses; the impulse and the stamp ride the
        // one-way render-FX vocabulary a death already uses (Phase 0, D138), so
        // this adds no new coupling and nothing sim-side can read it back.
        blackboard.set<float>("feedback.trauma",
            std::min(1.0f, blackboard.get_or<float>("feedback.trauma", 0.0f) + 0.35f));
        fx_events::push_impulse(blackboard, cx, cy, sz->get().width / 40.0f * 1.5f);
        fx_events::push_stamp(blackboard, cx, cy, /*kind=*/0, sz->get().width / 48.0f);
    }

    return dirty;
}
