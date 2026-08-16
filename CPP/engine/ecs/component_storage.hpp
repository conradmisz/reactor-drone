#ifndef COMPONENT_STORAGE_HPP
#define COMPONENT_STORAGE_HPP

#include "components.hpp"
#include "game/enemy_components.hpp"
#include "game/tower_components.hpp"
#include "game/player_components.hpp"
#include <unordered_map>
#include <optional>
#include <functional>
#include <vector>

/**
 * ComponentStorage class
 * 
 * Manages the storage and retrieval of components in the ECS architecture.
 * This class demonstrates a key ECS principle: components (data) are stored
 * separately from entities (IDs) and systems (logic).
 * 
 * The ComponentStorage associates component data with entity IDs using a
 * "table per component type" approach. Each component type (Position, Size, Color)
 * has its own std::unordered_map that maps Entity IDs to component instances.
 * 
 * Key Design Decisions:
 * - Template methods allow type-safe component operations
 * - std::optional<std::reference_wrapper<T>> for get_component() avoids copying
 *   and clearly indicates when a component doesn't exist
 * - Each entity can have at most one component of each type
 * - Adding a component when one already exists replaces the old value
 * 
 * Component Lifecycle:
 * - add_component<T>() associates a component with an entity
 * - get_component<T>() retrieves a reference to the component (if it exists)
 * - remove_component<T>() removes the association
 * - has_component<T>() checks if the association exists
 * - entities_with_component<T>() returns all entities that have that component type
 */
class ComponentStorage {
public:
    /**
     * Adds a component to an entity.
     * 
     * If the entity already has a component of this type, the old value is replaced.
     * This is the expected behavior for component updates - you don't need to remove
     * the old component first.
     * 
     * The component is copied into storage, so the original can be safely discarded.
     * 
     * @tparam T The component type (Position, Size, or Color)
     * @param entity The entity ID to attach the component to
     * @param component The component data to store
     */
    template<typename T>
    void add_component(Entity entity, const T& component);
    
    /**
     * Retrieves a component for an entity (mutable version).
     * 
     * Returns a reference wrapper to the component if it exists, allowing
     * modification of the component data. Returns std::nullopt if the entity
     * doesn't have this component type.
     * 
     * Using std::optional<std::reference_wrapper<T>> avoids copying the component
     * and clearly indicates the "not found" case without exceptions or null pointers.
     * 
     * Example usage:
     *   auto pos = storage.get_component<Position>(entity);
     *   if (pos.has_value()) {
     *       pos->get().x += 10;  // Modify the component
     *   }
     * 
     * @tparam T The component type (Position, Size, or Color)
     * @param entity The entity ID to retrieve the component from
     * @return std::optional containing a reference to the component, or std::nullopt
     */
    template<typename T>
    std::optional<std::reference_wrapper<T>> get_component(Entity entity);
    
    /**
     * Retrieves a component for an entity (const version).
     * 
     * Same as the mutable version, but returns a const reference for read-only access.
     * Used when you need to read component data without modifying it.
     * 
     * @tparam T The component type (Position, Size, or Color)
     * @param entity The entity ID to retrieve the component from
     * @return std::optional containing a const reference to the component, or std::nullopt
     */
    template<typename T>
    std::optional<std::reference_wrapper<const T>> get_component(Entity entity) const;
    
    /**
     * Removes a component from an entity.
     * 
     * After calling this method, has_component<T>(entity) will return false
     * and get_component<T>(entity) will return std::nullopt.
     * 
     * If the entity doesn't have this component type, this is a no-op (safe to call).
     * This simplifies cleanup code - you don't need to check has_component() first.
     * 
     * @tparam T The component type (Position, Size, or Color)
     * @param entity The entity ID to remove the component from
     */
    template<typename T>
    void remove_component(Entity entity);
    
    /**
     * Checks if an entity has a specific component type.
     * 
     * This is a convenience method that's equivalent to checking if
     * get_component<T>(entity).has_value(), but more readable.
     * 
     * @tparam T The component type (Position, Size, or Color)
     * @param entity The entity ID to check
     * @return true if the entity has this component type, false otherwise
     */
    template<typename T>
    bool has_component(Entity entity) const;
    
    /**
     * Returns all entities that have a specific component type.
     * 
     * This is used by systems to iterate over entities with required components.
     * For example, the RenderSystem uses this to find all entities with Position,
     * Size, and Color components.
     * 
     * The returned vector contains entity IDs in an unspecified but consistent order.
     * 
     * @tparam T The component type (Position, Size, or Color)
     * @return A vector of entity IDs that have this component type
     */
    template<typename T>
    std::vector<Entity> entities_with_component() const;

private:
    /**
     * Storage for Position components.
     * Maps entity IDs to Position data. Only entities that have a Position
     * component will have an entry in this map.
     */
    std::unordered_map<Entity, Position> positions_;
    
    /**
     * Storage for Size components.
     * Maps entity IDs to Size data. Only entities that have a Size
     * component will have an entry in this map.
     */
    std::unordered_map<Entity, Size> sizes_;
    
    /**
     * Storage for Color components.
     * Maps entity IDs to Color data. Only entities that have a Color
     * component will have an entry in this map.
     */
    std::unordered_map<Entity, Color> colors_;
    
    /**
     * Storage for Input components.
     * Maps entity IDs to Input data. Only entities that have an Input
     * component will have an entry in this map.
     */
    std::unordered_map<Entity, Input> inputs_;
    
    /**
     * Storage for Velocity components.
     * Maps entity IDs to Velocity data. Only entities that have a Velocity
     * component will have an entry in this map.
     */
    std::unordered_map<Entity, Velocity> velocities_;
    
    /**
     * Storage for Images components.
     * Maps entity IDs to Images data. Only entities that have an Images
     * component will have an entry in this map.
     */
    std::unordered_map<Entity, Images> images_;
    
