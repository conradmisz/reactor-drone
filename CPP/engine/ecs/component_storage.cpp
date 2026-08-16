#include "component_storage.hpp"

// Template specializations for get_storage() - maps component types to storage maps

template<>
std::unordered_map<Entity, Position>& ComponentStorage::get_storage<Position>() {
    return positions_;
}

template<>
std::unordered_map<Entity, Size>& ComponentStorage::get_storage<Size>() {
    return sizes_;
}

template<>
std::unordered_map<Entity, Color>& ComponentStorage::get_storage<Color>() {
    return colors_;
}

template<>
const std::unordered_map<Entity, Position>& ComponentStorage::get_storage<Position>() const {
    return positions_;
}

template<>
const std::unordered_map<Entity, Size>& ComponentStorage::get_storage<Size>() const {
    return sizes_;
}

template<>
const std::unordered_map<Entity, Color>& ComponentStorage::get_storage<Color>() const {
    return colors_;
}

template<>
std::unordered_map<Entity, Input>& ComponentStorage::get_storage<Input>() {
    return inputs_;
}

template<>
const std::unordered_map<Entity, Input>& ComponentStorage::get_storage<Input>() const {
    return inputs_;
}

template<>
std::unordered_map<Entity, Velocity>& ComponentStorage::get_storage<Velocity>() {
    return velocities_;
}

template<>
const std::unordered_map<Entity, Velocity>& ComponentStorage::get_storage<Velocity>() const {
    return velocities_;
}

template<>
std::unordered_map<Entity, Images>& ComponentStorage::get_storage<Images>() {
    return images_;
}

template<>
const std::unordered_map<Entity, Images>& ComponentStorage::get_storage<Images>() const {
    return images_;
}

template<>
std::unordered_map<Entity, Text>& ComponentStorage::get_storage<Text>() {
    return texts_;
}

template<>
const std::unordered_map<Entity, Text>& ComponentStorage::get_storage<Text>() const {
    return texts_;
}

template<>
std::unordered_map<Entity, ScreenPosition>& ComponentStorage::get_storage<ScreenPosition>() {
    return screen_positions_;
}

template<>
const std::unordered_map<Entity, ScreenPosition>& ComponentStorage::get_storage<ScreenPosition>() const {
    return screen_positions_;
}

template<>
std::unordered_map<Entity, Rotation>& ComponentStorage::get_storage<Rotation>() {
    return rotations_;
}

template<>
const std::unordered_map<Entity, Rotation>& ComponentStorage::get_storage<Rotation>() const {
    return rotations_;
}

template<>
std::unordered_map<Entity, Collider>& ComponentStorage::get_storage<Collider>() {
    return colliders_;
}

template<>
const std::unordered_map<Entity, Collider>& ComponentStorage::get_storage<Collider>() const {
    return colliders_;
}

template<>
std::unordered_map<Entity, CircleCollider>& ComponentStorage::get_storage<CircleCollider>() {
    return circle_colliders_;
}

template<>
const std::unordered_map<Entity, CircleCollider>& ComponentStorage::get_storage<CircleCollider>() const {
    return circle_colliders_;
}

template<>
std::unordered_map<Entity, OBBCollider>& ComponentStorage::get_storage<OBBCollider>() {
    return obb_colliders_;
}

template<>
const std::unordered_map<Entity, OBBCollider>& ComponentStorage::get_storage<OBBCollider>() const {
    return obb_colliders_;
}

template<>
std::unordered_map<Entity, Lifetime>& ComponentStorage::get_storage<Lifetime>() {
    return lifetimes_;
}

template<>
const std::unordered_map<Entity, Lifetime>& ComponentStorage::get_storage<Lifetime>() const {
    return lifetimes_;
}

template<>
std::unordered_map<Entity, WrapAround>& ComponentStorage::get_storage<WrapAround>() {
    return wrap_arounds_;
}

template<>
const std::unordered_map<Entity, WrapAround>& ComponentStorage::get_storage<WrapAround>() const {
    return wrap_arounds_;
}

template<>
std::unordered_map<Entity, DestroyRequest>& ComponentStorage::get_storage<DestroyRequest>() {
    return destroy_requests_;
}

template<>
const std::unordered_map<Entity, DestroyRequest>& ComponentStorage::get_storage<DestroyRequest>() const {
    return destroy_requests_;
}

template<>
std::unordered_map<Entity, CollidedWith>& ComponentStorage::get_storage<CollidedWith>() {
    return collided_withs_;
}

template<>
const std::unordered_map<Entity, CollidedWith>& ComponentStorage::get_storage<CollidedWith>() const {
    return collided_withs_;
}

template<>
std::unordered_map<Entity, Script>& ComponentStorage::get_storage<Script>() {
    return scripts_;
}

template<>
const std::unordered_map<Entity, Script>& ComponentStorage::get_storage<Script>() const {
    return scripts_;
}

template<>
std::unordered_map<Entity, SpriteSheet>& ComponentStorage::get_storage<SpriteSheet>() {
    return sprite_sheets_;
}

template<>
const std::unordered_map<Entity, SpriteSheet>& ComponentStorage::get_storage<SpriteSheet>() const {
    return sprite_sheets_;
}

