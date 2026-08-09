#ifndef SHOP_SYSTEM_HPP
#define SHOP_SYSTEM_HPP

#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"
#include "arena_config.hpp"        // ShopConfig
#include "player_components.hpp"   // ShipState
#include <vector>

/**
 * ShopSystem — the between-waves shop (Gameplay Phase 3, D2/D11).
 *
 * The shop is a game phase, not an overlay: main.cpp switches to PHASE_SHOP and
 * calls update() instead of the gameplay block, so the arena is frozen while it
 * is open. Entry is either a cleared wave whose number is a multiple of 5, or the
 * player spending a key mid-run; both are decided in main.cpp, which owns the
 * phase variable. Everything else — catalogue, prices, rendering, purchases —
 * lives here (R7: main.cpp gets calls, not logic).
 *
 * The UI is a numbered keyboard list rendered as Text + ScreenPosition entities,
 * the same pattern GameHUDSystem uses; they are created on open() and destroyed
 * on close(). Clickable cards are Phase 6 (D11).
 *
 * Purchases write straight into the player's existing components — Health,
 * WeaponStats, ShipState — plus one derived blackboard key for the barrel count
 * (see "ship.extra_shots" below). No new component type (D17).
 */
class ShopSystem {
public:
    void set_config(const ShopConfig* cfg) { cfg_ = cfg; }

    /// Spawn the shop's Text rows. Safe to call when already open (no-op).
    void open(ComponentStorage& storage, EntityManager& entity_manager,
              const Blackboard& blackboard);

    /// Mark the shop's Text rows for destruction. Caller sweeps.
    void close(ComponentStorage& storage);

    bool is_open() const { return open_; }

    /**
     * One shop frame. `digit` is 1-8 for a purchase row (0 = nothing pressed),
     * `leave` is the close key, `toggle_page` flips between the upgrade page and
     * the Phase 4 gear page (items + consumables). Returns true when the player
     * is done, at which point main.cpp switches back to PHASE_PLAYING.
     */
    bool update(ComponentStorage& storage, Blackboard& blackboard,
                int digit, bool leave, bool toggle_page = false);

    /// Price of the next purchase of catalogue row `index` for a given count.
    int price_for(int index, int already_bought) const;

private:
    void apply(const ShopUpgradeDef& def, Entity player,
               ComponentStorage& storage, Blackboard& blackboard);
    /// Equip an item (`is_item`) or a consumable. Flat price, replaces the slot.
    void equip(const ShopUpgradeDef& def, bool is_item, Entity player,
               ComponentStorage& storage, Blackboard& blackboard);
    void buy_gear(int index, Entity player, ComponentStorage& storage,
                  Blackboard& blackboard, ShipState& ship);
    void refresh_rows(ComponentStorage& storage, const ShipState& ship);
    /// Rows the widest page needs, so a page flip only rewrites text.
    size_t page_rows() const;

    const ShopConfig* cfg_ = nullptr;
    std::vector<Entity> rows_;   // [0] title, [1] credits, [2] slots, then page_rows(), then the footer
    bool open_ = false;
    int page_ = 0;               // 0 = upgrades, 1 = gear (items + consumables)
};

#endif // SHOP_SYSTEM_HPP
