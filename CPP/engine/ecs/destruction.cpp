#include "destruction.hpp"
#include "entity_manager.hpp"
#include "component_storage.hpp"
#include "game/tower_components.hpp"
#include "game/enemy_components.hpp"
#include "game/player_components.hpp"

void destroy_marked_entities(EntityManager& em, ComponentStorage& storage) {
    // Get a snapshot (vector copy) of all entities marked for destruction.
    // Iterating a copy is safe even though we modify the maps during the loop.
    auto marked = storage.entities_with_component<DestroyRequest>();

    for (Entity entity : marked) {
        // Remove ALL component types — each call is a no-op if the
        // entity doesn't have that particular component.

        // Engine components
        storage.remove_component<Position>(entity);
        storage.remove_component<Size>(entity);
        storage.remove_component<Color>(entity);
        storage.remove_component<Input>(entity);
        storage.remove_component<Velocity>(entity);
        storage.remove_component<Images>(entity);
        storage.remove_component<Text>(entity);
        storage.remove_component<ScreenPosition>(entity);
        storage.remove_component<Rotation>(entity);
        storage.remove_component<Collider>(entity);
        storage.remove_component<CircleCollider>(entity);
        storage.remove_component<OBBCollider>(entity);
        storage.remove_component<Lifetime>(entity);
        storage.remove_component<WrapAround>(entity);
        storage.remove_component<CollidedWith>(entity);
        storage.remove_component<Script>(entity);
        storage.remove_component<Animation>(entity);
        storage.remove_component<SpriteSheet>(entity);
        storage.remove_component<RenderLayer>(entity);
        storage.remove_component<Tint>(entity);

        // Game components — tower defense
        storage.remove_component<TowerTag>(entity);
        storage.remove_component<TowerStats>(entity);
        storage.remove_component<TowerAnimationClips>(entity);
        storage.remove_component<TowerAiming>(entity);
        storage.remove_component<BeamTag>(entity);
        storage.remove_component<ProjectileTag>(entity);
        storage.remove_component<ProjectileData>(entity);
        storage.remove_component<EnemyTag>(entity);
        storage.remove_component<Health>(entity);
        storage.remove_component<HealthBarTag>(entity);
        storage.remove_component<PathFollower>(entity);
        storage.remove_component<CannonFireRequest>(entity);
        storage.remove_component<LaserFireRequest>(entity);
        storage.remove_component<LaserFiring>(entity);
        storage.remove_component<DamageEvent>(entity);

        // Game components — Class-110 "Reactor Drone"
        storage.remove_component<PlayerTag>(entity);
        storage.remove_component<Experience>(entity);
        storage.remove_component<ContactDamage>(entity);
        storage.remove_component<WeaponStats>(entity);

        // DestroyRequest itself (must be last)
        storage.remove_component<DestroyRequest>(entity);

        em.destroy_entity(entity);
    }
}