template<>
std::unordered_map<Entity, Animation>& ComponentStorage::get_storage<Animation>() {
    return animations_;
}

template<>
const std::unordered_map<Entity, Animation>& ComponentStorage::get_storage<Animation>() const {
    return animations_;
}

template<>
std::unordered_map<Entity, EnemyTag>& ComponentStorage::get_storage<EnemyTag>() {
    return enemy_tags_;
}

template<>
const std::unordered_map<Entity, EnemyTag>& ComponentStorage::get_storage<EnemyTag>() const {
    return enemy_tags_;
}

template<>
std::unordered_map<Entity, PathFollower>& ComponentStorage::get_storage<PathFollower>() {
    return path_followers_;
}

template<>
const std::unordered_map<Entity, PathFollower>& ComponentStorage::get_storage<PathFollower>() const {
    return path_followers_;
}

template<>
std::unordered_map<Entity, Health>& ComponentStorage::get_storage<Health>() {
    return healths_;
}

template<>
const std::unordered_map<Entity, Health>& ComponentStorage::get_storage<Health>() const {
    return healths_;
}

template<>
std::unordered_map<Entity, TowerTag>& ComponentStorage::get_storage<TowerTag>() {
    return tower_tags_;
}

template<>
const std::unordered_map<Entity, TowerTag>& ComponentStorage::get_storage<TowerTag>() const {
    return tower_tags_;
}

template<>
std::unordered_map<Entity, TowerStats>& ComponentStorage::get_storage<TowerStats>() {
    return tower_stats_;
}

template<>
const std::unordered_map<Entity, TowerStats>& ComponentStorage::get_storage<TowerStats>() const {
    return tower_stats_;
}

template<>
std::unordered_map<Entity, TowerAnimationClips>& ComponentStorage::get_storage<TowerAnimationClips>() {
    return tower_animation_clips_;
}

template<>
const std::unordered_map<Entity, TowerAnimationClips>& ComponentStorage::get_storage<TowerAnimationClips>() const {
    return tower_animation_clips_;
}

template<>
std::unordered_map<Entity, TowerAiming>& ComponentStorage::get_storage<TowerAiming>() {
    return tower_aiming_;
}

template<>
const std::unordered_map<Entity, TowerAiming>& ComponentStorage::get_storage<TowerAiming>() const {
    return tower_aiming_;
}

template<>
std::unordered_map<Entity, RenderLayer>& ComponentStorage::get_storage<RenderLayer>() {
    return render_layers_;
}

template<>
const std::unordered_map<Entity, RenderLayer>& ComponentStorage::get_storage<RenderLayer>() const {
    return render_layers_;
}

// UI & menu layer (Option-040 port).
template<>
std::unordered_map<Entity, UIElement>& ComponentStorage::get_storage<UIElement>() { return ui_elements_; }
template<>
const std::unordered_map<Entity, UIElement>& ComponentStorage::get_storage<UIElement>() const { return ui_elements_; }

template<>
std::unordered_map<Entity, UIState>& ComponentStorage::get_storage<UIState>() { return ui_states_; }
template<>
const std::unordered_map<Entity, UIState>& ComponentStorage::get_storage<UIState>() const { return ui_states_; }

template<>
std::unordered_map<Entity, UIScreen>& ComponentStorage::get_storage<UIScreen>() { return ui_screens_; }
template<>
const std::unordered_map<Entity, UIScreen>& ComponentStorage::get_storage<UIScreen>() const { return ui_screens_; }

template<>
std::unordered_map<Entity, ScreenMembership>& ComponentStorage::get_storage<ScreenMembership>() { return screen_memberships_; }
template<>
const std::unordered_map<Entity, ScreenMembership>& ComponentStorage::get_storage<ScreenMembership>() const { return screen_memberships_; }

template<>
std::unordered_map<Entity, Tint>& ComponentStorage::get_storage<Tint>() { return tints_; }
template<>
const std::unordered_map<Entity, Tint>& ComponentStorage::get_storage<Tint>() const { return tints_; }

template<>
std::unordered_map<Entity, Particle>& ComponentStorage::get_storage<Particle>() { return particles_; }
template<>
const std::unordered_map<Entity, Particle>& ComponentStorage::get_storage<Particle>() const { return particles_; }

template<>
std::unordered_map<Entity, ParticleEmitter>& ComponentStorage::get_storage<ParticleEmitter>() { return particle_emitters_; }
template<>
const std::unordered_map<Entity, ParticleEmitter>& ComponentStorage::get_storage<ParticleEmitter>() const { return particle_emitters_; }

template<>
std::unordered_map<Entity, BeamTag>& ComponentStorage::get_storage<BeamTag>() {
    return beam_tags_;
}

template<>
const std::unordered_map<Entity, BeamTag>& ComponentStorage::get_storage<BeamTag>() const {
    return beam_tags_;
}

template<>
std::unordered_map<Entity, HealthBarTag>& ComponentStorage::get_storage<HealthBarTag>() {
    return health_bar_tags_;
}

template<>
const std::unordered_map<Entity, HealthBarTag>& ComponentStorage::get_storage<HealthBarTag>() const {
    return health_bar_tags_;
}

