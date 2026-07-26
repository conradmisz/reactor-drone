#ifndef ENEMY_SEEK_SYSTEM_HPP
#define ENEMY_SEEK_SYSTEM_HPP

#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"
#include "engine/tile_map.hpp"
#include "arena_config.hpp"   // ObstacleDef

#include <memory>
#include <vector>

/**
 * EnemySeekSystem — steers every enemy's Velocity toward the player (v2, Phase 7).
 *
 * Fast path: when line-of-sight to the player is clear, each enemy points its
 * velocity straight at the player's centre at its own speed (PathFollower.speed).
 * When an obstacle blocks LOS it falls back to the engine's A* (find_path over a
 * rasterised obstacle grid), steering toward the next path cell and only
 * recomputing every `repath_interval` seconds (per-enemy, tracked on PathFollower)
 * to keep the cost down. The engine MovementSystem then moves it. No-op when there
 * is no player, and reduces to pure straight seek when no arena grid is set.
 */
class EnemySeekSystem {
public:
    // Hand over the active arena's obstacle layout + prebuilt walkability grid
    // (call on each arena swap). nullptr / empty obstacles => straight seek only.
    void set_arena(const std::vector<ObstacleDef>* obstacles,
                   std::shared_ptr<TileMap> grid) {
        obstacles_ = obstacles;
        grid_ = std::move(grid);
    }
    void set_repath_interval(float seconds) { repath_interval_ = seconds; }

    void update(ComponentStorage& storage, Blackboard& blackboard);

private:
    const std::vector<ObstacleDef>* obstacles_ = nullptr;
    std::shared_ptr<TileMap> grid_;
    float repath_interval_ = 0.35f;
};

#endif // ENEMY_SEEK_SYSTEM_HPP