    /**
     * Storage for Text components.
     * Maps entity IDs to Text data. Only entities that have a Text
     * component will have an entry in this map.
     */
    std::unordered_map<Entity, Text> texts_;
    
    /**
     * Storage for ScreenPosition components.
     * Maps entity IDs to ScreenPosition data. Only entities that have a ScreenPosition
     * component will have an entry in this map.
     */
    std::unordered_map<Entity, ScreenPosition> screen_positions_;
    
    /**
     * Storage for Rotation components.
     * Maps entity IDs to Rotation data. Only entities that have a Rotation
     * component will have an entry in this map.
     */
    std::unordered_map<Entity, Rotation> rotations_;
    
    /**
     * Storage for Collider components.
     * Maps entity IDs to Collider data. Only entities that have a Collider
     * component will have an entry in this map.
     */
    std::unordered_map<Entity, Collider> colliders_;

    /**
     * Storage for CircleCollider components.
     * Maps entity IDs to CircleCollider data. Only entities that have a CircleCollider
     * component will have an entry in this map.
     */
    std::unordered_map<Entity, CircleCollider> circle_colliders_;

    /**
     * Storage for OBBCollider components.
     * Maps entity IDs to OBBCollider data. Only entities that have an OBBCollider
     * component will have an entry in this map.
     */
    std::unordered_map<Entity, OBBCollider> obb_colliders_;

    /**
     * Storage for Lifetime components.
     * Maps entity IDs to Lifetime data. Only entities that have a Lifetime
     * component will have an entry in this map.
     */
    std::unordered_map<Entity, Lifetime> lifetimes_;
    
    /**
     * Storage for WrapAround components.
     * Maps entity IDs to WrapAround data. Only entities that have a WrapAround
     * component will have an entry in this map.
     */
    std::unordered_map<Entity, WrapAround> wrap_arounds_;
    
    /**
     * Storage for DestroyRequest components.
     * Maps entity IDs to DestroyRequest data. Only entities that have a DestroyRequest
     * component will have an entry in this map.
     */
    std::unordered_map<Entity, DestroyRequest> destroy_requests_;
    
    /**
     * Storage for CollidedWith components.
     * Maps entity IDs to CollidedWith data. Only entities that have a CollidedWith
     * component will have an entry in this map.
     */
    std::unordered_map<Entity, CollidedWith> collided_withs_;
    
    /**
     * Storage for Script components.
     * Maps entity IDs to Script data. Only entities that have a Script
     * component will have an entry in this map.
     */
    std::unordered_map<Entity, Script> scripts_;
    
    /**
     * Storage for SpriteSheet components.
     * Maps entity IDs to SpriteSheet data. Only entities that have a SpriteSheet
     * component will have an entry in this map.
     */
    std::unordered_map<Entity, SpriteSheet> sprite_sheets_;
    
    /**
     * Storage for Animation components.
     * Maps entity IDs to Animation data. Only entities that have an Animation
     * component will have an entry in this map.
     */
    std::unordered_map<Entity, Animation> animations_;

    /**
     * Storage for EnemyTag components.
     * Maps entity IDs to EnemyTag data. Only entities that have an EnemyTag
     * component will have an entry in this map.
     */
    std::unordered_map<Entity, EnemyTag> enemy_tags_;

    /**
     * Storage for PathFollower components.
     * Maps entity IDs to PathFollower data. Only entities that have a PathFollower
     * component will have an entry in this map.
     */
    std::unordered_map<Entity, PathFollower> path_followers_;

    /**
     * Storage for Health components.
     * Maps entity IDs to Health data. Only entities that have a Health
     * component will have an entry in this map.
     */
    std::unordered_map<Entity, Health> healths_;

    /**
     * Storage for TowerTag components.
     * Maps entity IDs to TowerTag data. Only entities that have a TowerTag
     * component will have an entry in this map.
     */
    std::unordered_map<Entity, TowerTag> tower_tags_;

    /**
     * Storage for TowerStats components.
     * Maps entity IDs to TowerStats data. Only entities that have a TowerStats
     * component will have an entry in this map.
     */
    std::unordered_map<Entity, TowerStats> tower_stats_;

    /**
     * Storage for TowerAnimationClips components.
     * Maps entity IDs to TowerAnimationClips data. Only entities that have a
     * TowerAnimationClips component will have an entry in this map.
     */
    std::unordered_map<Entity, TowerAnimationClips> tower_animation_clips_;

    /**
     * Storage for TowerAiming components.
     * Maps entity IDs to TowerAiming data. Only entities that have a TowerAiming
     * component will have an entry in this map.
     */
    std::unordered_map<Entity, TowerAiming> tower_aiming_;

    /**
     * Storage for RenderLayer components (engine; draw-order hint).
     */
    std::unordered_map<Entity, RenderLayer> render_layers_;

    /**
     * Storage for Tint components (engine; v2 per-entity colour/alpha/blend mod).
     */
    std::unordered_map<Entity, Tint> tints_;

    /**
     * Storage for the UI & menu layer (engine; Option-040 port).
     */
    std::unordered_map<Entity, UIElement> ui_elements_;
    std::unordered_map<Entity, UIState> ui_states_;
    std::unordered_map<Entity, UIScreen> ui_screens_;
    std::unordered_map<Entity, ScreenMembership> screen_memberships_;

    /**
     * Storage for Particle / ParticleEmitter components (engine; ported from 020).
     */
    std::unordered_map<Entity, Particle> particles_;
    std::unordered_map<Entity, ParticleEmitter> particle_emitters_;

    /**
     * Storage for BeamTag components (laser-beam entity marker).
     */
    std::unordered_map<Entity, BeamTag> beam_tags_;

    /**
     * Storage for HealthBarTag components (floating health-bar entity marker).
     */
    std::unordered_map<Entity, HealthBarTag> health_bar_tags_;

    /**
     * Storage for ProjectileTag components.
     * Maps entity IDs to ProjectileTag data. Only entities that have a ProjectileTag
     * component will have an entry in this map.
     */
    std::unordered_map<Entity, ProjectileTag> projectile_tags_;

