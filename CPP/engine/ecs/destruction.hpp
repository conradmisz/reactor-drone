#ifndef DESTRUCTION_HPP
#define DESTRUCTION_HPP

class EntityManager;
class ComponentStorage;

/**
 * Destroys all entities marked with a DestroyRequest component.
 *
 * For each marked entity, every registered component type is removed from
 * ComponentStorage and the entity ID is recycled via EntityManager::destroy_entity().
 * The function iterates a snapshot (vector copy) returned by
 * entities_with_component<DestroyRequest>(), so removing components during
 * iteration is safe.
 *
 * Call this once per frame, after all systems have run and before the next
 * frame begins.
 *
 * @param em      The EntityManager that owns entity lifecycles
 * @param storage The ComponentStorage that holds all component data
 */
void destroy_marked_entities(EntityManager& em, ComponentStorage& storage);

#endif // DESTRUCTION_HPP
