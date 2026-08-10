#ifndef HAZARD_PATCH_HPP
#define HAZARD_PATCH_HPP

#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "collision_layers.hpp"
#include "player_components.hpp"   // ContactDamage
#include <cstdint>
#include <string>

/**
 * hazard — the one recipe for "a static thing that hurts the drone" (D69).
 *
 * `main.cpp`'s spawn_arena_props builds the permanent arena vents inline; the
 * Bio-lab's poison patches and the Foundry's mine blasts are the same entity with
 * a Lifetime on it. Rather than a third and fourth copy of that component list,
 * every transient one comes from here.
 *
 * ponytail: the permanent arena vents in spawn_arena_props are NOT routed through
 * this helper yet. Lane E's arena-VFX phase is rewriting that same lambda in
 * parallel, and a two-lane edit of one block is a merge conflict for no gameplay
 * gain. Pointing it here afterwards is a ~30-line deletion in main.cpp.
 *
 * Free functions in a header, the item_system.hpp / shield_system.hpp idiom: no
 * state to own, nothing to configure beyond the arguments.
 */
namespace hazard {

struct PatchSpec {
    float size = 90.0f;
    float damage = 9.0f;
    float lifetime = 3.0f;
    uint8_t r = 120, g = 235, b = 90, a = 200;
    float emission_rate = 26.0f;   // 0 = no plume (measure before raising: ENGINE.md §5)
    std::string image;             // empty = the flat Color rect
};

/**
 * A damaging patch centred on (cx, cy) that expires by itself.
 *
 * HAZARD layer + ContactDamage is the whole damage path: PlayerDamageSystem
 * already hurts the drone for anything carrying ContactDamage in its
 * CollidedWith, and HAZARD_MASK is PLAYER only, so a patch never touches the
 * enemy that dropped it. Score/currency 0 — a patch is not a kill.
 */
inline Entity spawn_patch(ComponentStorage& storage, EntityManager& entity_manager,
                          float cx, float cy, const PatchSpec& spec) {
    const float half = spec.size * 0.5f;
    Entity e = entity_manager.create_entity();
    storage.add_component<Position>(e, Position{cx - half, cy - half});
    storage.add_component<Size>(e, Size{spec.size, spec.size});
    storage.add_component<Color>(e, Color{spec.r, spec.g, spec.b, spec.a});
    storage.add_component<Collider>(e,
        Collider{spec.size, spec.size, layers::HAZARD, layers::HAZARD_MASK});
    storage.add_component<ContactDamage>(e, ContactDamage{spec.damage, 0, 0, 1.0f});
    storage.add_component<Lifetime>(e, Lifetime{spec.lifetime});
    if (!spec.image.empty()) storage.add_component<Images>(e, Images{{spec.image}, 0});
    // Same layer as the permanent vents: above obstacles, below the drone, so a
    // patch is never hidden behind a pillar you were about to walk past.
    storage.add_component<RenderLayer>(e, RenderLayer{3});
    if (spec.emission_rate > 0.0f) {
        ParticleEmitter p;
        p.shape = EmitterShape::Circle;
        p.radius = half;
        p.additive = true;
        p.emission_rate = spec.emission_rate;
        p.particle_lifetime = 0.7f;
        p.min_speed = 0.0f; p.max_speed = 20.0f;
        p.cone_half_angle = 180.0f;
        p.start_size = 6.0f; p.end_size = 0.0f;
        p.start_r = spec.r; p.start_g = spec.g; p.start_b = spec.b; p.start_a = 200;
        p.end_r = 40; p.end_g = 60; p.end_b = 30; p.end_a = 0;
        p.offset_x = half; p.offset_y = half;
        storage.add_component<ParticleEmitter>(e, p);
    }
    return e;
}

}  // namespace hazard

#endif  // HAZARD_PATCH_HPP