    /**
     * Storage for ProjectileData components.
     * Maps entity IDs to ProjectileData data. Only entities that have a ProjectileData
     * component will have an entry in this map.
     */
    std::unordered_map<Entity, ProjectileData> projectile_datas_;

    /**
     * Storage for CannonFireRequest components.
     * Maps entity IDs to CannonFireRequest data. Only entities that have a CannonFireRequest
     * component will have an entry in this map.
     */
    std::unordered_map<Entity, CannonFireRequest> cannon_fire_requests_;

    /**
     * Storage for LaserFireRequest components.
     * Maps entity IDs to LaserFireRequest data. Only entities that have a LaserFireRequest
     * component will have an entry in this map.
     */
    std::unordered_map<Entity, LaserFireRequest> laser_fire_requests_;

    /**
     * Storage for LaserFiring components.
     * Maps entity IDs to LaserFiring data. Only entities that have a LaserFiring
     * component will have an entry in this map.
     */
    std::unordered_map<Entity, LaserFiring> laser_firings_;

    /**
     * Storage for DamageEvent components.
     * Maps entity IDs to DamageEvent data. Only entities that have a DamageEvent
     * component will have an entry in this map.
     */
    std::unordered_map<Entity, DamageEvent> damage_events_;

    // Class-110 "Reactor Drone" game components.
    std::unordered_map<Entity, PlayerTag> player_tags_;
    std::unordered_map<Entity, ShipState> ship_states_;
    std::unordered_map<Entity, Pickup> pickups_;
    std::unordered_map<Entity, ContactDamage> contact_damages_;
    std::unordered_map<Entity, WeaponStats> weapon_stats_;
    std::unordered_map<Entity, Flash> flashes_;
    // Gameplay pack v2.3 tier 3 (D221): secondary-fire status effects.
    std::unordered_map<Entity, Burn> burns_;
    std::unordered_map<Entity, Chill> chills_;
    std::unordered_map<Entity, BlizzardTag> blizzards_;
    // Iteration 3 (D51): enemy projectiles and non-seeker enemy behaviour.
    std::unordered_map<Entity, EnemyShot> enemy_shots_;
    std::unordered_map<Entity, EnemyBehavior> enemy_behaviors_;

    /**
     * Helper method to get the storage map for a specific component type.
     * This allows the template methods to work uniformly across all component types.
     * 
     * @tparam T The component type
     * @return Reference to the storage map for that type
     */
    template<typename T>
    std::unordered_map<Entity, T>& get_storage();
    
    /**
     * Const version of get_storage() for read-only access.
     * 
     * @tparam T The component type
     * @return Const reference to the storage map for that type
     */
    template<typename T>
    const std::unordered_map<Entity, T>& get_storage() const;
};