template<>
std::unordered_map<Entity, ProjectileTag>& ComponentStorage::get_storage<ProjectileTag>() {
    return projectile_tags_;
}

template<>
const std::unordered_map<Entity, ProjectileTag>& ComponentStorage::get_storage<ProjectileTag>() const {
    return projectile_tags_;
}

template<>
std::unordered_map<Entity, ProjectileData>& ComponentStorage::get_storage<ProjectileData>() {
    return projectile_datas_;
}

template<>
const std::unordered_map<Entity, ProjectileData>& ComponentStorage::get_storage<ProjectileData>() const {
    return projectile_datas_;
}

template<>
std::unordered_map<Entity, CannonFireRequest>& ComponentStorage::get_storage<CannonFireRequest>() {
    return cannon_fire_requests_;
}

template<>
const std::unordered_map<Entity, CannonFireRequest>& ComponentStorage::get_storage<CannonFireRequest>() const {
    return cannon_fire_requests_;
}

template<>
std::unordered_map<Entity, LaserFireRequest>& ComponentStorage::get_storage<LaserFireRequest>() {
    return laser_fire_requests_;
}

template<>
const std::unordered_map<Entity, LaserFireRequest>& ComponentStorage::get_storage<LaserFireRequest>() const {
    return laser_fire_requests_;
}

template<>
std::unordered_map<Entity, LaserFiring>& ComponentStorage::get_storage<LaserFiring>() {
    return laser_firings_;
}

template<>
const std::unordered_map<Entity, LaserFiring>& ComponentStorage::get_storage<LaserFiring>() const {
    return laser_firings_;
}

template<>
std::unordered_map<Entity, DamageEvent>& ComponentStorage::get_storage<DamageEvent>() {
    return damage_events_;
}

template<>
const std::unordered_map<Entity, DamageEvent>& ComponentStorage::get_storage<DamageEvent>() const {
    return damage_events_;
}

// Template method implementations

template<typename T>
void ComponentStorage::add_component(Entity entity, const T& component) {
    // Get the storage map for this component type
    auto& storage = get_storage<T>();
    
    // Insert or update the component
    // If the entity already has this component type, this replaces it
    storage[entity] = component;
}

template<typename T>
std::optional<std::reference_wrapper<T>> ComponentStorage::get_component(Entity entity) {
    // Get the storage map for this component type
    auto& storage = get_storage<T>();
    
    // Look up the entity in the storage map
    auto it = storage.find(entity);
    
    if (it != storage.end()) {
        // Component found - return a reference wrapper to it
        return std::ref(it->second);
    } else {
        // Component not found - return empty optional
        return std::nullopt;
    }
}

template<typename T>
std::optional<std::reference_wrapper<const T>> ComponentStorage::get_component(Entity entity) const {
    // Get the storage map for this component type
    const auto& storage = get_storage<T>();
    
    // Look up the entity in the storage map
    auto it = storage.find(entity);
    
    if (it != storage.end()) {
        // Component found - return a const reference wrapper to it
        return std::cref(it->second);
    } else {
        // Component not found - return empty optional
        return std::nullopt;
    }
}

template<typename T>
void ComponentStorage::remove_component(Entity entity) {
    // Get the storage map for this component type
    auto& storage = get_storage<T>();
    
    // Remove the entity from the storage map
    // If the entity doesn't have this component, erase() is a no-op
    storage.erase(entity);
}

template<typename T>
bool ComponentStorage::has_component(Entity entity) const {
    // Get the storage map for this component type
    const auto& storage = get_storage<T>();
    
    // Check if the entity exists in the storage map
    return storage.find(entity) != storage.end();
}

template<typename T>
std::vector<Entity> ComponentStorage::entities_with_component() const {
    // Get the storage map for this component type
    const auto& storage = get_storage<T>();
    
    // Collect all entity IDs from the storage map
    std::vector<Entity> entities;
    entities.reserve(storage.size());  // Pre-allocate for efficiency
    
    for (const auto& [entity, component] : storage) {
        entities.push_back(entity);
    }
    
    return entities;
}

// Explicit template instantiations for the three component types
// This ensures the template methods are compiled for Position, Size, and Color

template void ComponentStorage::add_component<Position>(Entity, const Position&);
template void ComponentStorage::add_component<Size>(Entity, const Size&);
template void ComponentStorage::add_component<Color>(Entity, const Color&);
template void ComponentStorage::add_component<Input>(Entity, const Input&);
template void ComponentStorage::add_component<Velocity>(Entity, const Velocity&);

template void ComponentStorage::add_component<Images>(Entity, const Images&);

template void ComponentStorage::add_component<Text>(Entity, const Text&);
template void ComponentStorage::add_component<ScreenPosition>(Entity, const ScreenPosition&);

