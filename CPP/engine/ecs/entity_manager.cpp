#include "entity_manager.hpp"

Entity EntityManager::create_entity() {
    Entity new_entity;
    
    // Strategy: Reuse IDs from destroyed entities if available, otherwise generate new ID
    if (!available_ids_.empty()) {
        // Reuse a recycled ID from the pool
        new_entity = available_ids_.back();
        available_ids_.pop_back();
    } else {
        // No recycled IDs available, generate a new one
        new_entity = next_id_;
        next_id_++;
    }
    
    // Mark this entity as active
    active_entities_.insert(new_entity);
    
    return new_entity;
}

void EntityManager::destroy_entity(Entity entity) {
    // Check if the entity is actually active
    auto it = active_entities_.find(entity);
    if (it == active_entities_.end()) {
        // Entity is not active, nothing to do (safe no-op)
        return;
    }
    
    // Remove from active set
    active_entities_.erase(it);
    
    // Add to available pool for reuse
    available_ids_.push_back(entity);
}

bool EntityManager::is_alive(Entity entity) const {
    // O(1) lookup in the active entities set
    return active_entities_.find(entity) != active_entities_.end();
}

size_t EntityManager::active_count() const {
    // The size of the active set is the count of active entities
    // Invariant: This should always equal (total creates - total destroys)
    return active_entities_.size();
}

std::vector<Entity> EntityManager::all_entities() const {
    // Copy the active set into a vector for safe iteration/bulk operations
    // (e.g. marking every entity for destruction on a level reload/restart).
    return std::vector<Entity>(active_entities_.begin(), active_entities_.end());
}