// Declare explicit specializations of get_storage
template<> std::unordered_map<Entity, Position>& ComponentStorage::get_storage<Position>();
template<> std::unordered_map<Entity, Size>& ComponentStorage::get_storage<Size>();
template<> std::unordered_map<Entity, Color>& ComponentStorage::get_storage<Color>();
template<> std::unordered_map<Entity, Input>& ComponentStorage::get_storage<Input>();
template<> std::unordered_map<Entity, Velocity>& ComponentStorage::get_storage<Velocity>();
template<> std::unordered_map<Entity, Images>& ComponentStorage::get_storage<Images>();
template<> std::unordered_map<Entity, Text>& ComponentStorage::get_storage<Text>();
template<> std::unordered_map<Entity, ScreenPosition>& ComponentStorage::get_storage<ScreenPosition>();
template<> const std::unordered_map<Entity, Position>& ComponentStorage::get_storage<Position>() const;
template<> const std::unordered_map<Entity, Size>& ComponentStorage::get_storage<Size>() const;
template<> const std::unordered_map<Entity, Color>& ComponentStorage::get_storage<Color>() const;
template<> const std::unordered_map<Entity, Input>& ComponentStorage::get_storage<Input>() const;
template<> const std::unordered_map<Entity, Velocity>& ComponentStorage::get_storage<Velocity>() const;
template<> const std::unordered_map<Entity, Images>& ComponentStorage::get_storage<Images>() const;
template<> const std::unordered_map<Entity, Text>& ComponentStorage::get_storage<Text>() const;
template<> const std::unordered_map<Entity, ScreenPosition>& ComponentStorage::get_storage<ScreenPosition>() const;
template<> std::unordered_map<Entity, Rotation>& ComponentStorage::get_storage<Rotation>();
template<> std::unordered_map<Entity, Collider>& ComponentStorage::get_storage<Collider>();
template<> std::unordered_map<Entity, CircleCollider>& ComponentStorage::get_storage<CircleCollider>();
template<> std::unordered_map<Entity, OBBCollider>& ComponentStorage::get_storage<OBBCollider>();
template<> std::unordered_map<Entity, Lifetime>& ComponentStorage::get_storage<Lifetime>();
template<> std::unordered_map<Entity, WrapAround>& ComponentStorage::get_storage<WrapAround>();
template<> std::unordered_map<Entity, DestroyRequest>& ComponentStorage::get_storage<DestroyRequest>();
template<> std::unordered_map<Entity, CollidedWith>& ComponentStorage::get_storage<CollidedWith>();
template<> const std::unordered_map<Entity, Rotation>& ComponentStorage::get_storage<Rotation>() const;
template<> const std::unordered_map<Entity, Collider>& ComponentStorage::get_storage<Collider>() const;
template<> const std::unordered_map<Entity, CircleCollider>& ComponentStorage::get_storage<CircleCollider>() const;
template<> const std::unordered_map<Entity, OBBCollider>& ComponentStorage::get_storage<OBBCollider>() const;
template<> const std::unordered_map<Entity, Lifetime>& ComponentStorage::get_storage<Lifetime>() const;
template<> const std::unordered_map<Entity, WrapAround>& ComponentStorage::get_storage<WrapAround>() const;
template<> const std::unordered_map<Entity, DestroyRequest>& ComponentStorage::get_storage<DestroyRequest>() const;
template<> const std::unordered_map<Entity, CollidedWith>& ComponentStorage::get_storage<CollidedWith>() const;
template<> std::unordered_map<Entity, Script>& ComponentStorage::get_storage<Script>();
template<> const std::unordered_map<Entity, Script>& ComponentStorage::get_storage<Script>() const;
template<> std::unordered_map<Entity, SpriteSheet>& ComponentStorage::get_storage<SpriteSheet>();
template<> const std::unordered_map<Entity, SpriteSheet>& ComponentStorage::get_storage<SpriteSheet>() const;
template<> std::unordered_map<Entity, Animation>& ComponentStorage::get_storage<Animation>();
template<> const std::unordered_map<Entity, Animation>& ComponentStorage::get_storage<Animation>() const;
template<> std::unordered_map<Entity, EnemyTag>& ComponentStorage::get_storage<EnemyTag>();
template<> const std::unordered_map<Entity, EnemyTag>& ComponentStorage::get_storage<EnemyTag>() const;
template<> std::unordered_map<Entity, PathFollower>& ComponentStorage::get_storage<PathFollower>();
template<> const std::unordered_map<Entity, PathFollower>& ComponentStorage::get_storage<PathFollower>() const;
template<> std::unordered_map<Entity, Health>& ComponentStorage::get_storage<Health>();
template<> const std::unordered_map<Entity, Health>& ComponentStorage::get_storage<Health>() const;
template<> std::unordered_map<Entity, TowerTag>& ComponentStorage::get_storage<TowerTag>();
template<> const std::unordered_map<Entity, TowerTag>& ComponentStorage::get_storage<TowerTag>() const;
template<> std::unordered_map<Entity, TowerStats>& ComponentStorage::get_storage<TowerStats>();
template<> const std::unordered_map<Entity, TowerStats>& ComponentStorage::get_storage<TowerStats>() const;
template<> std::unordered_map<Entity, TowerAnimationClips>& ComponentStorage::get_storage<TowerAnimationClips>();
template<> const std::unordered_map<Entity, TowerAnimationClips>& ComponentStorage::get_storage<TowerAnimationClips>() const;
template<> std::unordered_map<Entity, TowerAiming>& ComponentStorage::get_storage<TowerAiming>();
template<> const std::unordered_map<Entity, TowerAiming>& ComponentStorage::get_storage<TowerAiming>() const;
template<> std::unordered_map<Entity, RenderLayer>& ComponentStorage::get_storage<RenderLayer>();
template<> const std::unordered_map<Entity, RenderLayer>& ComponentStorage::get_storage<RenderLayer>() const;
template<> std::unordered_map<Entity, Tint>& ComponentStorage::get_storage<Tint>();
template<> const std::unordered_map<Entity, Tint>& ComponentStorage::get_storage<Tint>() const;
template<> std::unordered_map<Entity, Particle>& ComponentStorage::get_storage<Particle>();
template<> const std::unordered_map<Entity, Particle>& ComponentStorage::get_storage<Particle>() const;
template<> std::unordered_map<Entity, ParticleEmitter>& ComponentStorage::get_storage<ParticleEmitter>();
template<> const std::unordered_map<Entity, ParticleEmitter>& ComponentStorage::get_storage<ParticleEmitter>() const;
template<> std::unordered_map<Entity, BeamTag>& ComponentStorage::get_storage<BeamTag>();
template<> const std::unordered_map<Entity, BeamTag>& ComponentStorage::get_storage<BeamTag>() const;
template<> std::unordered_map<Entity, HealthBarTag>& ComponentStorage::get_storage<HealthBarTag>();
template<> const std::unordered_map<Entity, HealthBarTag>& ComponentStorage::get_storage<HealthBarTag>() const;
template<> std::unordered_map<Entity, ProjectileTag>& ComponentStorage::get_storage<ProjectileTag>();
template<> const std::unordered_map<Entity, ProjectileTag>& ComponentStorage::get_storage<ProjectileTag>() const;
template<> std::unordered_map<Entity, ProjectileData>& ComponentStorage::get_storage<ProjectileData>();
template<> const std::unordered_map<Entity, ProjectileData>& ComponentStorage::get_storage<ProjectileData>() const;
template<> std::unordered_map<Entity, CannonFireRequest>& ComponentStorage::get_storage<CannonFireRequest>();
template<> const std::unordered_map<Entity, CannonFireRequest>& ComponentStorage::get_storage<CannonFireRequest>() const;
template<> std::unordered_map<Entity, LaserFireRequest>& ComponentStorage::get_storage<LaserFireRequest>();
template<> const std::unordered_map<Entity, LaserFireRequest>& ComponentStorage::get_storage<LaserFireRequest>() const;
template<> std::unordered_map<Entity, LaserFiring>& ComponentStorage::get_storage<LaserFiring>();
template<> const std::unordered_map<Entity, LaserFiring>& ComponentStorage::get_storage<LaserFiring>() const;
template<> std::unordered_map<Entity, DamageEvent>& ComponentStorage::get_storage<DamageEvent>();
template<> const std::unordered_map<Entity, DamageEvent>& ComponentStorage::get_storage<DamageEvent>() const;
template<> std::unordered_map<Entity, PlayerTag>& ComponentStorage::get_storage<PlayerTag>();
template<> const std::unordered_map<Entity, PlayerTag>& ComponentStorage::get_storage<PlayerTag>() const;
template<> std::unordered_map<Entity, ShipState>& ComponentStorage::get_storage<ShipState>();
template<> const std::unordered_map<Entity, ShipState>& ComponentStorage::get_storage<ShipState>() const;
template<> std::unordered_map<Entity, Pickup>& ComponentStorage::get_storage<Pickup>();
template<> const std::unordered_map<Entity, Pickup>& ComponentStorage::get_storage<Pickup>() const;
template<> std::unordered_map<Entity, ContactDamage>& ComponentStorage::get_storage<ContactDamage>();
template<> const std::unordered_map<Entity, ContactDamage>& ComponentStorage::get_storage<ContactDamage>() const;
template<> std::unordered_map<Entity, WeaponStats>& ComponentStorage::get_storage<WeaponStats>();
template<> const std::unordered_map<Entity, WeaponStats>& ComponentStorage::get_storage<WeaponStats>() const;
template<> std::unordered_map<Entity, Flash>& ComponentStorage::get_storage<Flash>();
template<> const std::unordered_map<Entity, Flash>& ComponentStorage::get_storage<Flash>() const;
template<> std::unordered_map<Entity, Burn>& ComponentStorage::get_storage<Burn>();
template<> const std::unordered_map<Entity, Burn>& ComponentStorage::get_storage<Burn>() const;
template<> std::unordered_map<Entity, Chill>& ComponentStorage::get_storage<Chill>();
template<> const std::unordered_map<Entity, Chill>& ComponentStorage::get_storage<Chill>() const;
template<> std::unordered_map<Entity, BlizzardTag>& ComponentStorage::get_storage<BlizzardTag>();
template<> const std::unordered_map<Entity, BlizzardTag>& ComponentStorage::get_storage<BlizzardTag>() const;
template<> std::unordered_map<Entity, EnemyShot>& ComponentStorage::get_storage<EnemyShot>();
template<> const std::unordered_map<Entity, EnemyShot>& ComponentStorage::get_storage<EnemyShot>() const;
template<> std::unordered_map<Entity, EnemyBehavior>& ComponentStorage::get_storage<EnemyBehavior>();
template<> const std::unordered_map<Entity, EnemyBehavior>& ComponentStorage::get_storage<EnemyBehavior>() const;