template void ComponentStorage::add_component<Rotation>(Entity, const Rotation&);
template void ComponentStorage::add_component<Collider>(Entity, const Collider&);
template void ComponentStorage::add_component<CircleCollider>(Entity, const CircleCollider&);
template void ComponentStorage::add_component<OBBCollider>(Entity, const OBBCollider&);
template void ComponentStorage::add_component<Lifetime>(Entity, const Lifetime&);
template void ComponentStorage::add_component<WrapAround>(Entity, const WrapAround&);
template void ComponentStorage::add_component<DestroyRequest>(Entity, const DestroyRequest&);
template void ComponentStorage::add_component<CollidedWith>(Entity, const CollidedWith&);
template void ComponentStorage::add_component<Script>(Entity, const Script&);
template void ComponentStorage::add_component<SpriteSheet>(Entity, const SpriteSheet&);
template void ComponentStorage::add_component<Animation>(Entity, const Animation&);
template void ComponentStorage::add_component<EnemyTag>(Entity, const EnemyTag&);
template void ComponentStorage::add_component<PathFollower>(Entity, const PathFollower&);
template void ComponentStorage::add_component<Health>(Entity, const Health&);
template void ComponentStorage::add_component<TowerTag>(Entity, const TowerTag&);
template void ComponentStorage::add_component<TowerStats>(Entity, const TowerStats&);
template void ComponentStorage::add_component<ProjectileTag>(Entity, const ProjectileTag&);
template void ComponentStorage::add_component<ProjectileData>(Entity, const ProjectileData&);
template void ComponentStorage::add_component<CannonFireRequest>(Entity, const CannonFireRequest&);
template void ComponentStorage::add_component<LaserFireRequest>(Entity, const LaserFireRequest&);
template void ComponentStorage::add_component<LaserFiring>(Entity, const LaserFiring&);
template void ComponentStorage::add_component<DamageEvent>(Entity, const DamageEvent&);

template std::optional<std::reference_wrapper<Position>> ComponentStorage::get_component<Position>(Entity);
template std::optional<std::reference_wrapper<Size>> ComponentStorage::get_component<Size>(Entity);
template std::optional<std::reference_wrapper<Color>> ComponentStorage::get_component<Color>(Entity);
template std::optional<std::reference_wrapper<Input>> ComponentStorage::get_component<Input>(Entity);
template std::optional<std::reference_wrapper<Velocity>> ComponentStorage::get_component<Velocity>(Entity);

template std::optional<std::reference_wrapper<Images>> ComponentStorage::get_component<Images>(Entity);

template std::optional<std::reference_wrapper<Text>> ComponentStorage::get_component<Text>(Entity);
template std::optional<std::reference_wrapper<ScreenPosition>> ComponentStorage::get_component<ScreenPosition>(Entity);

template std::optional<std::reference_wrapper<Rotation>> ComponentStorage::get_component<Rotation>(Entity);
template std::optional<std::reference_wrapper<Collider>> ComponentStorage::get_component<Collider>(Entity);
template std::optional<std::reference_wrapper<CircleCollider>> ComponentStorage::get_component<CircleCollider>(Entity);
template std::optional<std::reference_wrapper<OBBCollider>> ComponentStorage::get_component<OBBCollider>(Entity);
template std::optional<std::reference_wrapper<Lifetime>> ComponentStorage::get_component<Lifetime>(Entity);
template std::optional<std::reference_wrapper<WrapAround>> ComponentStorage::get_component<WrapAround>(Entity);
template std::optional<std::reference_wrapper<DestroyRequest>> ComponentStorage::get_component<DestroyRequest>(Entity);
template std::optional<std::reference_wrapper<CollidedWith>> ComponentStorage::get_component<CollidedWith>(Entity);
template std::optional<std::reference_wrapper<Script>> ComponentStorage::get_component<Script>(Entity);
template std::optional<std::reference_wrapper<SpriteSheet>> ComponentStorage::get_component<SpriteSheet>(Entity);
template std::optional<std::reference_wrapper<Animation>> ComponentStorage::get_component<Animation>(Entity);
template std::optional<std::reference_wrapper<EnemyTag>> ComponentStorage::get_component<EnemyTag>(Entity);
template std::optional<std::reference_wrapper<PathFollower>> ComponentStorage::get_component<PathFollower>(Entity);
template std::optional<std::reference_wrapper<Health>> ComponentStorage::get_component<Health>(Entity);
template std::optional<std::reference_wrapper<TowerTag>> ComponentStorage::get_component<TowerTag>(Entity);
template std::optional<std::reference_wrapper<TowerStats>> ComponentStorage::get_component<TowerStats>(Entity);
template std::optional<std::reference_wrapper<ProjectileTag>> ComponentStorage::get_component<ProjectileTag>(Entity);
template std::optional<std::reference_wrapper<ProjectileData>> ComponentStorage::get_component<ProjectileData>(Entity);
template std::optional<std::reference_wrapper<CannonFireRequest>> ComponentStorage::get_component<CannonFireRequest>(Entity);
template std::optional<std::reference_wrapper<LaserFireRequest>> ComponentStorage::get_component<LaserFireRequest>(Entity);
template std::optional<std::reference_wrapper<LaserFiring>> ComponentStorage::get_component<LaserFiring>(Entity);
template std::optional<std::reference_wrapper<DamageEvent>> ComponentStorage::get_component<DamageEvent>(Entity);

