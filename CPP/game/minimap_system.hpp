#ifndef MINIMAP_SYSTEM_HPP
#define MINIMAP_SYSTEM_HPP

#include <cstddef>
#include <vector>

#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "arena_config.hpp"   // MinimapConfig, ArenaConfig

/**
 * MinimapSystem — the arena overview in the HUD's top-right corner (#7, D58).
 *
 * Every frame it re-points a FIXED pool of blip widgets at the player, the
 * enemies, the boss and the loose loot, mapping world positions through
 * minimap_math. The pool is allocated once and never churned: creating and
 * destroying ~100 entities per frame would cost more than everything else the
 * minimap does put together, and would recycle entity ids fast enough to trip
 * the ghost-component traps this codebase has already been bitten by.
 *
 * The blips are UI *widgets* (UIElement + UIState + ScreenMembership on the
 * always-active `gameplay` screen), not world entities — see D58. That is the
 * same mechanism GameHUDSystem's hull/shield gauges use, and it is the only one
 * that actually draws: `RenderSystem::render` iterates
 * `entities_with_component<Position>()`, so a screen-space entity carrying only
 * ScreenPosition + Size + Color is never drawn by anything, and giving it a
 * Position hands it to CameraSystem, which overwrites ScreenPosition every
 * frame. Widgets also give z-ordering over the frame panel and survive
 * `spawn_world` (which deliberately skips UIElement entities), so the pool is
 * allocated exactly once per process.
 */
class MinimapSystem {
public:
    /// Style ids the blips select, and the frame widget's authored name. Code
    /// constants rather than JSON indices, the same discipline as item_ids (D26).
    static constexpr const char* SCREEN_NAME    = "gameplay";
    static constexpr const char* FRAME_WIDGET   = "minimap_frame";
    static constexpr const char* STYLE_PLAYER   = "minimap_player";
    static constexpr const char* STYLE_ENEMY    = "minimap_enemy";
    static constexpr const char* STYLE_PICKUP   = "minimap_pickup";

    void set_config(const MinimapConfig& minimap, const ArenaConfig& arena) {
        cfg_ = minimap;
        arena_ = arena;
    }

    void update(ComponentStorage& component_storage,
                EntityManager& entity_manager,
                Blackboard& blackboard);

    /// Blips currently in the pool — the cap, once the pool has been built.
    std::size_t pool_size() const { return pool_.size(); }
    /// Peak number of blips the world asked for, cap included. Test hook.
    int peak_requested() const { return peak_requested_; }

private:
    void ensure_pool(ComponentStorage& component_storage,
                     EntityManager& entity_manager);

    MinimapConfig cfg_{};
    ArenaConfig arena_{};
    std::vector<Entity> pool_;
    int peak_requested_ = 0;
    int logged_overflow_ = 0;   // highest overflow already reported
};

#endif  // MINIMAP_SYSTEM_HPP