// Prevent implicit instantiation — definitions are in component_storage.cpp
extern template void ComponentStorage::add_component<Position>(Entity, const Position&);
extern template void ComponentStorage::add_component<Size>(Entity, const Size&);
extern template void ComponentStorage::add_component<Color>(Entity, const Color&);
extern template void ComponentStorage::add_component<Input>(Entity, const Input&);
extern template void ComponentStorage::add_component<Velocity>(Entity, const Velocity&);
extern template void ComponentStorage::add_component<Images>(Entity, const Images&);
extern template void ComponentStorage::add_component<Text>(Entity, const Text&);
extern template void ComponentStorage::add_component<ScreenPosition>(Entity, const ScreenPosition&);

extern template std::optional<std::reference_wrapper<Position>> ComponentStorage::get_component<Position>(Entity);
extern template std::optional<std::reference_wrapper<Size>> ComponentStorage::get_component<Size>(Entity);
extern template std::optional<std::reference_wrapper<Color>> ComponentStorage::get_component<Color>(Entity);
extern template std::optional<std::reference_wrapper<Input>> ComponentStorage::get_component<Input>(Entity);
extern template std::optional<std::reference_wrapper<Velocity>> ComponentStorage::get_component<Velocity>(Entity);
extern template std::optional<std::reference_wrapper<Images>> ComponentStorage::get_component<Images>(Entity);
extern template std::optional<std::reference_wrapper<Text>> ComponentStorage::get_component<Text>(Entity);
extern template std::optional<std::reference_wrapper<ScreenPosition>> ComponentStorage::get_component<ScreenPosition>(Entity);

extern template std::optional<std::reference_wrapper<const Position>> ComponentStorage::get_component<Position>(Entity) const;
extern template std::optional<std::reference_wrapper<const Size>> ComponentStorage::get_component<Size>(Entity) const;
extern template std::optional<std::reference_wrapper<const Color>> ComponentStorage::get_component<Color>(Entity) const;
extern template std::optional<std::reference_wrapper<const Input>> ComponentStorage::get_component<Input>(Entity) const;
extern template std::optional<std::reference_wrapper<const Velocity>> ComponentStorage::get_component<Velocity>(Entity) const;
extern template std::optional<std::reference_wrapper<const Images>> ComponentStorage::get_component<Images>(Entity) const;
extern template std::optional<std::reference_wrapper<const Text>> ComponentStorage::get_component<Text>(Entity) const;
extern template std::optional<std::reference_wrapper<const ScreenPosition>> ComponentStorage::get_component<ScreenPosition>(Entity) const;

extern template void ComponentStorage::remove_component<Position>(Entity);
extern template void ComponentStorage::remove_component<Size>(Entity);
extern template void ComponentStorage::remove_component<Color>(Entity);
extern template void ComponentStorage::remove_component<Input>(Entity);
extern template void ComponentStorage::remove_component<Velocity>(Entity);
extern template void ComponentStorage::remove_component<Images>(Entity);
extern template void ComponentStorage::remove_component<Text>(Entity);
extern template void ComponentStorage::remove_component<ScreenPosition>(Entity);

extern template bool ComponentStorage::has_component<Position>(Entity) const;
extern template bool ComponentStorage::has_component<Size>(Entity) const;
extern template bool ComponentStorage::has_component<Color>(Entity) const;
extern template bool ComponentStorage::has_component<Input>(Entity) const;
extern template bool ComponentStorage::has_component<Velocity>(Entity) const;
extern template bool ComponentStorage::has_component<Images>(Entity) const;
extern template bool ComponentStorage::has_component<Text>(Entity) const;
extern template bool ComponentStorage::has_component<ScreenPosition>(Entity) const;

extern template std::vector<Entity> ComponentStorage::entities_with_component<Position>() const;
extern template std::vector<Entity> ComponentStorage::entities_with_component<Size>() const;
extern template std::vector<Entity> ComponentStorage::entities_with_component<Color>() const;
extern template std::vector<Entity> ComponentStorage::entities_with_component<Input>() const;
extern template std::vector<Entity> ComponentStorage::entities_with_component<Velocity>() const;
extern template std::vector<Entity> ComponentStorage::entities_with_component<Images>() const;
extern template std::vector<Entity> ComponentStorage::entities_with_component<Text>() const;
extern template std::vector<Entity> ComponentStorage::entities_with_component<ScreenPosition>() const;