template std::optional<std::reference_wrapper<const Position>> ComponentStorage::get_component<Position>(Entity) const;
template std::optional<std::reference_wrapper<const Size>> ComponentStorage::get_component<Size>(Entity) const;
template std::optional<std::reference_wrapper<const Color>> ComponentStorage::get_component<Color>(Entity) const;
template std::optional<std::reference_wrapper<const Input>> ComponentStorage::get_component<Input>(Entity) const;
template std::optional<std::reference_wrapper<const Velocity>> ComponentStorage::get_component<Velocity>(Entity) const;

template std::optional<std::reference_wrapper<const Images>> ComponentStorage::get_component<Images>(Entity) const;

template std::optional<std::reference_wrapper<const Text>> ComponentStorage::get_component<Text>(Entity) const;
template std::optional<std::reference_wrapper<const ScreenPosition>> ComponentStorage::get_component<ScreenPosition>(Entity) const;

template std::optional<std::reference_wrapper<const Rotation>> ComponentStorage::get_component<Rotation>(Entity) const;
template std::optional<std::reference_wrapper<const Collider>> ComponentStorage::get_component<Collider>(Entity) const;
template std::optional<std::reference_wrapper<const CircleCollider>> ComponentStorage::get_component<CircleCollider>(Entity) const;
template std::optional<std::reference_wrapper<const OBBCollider>> ComponentStorage::get_component<OBBCollider>(Entity) const;
template std::optional<std::reference_wrapper<const Lifetime>> ComponentStorage::get_component<Lifetime>(Entity) const;
template std::optional<std::reference_wrapper<const WrapAround>> ComponentStorage::get_component<WrapAround>(Entity) const;
template std::optional<std::reference_wrapper<const DestroyRequest>> ComponentStorage::get_component<DestroyRequest>(Entity) const;
template std::optional<std::reference_wrapper<const CollidedWith>> ComponentStorage::get_component<CollidedWith>(Entity) const;
template std::optional<std::reference_wrapper<const Script>> ComponentStorage::get_component<Script>(Entity) const;
template std::optional<std::reference_wrapper<const SpriteSheet>> ComponentStorage::get_component<SpriteSheet>(Entity) const;
template std::optional<std::reference_wrapper<const Animation>> ComponentStorage::get_component<Animation>(Entity) const;
template std::optional<std::reference_wrapper<const EnemyTag>> ComponentStorage::get_component<EnemyTag>(Entity) const;
template std::optional<std::reference_wrapper<const PathFollower>> ComponentStorage::get_component<PathFollower>(Entity) const;
template std::optional<std::reference_wrapper<const Health>> ComponentStorage::get_component<Health>(Entity) const;
template std::optional<std::reference_wrapper<const TowerTag>> ComponentStorage::get_component<TowerTag>(Entity) const;
template std::optional<std::reference_wrapper<const TowerStats>> ComponentStorage::get_component<TowerStats>(Entity) const;
template std::optional<std::reference_wrapper<const ProjectileTag>> ComponentStorage::get_component<ProjectileTag>(Entity) const;
template std::optional<std::reference_wrapper<const ProjectileData>> ComponentStorage::get_component<ProjectileData>(Entity) const;
template std::optional<std::reference_wrapper<const CannonFireRequest>> ComponentStorage::get_component<CannonFireRequest>(Entity) const;
template std::optional<std::reference_wrapper<const LaserFireRequest>> ComponentStorage::get_component<LaserFireRequest>(Entity) const;
template std::optional<std::reference_wrapper<const LaserFiring>> ComponentStorage::get_component<LaserFiring>(Entity) const;
template std::optional<std::reference_wrapper<const DamageEvent>> ComponentStorage::get_component<DamageEvent>(Entity) const;

template void ComponentStorage::remove_component<Position>(Entity);
template void ComponentStorage::remove_component<Size>(Entity);
template void ComponentStorage::remove_component<Color>(Entity);
template void ComponentStorage::remove_component<Input>(Entity);
template void ComponentStorage::remove_component<Velocity>(Entity);

template void ComponentStorage::remove_component<Images>(Entity);

template void ComponentStorage::remove_component<Text>(Entity);
template void ComponentStorage::remove_component<ScreenPosition>(Entity);

template void ComponentStorage::remove_component<Rotation>(Entity);
template void ComponentStorage::remove_component<Collider>(Entity);
template void ComponentStorage::remove_component<CircleCollider>(Entity);
template void ComponentStorage::remove_component<OBBCollider>(Entity);
template void ComponentStorage::remove_component<Lifetime>(Entity);
template void ComponentStorage::remove_component<WrapAround>(Entity);
template void ComponentStorage::remove_component<DestroyRequest>(Entity);
template void ComponentStorage::remove_component<CollidedWith>(Entity);
template void ComponentStorage::remove_component<Script>(Entity);
template void ComponentStorage::remove_component<SpriteSheet>(Entity);
template void ComponentStorage::remove_component<Animation>(Entity);
template void ComponentStorage::remove_component<EnemyTag>(Entity);
template void ComponentStorage::remove_component<PathFollower>(Entity);
template void ComponentStorage::remove_component<Health>(Entity);
template void ComponentStorage::remove_component<TowerTag>(Entity);
template void ComponentStorage::remove_component<TowerStats>(Entity);
template void ComponentStorage::remove_component<ProjectileTag>(Entity);
template void ComponentStorage::remove_component<ProjectileData>(Entity);
template void ComponentStorage::remove_component<CannonFireRequest>(Entity);
template void ComponentStorage::remove_component<LaserFireRequest>(Entity);
template void ComponentStorage::remove_component<LaserFiring>(Entity);
template void ComponentStorage::remove_component<DamageEvent>(Entity);

