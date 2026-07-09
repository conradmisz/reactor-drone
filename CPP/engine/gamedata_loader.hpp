#ifndef GAMEDATA_LOADER_HPP
#define GAMEDATA_LOADER_HPP

#include <string>
#include <vector>
#include <utility>
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"

/// Load entities and configuration from a GameData.json file.
/// Creates entities, attaches components, and populates Blackboard.
/// Throws std::runtime_error on file-not-found, invalid JSON, or missing entity ID.
void load_game_data(const std::string& file_path,
                    EntityManager& entity_manager,
                    ComponentStorage& component_storage,
                    Blackboard& blackboard);

/// Serialize ECS state back to GameData.json format.
/// Used for round-trip property testing.
/// entity_ids: vector of (string_id, Entity) pairs to serialize.
std::string serialize_game_data(
    const ComponentStorage& component_storage,
    const Blackboard& blackboard,
    const std::vector<std::pair<std::string, Entity>>& entity_ids);

#endif // GAMEDATA_LOADER_HPP