extern template void ComponentStorage::add_component<Rotation>(Entity, const Rotation&);
extern template void ComponentStorage::add_component<Collider>(Entity, const Collider&);
extern template void ComponentStorage::add_component<CircleCollider>(Entity, const CircleCollider&);
extern template void ComponentStorage::add_component<OBBCollider>(Entity, const OBBCollider&);
extern template void ComponentStorage::add_component<Lifetime>(Entity, const Lifetime&);
extern template void ComponentStorage::add_component<WrapAround>(Entity, const WrapAround&);
extern template void ComponentStorage::add_component<DestroyRequest>(Entity, const DestroyRequest&);
extern template void ComponentStorage::add_component<CollidedWith>(Entity, const CollidedWith&);

extern template std::optional<std::reference_wrapper<Rotation>> ComponentStorage::get_component<Rotation>(Entity);
extern template std::optional<std::reference_wrapper<Collider>> ComponentStorage::get_component<Collider>(Entity);
extern template std::optional<std::reference_wrapper<CircleCollider>> ComponentStorage::get_component<CircleCollider>(Entity);
extern template std::optional<std::reference_wrapper<OBBCollider>> ComponentStorage::get_component<OBBCollider>(Entity);
extern template std::optional<std::reference_wrapper<Lifetime>> ComponentStorage::get_component<Lifetime>(Entity);
extern template std::optional<std::reference_wrapper<WrapAround>> ComponentStorage::get_component<WrapAround>(Entity);
extern template std::optional<std::reference_wrapper<DestroyRequest>> ComponentStorage::get_component<DestroyRequest>(Entity);
extern template std::optional<std::reference_wrapper<CollidedWith>> ComponentStorage::get_component<CollidedWith>(Entity);

extern template std::optional<std::reference_wrapper<const Rotation>> ComponentStorage::get_component<Rotation>(Entity) const;
extern template std::optional<std::reference_wrapper<const Collider>> ComponentStorage::get_component<Collider>(Entity) const;
extern template std::optional<std::reference_wrapper<const CircleCollider>> ComponentStorage::get_component<CircleCollider>(Entity) const;
extern template std::optional<std::reference_wrapper<const OBBCollider>> ComponentStorage::get_component<OBBCollider>(Entity) const;
extern template std::optional<std::reference_wrapper<const Lifetime>> ComponentStorage::get_component<Lifetime>(Entity) const;
extern template std::optional<std::reference_wrapper<const WrapAround>> ComponentStorage::get_component<WrapAround>(Entity) const;
extern template std::optional<std::reference_wrapper<const DestroyRequest>> ComponentStorage::get_component<DestroyRequest>(Entity) const;
extern template std::optional<std::reference_wrapper<const CollidedWith>> ComponentStorage::get_component<CollidedWith>(Entity) const;

extern template void ComponentStorage::remove_component<Rotation>(Entity);
extern template void ComponentStorage::remove_component<Collider>(Entity);
extern template void ComponentStorage::remove_component<CircleCollider>(Entity);
extern template void ComponentStorage::remove_component<OBBCollider>(Entity);
extern template void ComponentStorage::remove_component<Lifetime>(Entity);
extern template void ComponentStorage::remove_component<WrapAround>(Entity);
extern template void ComponentStorage::remove_component<DestroyRequest>(Entity);
extern template void ComponentStorage::remove_component<CollidedWith>(Entity);

extern template bool ComponentStorage::has_component<Rotation>(Entity) const;
extern template bool ComponentStorage::has_component<Collider>(Entity) const;
extern template bool ComponentStorage::has_component<CircleCollider>(Entity) const;
extern template bool ComponentStorage::has_component<OBBCollider>(Entity) const;
extern template bool ComponentStorage::has_component<Lifetime>(Entity) const;
extern template bool ComponentStorage::has_component<WrapAround>(Entity) const;
extern template bool ComponentStorage::has_component<DestroyRequest>(Entity) const;
extern template bool ComponentStorage::has_component<CollidedWith>(Entity) const;

extern template std::vector<Entity> ComponentStorage::entities_with_component<Rotation>() const;
extern template std::vector<Entity> ComponentStorage::entities_with_component<Collider>() const;
extern template std::vector<Entity> ComponentStorage::entities_with_component<CircleCollider>() const;
extern template std::vector<Entity> ComponentStorage::entities_with_component<OBBCollider>() const;
extern template std::vector<Entity> ComponentStorage::entities_with_component<Lifetime>() const;
extern template std::vector<Entity> ComponentStorage::entities_with_component<WrapAround>() const;
extern template std::vector<Entity> ComponentStorage::entities_with_component<DestroyRequest>() const;
extern template std::vector<Entity> ComponentStorage::entities_with_component<CollidedWith>() const;

extern template void ComponentStorage::add_component<Script>(Entity, const Script&);

extern template std::optional<std::reference_wrapper<Script>> ComponentStorage::get_component<Script>(Entity);

extern template std::optional<std::reference_wrapper<const Script>> ComponentStorage::get_component<Script>(Entity) const;

extern template void ComponentStorage::remove_component<Script>(Entity);

extern template bool ComponentStorage::has_component<Script>(Entity) const;

extern template std::vector<Entity> ComponentStorage::entities_with_component<Script>() const;

extern template void ComponentStorage::add_component<SpriteSheet>(Entity, const SpriteSheet&);

extern template std::optional<std::reference_wrapper<SpriteSheet>> ComponentStorage::get_component<SpriteSheet>(Entity);

extern template std::optional<std::reference_wrapper<const SpriteSheet>> ComponentStorage::get_component<SpriteSheet>(Entity) const;

extern template void ComponentStorage::remove_component<SpriteSheet>(Entity);

extern template bool ComponentStorage::has_component<SpriteSheet>(Entity) const;