template bool ComponentStorage::has_component<Position>(Entity) const;
template bool ComponentStorage::has_component<Size>(Entity) const;
template bool ComponentStorage::has_component<Color>(Entity) const;
template bool ComponentStorage::has_component<Input>(Entity) const;
template bool ComponentStorage::has_component<Velocity>(Entity) const;

template bool ComponentStorage::has_component<Images>(Entity) const;

template bool ComponentStorage::has_component<Text>(Entity) const;
template bool ComponentStorage::has_component<ScreenPosition>(Entity) const;

template bool ComponentStorage::has_component<Rotation>(Entity) const;
template bool ComponentStorage::has_component<Collider>(Entity) const;
template bool ComponentStorage::has_component<CircleCollider>(Entity) const;
template bool ComponentStorage::has_component<OBBCollider>(Entity) const;
template bool ComponentStorage::has_component<Lifetime>(Entity) const;
template bool ComponentStorage::has_component<WrapAround>(Entity) const;
template bool ComponentStorage::has_component<DestroyRequest>(Entity) const;
template bool ComponentStorage::has_component<CollidedWith>(Entity) const;
template bool ComponentStorage::has_component<Script>(Entity) const;
template bool ComponentStorage::has_component<SpriteSheet>(Entity) const;
template bool ComponentStorage::has_component<Animation>(Entity) const;
template bool ComponentStorage::has_component<EnemyTag>(Entity) const;
template bool ComponentStorage::has_component<PathFollower>(Entity) const;
template bool ComponentStorage::has_component<Health>(Entity) const;
template bool ComponentStorage::has_component<TowerTag>(Entity) const;
template bool ComponentStorage::has_component<TowerStats>(Entity) const;
template bool ComponentStorage::has_component<ProjectileTag>(Entity) const;
template bool ComponentStorage::has_component<ProjectileData>(Entity) const;
template bool ComponentStorage::has_component<CannonFireRequest>(Entity) const;
template bool ComponentStorage::has_component<LaserFireRequest>(Entity) const;
template bool ComponentStorage::has_component<LaserFiring>(Entity) const;
template bool ComponentStorage::has_component<DamageEvent>(Entity) const;

template std::vector<Entity> ComponentStorage::entities_with_component<Position>() const;
template std::vector<Entity> ComponentStorage::entities_with_component<Size>() const;
template std::vector<Entity> ComponentStorage::entities_with_component<Color>() const;
template std::vector<Entity> ComponentStorage::entities_with_component<Input>() const;
template std::vector<Entity> ComponentStorage::entities_with_component<Velocity>() const;

template std::vector<Entity> ComponentStorage::entities_with_component<Images>() const;

template std::vector<Entity> ComponentStorage::entities_with_component<Text>() const;
template std::vector<Entity> ComponentStorage::entities_with_component<ScreenPosition>() const;

template std::vector<Entity> ComponentStorage::entities_with_component<Rotation>() const;
template std::vector<Entity> ComponentStorage::entities_with_component<Collider>() const;
template std::vector<Entity> ComponentStorage::entities_with_component<CircleCollider>() const;
template std::vector<Entity> ComponentStorage::entities_with_component<OBBCollider>() const;
template std::vector<Entity> ComponentStorage::entities_with_component<Lifetime>() const;
template std::vector<Entity> ComponentStorage::entities_with_component<WrapAround>() const;
template std::vector<Entity> ComponentStorage::entities_with_component<DestroyRequest>() const;
template std::vector<Entity> ComponentStorage::entities_with_component<CollidedWith>() const;
template std::vector<Entity> ComponentStorage::entities_with_component<Script>() const;
template std::vector<Entity> ComponentStorage::entities_with_component<SpriteSheet>() const;
template std::vector<Entity> ComponentStorage::entities_with_component<Animation>() const;
template std::vector<Entity> ComponentStorage::entities_with_component<EnemyTag>() const;
template std::vector<Entity> ComponentStorage::entities_with_component<PathFollower>() const;
template std::vector<Entity> ComponentStorage::entities_with_component<Health>() const;
template std::vector<Entity> ComponentStorage::entities_with_component<TowerTag>() const;
template std::vector<Entity> ComponentStorage::entities_with_component<TowerStats>() const;
template std::vector<Entity> ComponentStorage::entities_with_component<ProjectileTag>() const;
template std::vector<Entity> ComponentStorage::entities_with_component<ProjectileData>() const;
template std::vector<Entity> ComponentStorage::entities_with_component<CannonFireRequest>() const;
template std::vector<Entity> ComponentStorage::entities_with_component<LaserFireRequest>() const;
template std::vector<Entity> ComponentStorage::entities_with_component<LaserFiring>() const;
template std::vector<Entity> ComponentStorage::entities_with_component<DamageEvent>() const;