extern template std::vector<Entity> ComponentStorage::entities_with_component<SpriteSheet>() const;

extern template void ComponentStorage::add_component<Animation>(Entity, const Animation&);

extern template std::optional<std::reference_wrapper<Animation>> ComponentStorage::get_component<Animation>(Entity);

extern template std::optional<std::reference_wrapper<const Animation>> ComponentStorage::get_component<Animation>(Entity) const;

extern template void ComponentStorage::remove_component<Animation>(Entity);

extern template bool ComponentStorage::has_component<Animation>(Entity) const;

extern template std::vector<Entity> ComponentStorage::entities_with_component<Animation>() const;

extern template void ComponentStorage::add_component<EnemyTag>(Entity, const EnemyTag&);

extern template std::optional<std::reference_wrapper<EnemyTag>> ComponentStorage::get_component<EnemyTag>(Entity);

extern template std::optional<std::reference_wrapper<const EnemyTag>> ComponentStorage::get_component<EnemyTag>(Entity) const;

extern template void ComponentStorage::remove_component<EnemyTag>(Entity);

extern template bool ComponentStorage::has_component<EnemyTag>(Entity) const;

extern template std::vector<Entity> ComponentStorage::entities_with_component<EnemyTag>() const;

extern template void ComponentStorage::add_component<PathFollower>(Entity, const PathFollower&);

extern template std::optional<std::reference_wrapper<PathFollower>> ComponentStorage::get_component<PathFollower>(Entity);

extern template std::optional<std::reference_wrapper<const PathFollower>> ComponentStorage::get_component<PathFollower>(Entity) const;

extern template void ComponentStorage::remove_component<PathFollower>(Entity);

extern template bool ComponentStorage::has_component<PathFollower>(Entity) const;

extern template std::vector<Entity> ComponentStorage::entities_with_component<PathFollower>() const;

extern template void ComponentStorage::add_component<Health>(Entity, const Health&);

extern template std::optional<std::reference_wrapper<Health>> ComponentStorage::get_component<Health>(Entity);

extern template std::optional<std::reference_wrapper<const Health>> ComponentStorage::get_component<Health>(Entity) const;

extern template void ComponentStorage::remove_component<Health>(Entity);

extern template bool ComponentStorage::has_component<Health>(Entity) const;

extern template std::vector<Entity> ComponentStorage::entities_with_component<Health>() const;

extern template void ComponentStorage::add_component<TowerTag>(Entity, const TowerTag&);

extern template std::optional<std::reference_wrapper<TowerTag>> ComponentStorage::get_component<TowerTag>(Entity);

extern template std::optional<std::reference_wrapper<const TowerTag>> ComponentStorage::get_component<TowerTag>(Entity) const;

extern template void ComponentStorage::remove_component<TowerTag>(Entity);

extern template bool ComponentStorage::has_component<TowerTag>(Entity) const;

extern template std::vector<Entity> ComponentStorage::entities_with_component<TowerTag>() const;

extern template void ComponentStorage::add_component<TowerStats>(Entity, const TowerStats&);

extern template std::optional<std::reference_wrapper<TowerStats>> ComponentStorage::get_component<TowerStats>(Entity);

extern template std::optional<std::reference_wrapper<const TowerStats>> ComponentStorage::get_component<TowerStats>(Entity) const;

extern template void ComponentStorage::remove_component<TowerStats>(Entity);

extern template bool ComponentStorage::has_component<TowerStats>(Entity) const;

extern template std::vector<Entity> ComponentStorage::entities_with_component<TowerStats>() const;

extern template void ComponentStorage::add_component<TowerAnimationClips>(Entity, const TowerAnimationClips&);

extern template std::optional<std::reference_wrapper<TowerAnimationClips>> ComponentStorage::get_component<TowerAnimationClips>(Entity);

extern template std::optional<std::reference_wrapper<const TowerAnimationClips>> ComponentStorage::get_component<TowerAnimationClips>(Entity) const;

extern template void ComponentStorage::remove_component<TowerAnimationClips>(Entity);

extern template bool ComponentStorage::has_component<TowerAnimationClips>(Entity) const;

extern template std::vector<Entity> ComponentStorage::entities_with_component<TowerAnimationClips>() const;

extern template void ComponentStorage::add_component<TowerAiming>(Entity, const TowerAiming&);

extern template std::optional<std::reference_wrapper<TowerAiming>> ComponentStorage::get_component<TowerAiming>(Entity);

extern template std::optional<std::reference_wrapper<const TowerAiming>> ComponentStorage::get_component<TowerAiming>(Entity) const;

extern template void ComponentStorage::remove_component<TowerAiming>(Entity);

extern template bool ComponentStorage::has_component<TowerAiming>(Entity) const;

extern template std::vector<Entity> ComponentStorage::entities_with_component<TowerAiming>() const;

extern template void ComponentStorage::add_component<RenderLayer>(Entity, const RenderLayer&);
extern template std::optional<std::reference_wrapper<RenderLayer>> ComponentStorage::get_component<RenderLayer>(Entity);
extern template std::optional<std::reference_wrapper<const RenderLayer>> ComponentStorage::get_component<RenderLayer>(Entity) const;
extern template void ComponentStorage::remove_component<RenderLayer>(Entity);
extern template bool ComponentStorage::has_component<RenderLayer>(Entity) const;
extern template std::vector<Entity> ComponentStorage::entities_with_component<RenderLayer>() const;

extern template void ComponentStorage::add_component<BeamTag>(Entity, const BeamTag&);
extern template std::optional<std::reference_wrapper<BeamTag>> ComponentStorage::get_component<BeamTag>(Entity);
extern template std::optional<std::reference_wrapper<const BeamTag>> ComponentStorage::get_component<BeamTag>(Entity) const;
extern template void ComponentStorage::remove_component<BeamTag>(Entity);
extern template bool ComponentStorage::has_component<BeamTag>(Entity) const;
extern template std::vector<Entity> ComponentStorage::entities_with_component<BeamTag>() const;