// TowerAnimationClips — caches a tower's idle + fire clips for TowerAnimationSystem.
template void ComponentStorage::add_component<TowerAnimationClips>(Entity, const TowerAnimationClips&);
template std::optional<std::reference_wrapper<TowerAnimationClips>> ComponentStorage::get_component<TowerAnimationClips>(Entity);
template std::optional<std::reference_wrapper<const TowerAnimationClips>> ComponentStorage::get_component<TowerAnimationClips>(Entity) const;
template void ComponentStorage::remove_component<TowerAnimationClips>(Entity);
template bool ComponentStorage::has_component<TowerAnimationClips>(Entity) const;
template std::vector<Entity> ComponentStorage::entities_with_component<TowerAnimationClips>() const;

// TowerAiming — per-facing screen angles for directional tower aiming.
template void ComponentStorage::add_component<TowerAiming>(Entity, const TowerAiming&);
template std::optional<std::reference_wrapper<TowerAiming>> ComponentStorage::get_component<TowerAiming>(Entity);
template std::optional<std::reference_wrapper<const TowerAiming>> ComponentStorage::get_component<TowerAiming>(Entity) const;
template void ComponentStorage::remove_component<TowerAiming>(Entity);
template bool ComponentStorage::has_component<TowerAiming>(Entity) const;
template std::vector<Entity> ComponentStorage::entities_with_component<TowerAiming>() const;

// RenderLayer — draw-order hint consumed by RenderSystem.
template void ComponentStorage::add_component<RenderLayer>(Entity, const RenderLayer&);
template void ComponentStorage::add_component<UIElement>(Entity, const UIElement&);
template void ComponentStorage::add_component<UIState>(Entity, const UIState&);
template void ComponentStorage::add_component<UIScreen>(Entity, const UIScreen&);
template void ComponentStorage::add_component<ScreenMembership>(Entity, const ScreenMembership&);
template std::optional<std::reference_wrapper<RenderLayer>> ComponentStorage::get_component<RenderLayer>(Entity);
template std::optional<std::reference_wrapper<UIElement>> ComponentStorage::get_component<UIElement>(Entity);
template std::optional<std::reference_wrapper<UIState>> ComponentStorage::get_component<UIState>(Entity);
template std::optional<std::reference_wrapper<UIScreen>> ComponentStorage::get_component<UIScreen>(Entity);
template std::optional<std::reference_wrapper<ScreenMembership>> ComponentStorage::get_component<ScreenMembership>(Entity);
template std::optional<std::reference_wrapper<const RenderLayer>> ComponentStorage::get_component<RenderLayer>(Entity) const;
template std::optional<std::reference_wrapper<const UIElement>> ComponentStorage::get_component<UIElement>(Entity) const;
template std::optional<std::reference_wrapper<const UIState>> ComponentStorage::get_component<UIState>(Entity) const;
template std::optional<std::reference_wrapper<const UIScreen>> ComponentStorage::get_component<UIScreen>(Entity) const;
template std::optional<std::reference_wrapper<const ScreenMembership>> ComponentStorage::get_component<ScreenMembership>(Entity) const;
template void ComponentStorage::remove_component<RenderLayer>(Entity);
template void ComponentStorage::remove_component<UIElement>(Entity);
template void ComponentStorage::remove_component<UIState>(Entity);
template void ComponentStorage::remove_component<UIScreen>(Entity);
template void ComponentStorage::remove_component<ScreenMembership>(Entity);
template bool ComponentStorage::has_component<RenderLayer>(Entity) const;
template bool ComponentStorage::has_component<UIElement>(Entity) const;
template bool ComponentStorage::has_component<UIState>(Entity) const;
template bool ComponentStorage::has_component<UIScreen>(Entity) const;
template bool ComponentStorage::has_component<ScreenMembership>(Entity) const;
template std::vector<Entity> ComponentStorage::entities_with_component<RenderLayer>() const;
template std::vector<Entity> ComponentStorage::entities_with_component<UIElement>() const;
template std::vector<Entity> ComponentStorage::entities_with_component<UIState>() const;
template std::vector<Entity> ComponentStorage::entities_with_component<UIScreen>() const;
template std::vector<Entity> ComponentStorage::entities_with_component<ScreenMembership>() const;

// BeamTag — laser-beam entity marker.
template void ComponentStorage::add_component<BeamTag>(Entity, const BeamTag&);
template std::optional<std::reference_wrapper<BeamTag>> ComponentStorage::get_component<BeamTag>(Entity);
template std::optional<std::reference_wrapper<const BeamTag>> ComponentStorage::get_component<BeamTag>(Entity) const;
template void ComponentStorage::remove_component<BeamTag>(Entity);
template bool ComponentStorage::has_component<BeamTag>(Entity) const;
template std::vector<Entity> ComponentStorage::entities_with_component<BeamTag>() const;

// HealthBarTag — floating health-bar entity marker.
template void ComponentStorage::add_component<HealthBarTag>(Entity, const HealthBarTag&);
template std::optional<std::reference_wrapper<HealthBarTag>> ComponentStorage::get_component<HealthBarTag>(Entity);
template std::optional<std::reference_wrapper<const HealthBarTag>> ComponentStorage::get_component<HealthBarTag>(Entity) const;
template void ComponentStorage::remove_component<HealthBarTag>(Entity);
template bool ComponentStorage::has_component<HealthBarTag>(Entity) const;
template std::vector<Entity> ComponentStorage::entities_with_component<HealthBarTag>() const;