extern template void ComponentStorage::add_component<HealthBarTag>(Entity, const HealthBarTag&);
extern template std::optional<std::reference_wrapper<HealthBarTag>> ComponentStorage::get_component<HealthBarTag>(Entity);
extern template std::optional<std::reference_wrapper<const HealthBarTag>> ComponentStorage::get_component<HealthBarTag>(Entity) const;
extern template void ComponentStorage::remove_component<HealthBarTag>(Entity);
extern template bool ComponentStorage::has_component<HealthBarTag>(Entity) const;
extern template std::vector<Entity> ComponentStorage::entities_with_component<HealthBarTag>() const;

extern template void ComponentStorage::add_component<ProjectileTag>(Entity, const ProjectileTag&);

extern template std::optional<std::reference_wrapper<ProjectileTag>> ComponentStorage::get_component<ProjectileTag>(Entity);

extern template std::optional<std::reference_wrapper<const ProjectileTag>> ComponentStorage::get_component<ProjectileTag>(Entity) const;

extern template void ComponentStorage::remove_component<ProjectileTag>(Entity);

extern template bool ComponentStorage::has_component<ProjectileTag>(Entity) const;

extern template std::vector<Entity> ComponentStorage::entities_with_component<ProjectileTag>() const;

extern template void ComponentStorage::add_component<ProjectileData>(Entity, const ProjectileData&);

extern template std::optional<std::reference_wrapper<ProjectileData>> ComponentStorage::get_component<ProjectileData>(Entity);

extern template std::optional<std::reference_wrapper<const ProjectileData>> ComponentStorage::get_component<ProjectileData>(Entity) const;

extern template void ComponentStorage::remove_component<ProjectileData>(Entity);

extern template bool ComponentStorage::has_component<ProjectileData>(Entity) const;

extern template std::vector<Entity> ComponentStorage::entities_with_component<ProjectileData>() const;

extern template void ComponentStorage::add_component<CannonFireRequest>(Entity, const CannonFireRequest&);

extern template std::optional<std::reference_wrapper<CannonFireRequest>> ComponentStorage::get_component<CannonFireRequest>(Entity);

extern template std::optional<std::reference_wrapper<const CannonFireRequest>> ComponentStorage::get_component<CannonFireRequest>(Entity) const;

extern template void ComponentStorage::remove_component<CannonFireRequest>(Entity);

extern template bool ComponentStorage::has_component<CannonFireRequest>(Entity) const;

extern template std::vector<Entity> ComponentStorage::entities_with_component<CannonFireRequest>() const;

extern template void ComponentStorage::add_component<LaserFireRequest>(Entity, const LaserFireRequest&);

extern template std::optional<std::reference_wrapper<LaserFireRequest>> ComponentStorage::get_component<LaserFireRequest>(Entity);

extern template std::optional<std::reference_wrapper<const LaserFireRequest>> ComponentStorage::get_component<LaserFireRequest>(Entity) const;

extern template void ComponentStorage::remove_component<LaserFireRequest>(Entity);

extern template bool ComponentStorage::has_component<LaserFireRequest>(Entity) const;

extern template std::vector<Entity> ComponentStorage::entities_with_component<LaserFireRequest>() const;

extern template void ComponentStorage::add_component<LaserFiring>(Entity, const LaserFiring&);

extern template std::optional<std::reference_wrapper<LaserFiring>> ComponentStorage::get_component<LaserFiring>(Entity);

extern template std::optional<std::reference_wrapper<const LaserFiring>> ComponentStorage::get_component<LaserFiring>(Entity) const;

extern template void ComponentStorage::remove_component<LaserFiring>(Entity);

extern template bool ComponentStorage::has_component<LaserFiring>(Entity) const;

extern template std::vector<Entity> ComponentStorage::entities_with_component<LaserFiring>() const;

extern template void ComponentStorage::add_component<DamageEvent>(Entity, const DamageEvent&);

extern template std::optional<std::reference_wrapper<DamageEvent>> ComponentStorage::get_component<DamageEvent>(Entity);

extern template std::optional<std::reference_wrapper<const DamageEvent>> ComponentStorage::get_component<DamageEvent>(Entity) const;

extern template void ComponentStorage::remove_component<DamageEvent>(Entity);

extern template bool ComponentStorage::has_component<DamageEvent>(Entity) const;

extern template std::vector<Entity> ComponentStorage::entities_with_component<DamageEvent>() const;

// Class-110 game components (PlayerTag, ShipState, Pickup, ContactDamage, WeaponStats).
#define CS110_EXTERN(T) \
    extern template void ComponentStorage::add_component<T>(Entity, const T&); \
    extern template std::optional<std::reference_wrapper<T>> ComponentStorage::get_component<T>(Entity); \
    extern template std::optional<std::reference_wrapper<const T>> ComponentStorage::get_component<T>(Entity) const; \
    extern template void ComponentStorage::remove_component<T>(Entity); \
    extern template bool ComponentStorage::has_component<T>(Entity) const; \
    extern template std::vector<Entity> ComponentStorage::entities_with_component<T>() const;
CS110_EXTERN(Tint)             // engine components (v2); registered via the shared macro
CS110_EXTERN(Particle)
CS110_EXTERN(ParticleEmitter)
CS110_EXTERN(PlayerTag)
CS110_EXTERN(ShipState)
CS110_EXTERN(Pickup)
CS110_EXTERN(ContactDamage)
CS110_EXTERN(WeaponStats)
CS110_EXTERN(Flash)
CS110_EXTERN(EnemyShot)        // Iteration 3 (D51)
CS110_EXTERN(EnemyBehavior)
CS110_EXTERN(Burn)             // gameplay pack v2.3 tier 3 (D221)
CS110_EXTERN(Chill)
CS110_EXTERN(BlizzardTag)
#undef CS110_EXTERN

#endif // COMPONENT_STORAGE_HPP