// ---------------------------------------------------------------------------
// Class-110 "Reactor Drone" game components: PlayerTag, ShipState, Pickup,
// ContactDamage, WeaponStats. get_storage() specializations + explicit
// instantiations, mirroring the pattern used for the tower-defense components.
// ---------------------------------------------------------------------------

template<>
std::unordered_map<Entity, PlayerTag>& ComponentStorage::get_storage<PlayerTag>() { return player_tags_; }
template<>
const std::unordered_map<Entity, PlayerTag>& ComponentStorage::get_storage<PlayerTag>() const { return player_tags_; }

template<>
std::unordered_map<Entity, ShipState>& ComponentStorage::get_storage<ShipState>() { return ship_states_; }
template<>
const std::unordered_map<Entity, ShipState>& ComponentStorage::get_storage<ShipState>() const { return ship_states_; }

template<>
std::unordered_map<Entity, Pickup>& ComponentStorage::get_storage<Pickup>() { return pickups_; }
template<>
const std::unordered_map<Entity, Pickup>& ComponentStorage::get_storage<Pickup>() const { return pickups_; }

template<>
std::unordered_map<Entity, ContactDamage>& ComponentStorage::get_storage<ContactDamage>() { return contact_damages_; }
template<>
const std::unordered_map<Entity, ContactDamage>& ComponentStorage::get_storage<ContactDamage>() const { return contact_damages_; }

template<>
std::unordered_map<Entity, WeaponStats>& ComponentStorage::get_storage<WeaponStats>() { return weapon_stats_; }
template<>
const std::unordered_map<Entity, WeaponStats>& ComponentStorage::get_storage<WeaponStats>() const { return weapon_stats_; }

template<>
std::unordered_map<Entity, Flash>& ComponentStorage::get_storage<Flash>() { return flashes_; }
template<>
const std::unordered_map<Entity, Flash>& ComponentStorage::get_storage<Flash>() const { return flashes_; }

// Iteration 3 (D51): enemy projectiles and non-seeker enemy behaviour.
template<>
std::unordered_map<Entity, EnemyShot>& ComponentStorage::get_storage<EnemyShot>() { return enemy_shots_; }
template<>
const std::unordered_map<Entity, EnemyShot>& ComponentStorage::get_storage<EnemyShot>() const { return enemy_shots_; }

template<>
std::unordered_map<Entity, EnemyBehavior>& ComponentStorage::get_storage<EnemyBehavior>() { return enemy_behaviors_; }
template<>
const std::unordered_map<Entity, EnemyBehavior>& ComponentStorage::get_storage<EnemyBehavior>() const { return enemy_behaviors_; }

// Gameplay pack v2.3 tier 3 (D221): secondary-fire status effects.
template<>
std::unordered_map<Entity, Burn>& ComponentStorage::get_storage<Burn>() { return burns_; }
template<>
const std::unordered_map<Entity, Burn>& ComponentStorage::get_storage<Burn>() const { return burns_; }
template<>
std::unordered_map<Entity, Chill>& ComponentStorage::get_storage<Chill>() { return chills_; }
template<>
const std::unordered_map<Entity, Chill>& ComponentStorage::get_storage<Chill>() const { return chills_; }
template<>
std::unordered_map<Entity, BlizzardTag>& ComponentStorage::get_storage<BlizzardTag>() { return blizzards_; }
template<>
const std::unordered_map<Entity, BlizzardTag>& ComponentStorage::get_storage<BlizzardTag>() const { return blizzards_; }

#define CS110_INSTANTIATE(T) \
    template void ComponentStorage::add_component<T>(Entity, const T&); \
    template std::optional<std::reference_wrapper<T>> ComponentStorage::get_component<T>(Entity); \
    template std::optional<std::reference_wrapper<const T>> ComponentStorage::get_component<T>(Entity) const; \
    template void ComponentStorage::remove_component<T>(Entity); \
    template bool ComponentStorage::has_component<T>(Entity) const; \
    template std::vector<Entity> ComponentStorage::entities_with_component<T>() const;
CS110_INSTANTIATE(Tint)             // engine components (v2); registered via the shared macro
CS110_INSTANTIATE(Particle)
CS110_INSTANTIATE(ParticleEmitter)
CS110_INSTANTIATE(PlayerTag)
CS110_INSTANTIATE(ShipState)
CS110_INSTANTIATE(Pickup)
CS110_INSTANTIATE(ContactDamage)
CS110_INSTANTIATE(WeaponStats)
CS110_INSTANTIATE(Flash)
CS110_INSTANTIATE(EnemyShot)        // Iteration 3 (D51)
CS110_INSTANTIATE(EnemyBehavior)
CS110_INSTANTIATE(Burn)             // gameplay pack v2.3 tier 3 (D221)
CS110_INSTANTIATE(Chill)
CS110_INSTANTIATE(BlizzardTag)
#undef CS110_INSTANTIATE
