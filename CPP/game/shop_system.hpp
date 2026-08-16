#ifndef SHOP_SYSTEM_HPP
#define SHOP_SYSTEM_HPP

#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"
#include "arena_config.hpp"        // ShopConfig
#include "player_components.hpp"   // ShipState
#include <string>
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
 * Iteration 3 / Lane C (D61-D65) turned the presentation into a real menu: a
 * `shop` screen authored in GameData.json, one clickable card per catalogue row,
 * hover tooltips and a live drone preview, all driven from menu_tick(). The old
 * numbered Text list stays as the fallback for a data file that carries no shop
 * screen, and the 1-8 / TAB / B keyboard path is unchanged so headless scripts
 * keep working.
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

    // --- Lane C: the widget menu -------------------------------------------

    /// Screen authored in GameData.json -> screens. Pushed while the shop is open.
    static constexpr const char* SCREEN_NAME = "shop";

    /**
     * One frame of the clickable menu, fired from main.cpp's `shop-menu` hook.
     * Builds/tears the screen down to follow is_open(), routes confirmed clicks
     * off UISystem::UI_CLICK_KEY (and removes the key), and refreshes the cards,
     * the hover tooltip and the drone preview.
     *
     * Returns true when the player pressed LAUNCH — the caller then closes the
     * shop exactly as it does for the keyboard path. A data file with no `shop`
     * screen leaves this a permanent no-op returning false.
     */
    bool menu_tick(ComponentStorage& storage, EntityManager& entity_manager,
                   Blackboard& blackboard);

    // --- Gear levels (#11) --------------------------------------------------
    // Levels apply to the ONE fitted passive item, indexed into
    // ShipState.gear_levels by its row in cfg_->items. See D62.

    /// Price of the next level of item row `index` at `level`: base * growth^level.
    int gear_price(int index, int level) const;

    /// True when that item row is the one currently fitted (D62: only fitted gear levels).
    bool owns_gear(const ShipState& ship, int index) const;

    /// Buy one level. False (with a hud_message) when unowned, unaffordable, or
    /// the row has no scalable amount.
    bool upgrade_gear(int index, ComponentStorage& storage, Blackboard& blackboard,
                      ShipState& ship);

    /**
     * --dev / --god: buy every stacked upgrade up to its max_stacks.
     *
     * Routed through the real buy_upgrade path — prices are charged against the
     * dev balance and apply() does the stat writes — so there is no second
     * place that knows how an upgrade lands. Called once at run start.
     *
     * ponytail: rows with max_stacks == 0 (unlimited) are skipped; the shipped
     * catalogue has none. Give them a dev cap here if one ever appears.
     */
    void dev_max_upgrades(Entity player, ComponentStorage& storage,
                          Blackboard& blackboard, ShipState& ship);

    /// Last tooltip text written, for tests. Empty when nothing is hovered.
    const std::string& tooltip_name() const { return tip_name_text_; }
    const std::string& tooltip_detail() const { return tip_detail_text_; }

    /// D191: card `c`'s visible table text (name column + pip column), for
    /// tests — the card button itself is a caption-less hit target now.
    std::string card_line(const ComponentStorage& storage, int c) const;

private:
    void apply(const ShopUpgradeDef& def, Entity player,
               ComponentStorage& storage, Blackboard& blackboard);
    void buy_upgrade(int index, Entity player, ComponentStorage& storage,
                     Blackboard& blackboard, ShipState& ship);
    /// Equip an item (`is_item`) or a consumable. Flat price, replaces the slot.
    void equip(const ShopUpgradeDef& def, bool is_item, Entity player,
               ComponentStorage& storage, Blackboard& blackboard);
    void buy_gear(int index, Entity player, ComponentStorage& storage,
                  Blackboard& blackboard, ShipState& ship);
    void refresh_rows(ComponentStorage& storage, const ShipState& ship);
    /// Rows the widest page needs, so a page flip only rewrites text.
    size_t page_rows() const;

    // --- menu internals ---
    /// Resolve the named widgets, hide the legacy text rows, push the screen and
    /// spawn the preview. False when the data file carries no shop screen.
    bool menu_build(ComponentStorage& storage, EntityManager& entity_manager,
                    Blackboard& blackboard);
    void menu_teardown(ComponentStorage& storage, Blackboard& blackboard);
    /// Card slot -> catalogue index for the current page.
    void rebuild_visible(const ShipState& ship);
    void refresh_cards(ComponentStorage& storage, const ShipState& ship);
    /// The right-hand stat sheet (playtest #5). One row per shop upgrade, showing
    /// the live stat it moves; `card` is the hovered card (-1 none), whose row
    /// shows "now > after" in the gain colour instead of its pip meter.
    void refresh_stats(ComponentStorage& storage, const Blackboard& blackboard,
                       Entity player, const ShipState& ship, int card);
    /// Card slot under the pointer, or -1. Reads UIState.hovered (UISystem owns it).
    int  hovered_card(const ComponentStorage& storage) const;
    void refresh_tooltip(ComponentStorage& storage, const ShipState& ship, int card);
    void refresh_preview(ComponentStorage& storage, const Blackboard& blackboard,
                         const ShipState& ship, int card);
    void buy_visible_row(int card, Entity player, ComponentStorage& storage,
                         Blackboard& blackboard, ShipState& ship);

    const ShopConfig* cfg_ = nullptr;
    std::vector<Entity> rows_;   // [0] title, [1] credits, [2] slots, then page_rows(), then the footer
    bool open_ = false;
    int page_ = 0;               // 0 = upgrades, 1 = gear (items + consumables), 2 = gear levels

    static constexpr int MENU_CARDS = 8;
    bool   menu_built_ = false;
    bool   menu_pushed_ = false;
    bool   menu_absent_ = false;         // no `shop` screen in the data file
    Entity card_[MENU_CARDS] = {};
    UIRect card_rect_[MENU_CARDS] = {};  // authored rects, restored when a card is used
    Entity title_ = 0, credits_ = 0, tab_[3] = {}, leave_ = 0;
    Entity tip_panel_ = 0, tip_name_ = 0, tip_desc_ = 0;
    // D189: press-and-hold purchase. `hold_bar_` is a pooled fill panel
    // stretched along the held card's bottom edge (HUD gauge idiom).
    float  hold_t_ = 0.0f;
    int    hold_card_ = -1;
    Entity hold_bar_ = 0;
    // D191: the two-column card table — pooled labels layered over the (now
    // caption-less) card buttons. [0] name at a fixed left edge, so first letters
    // align; [1] the price column.
    Entity col_name_[MENU_CARDS] = {};
    Entity col_pips_[MENU_CARDS] = {};
    // Playtest #5: the stat sheet in the right column, one name/value pair per
    // shop upgrade row. The value column is what previews a hovered purchase —
    // the per-row pip previews that used to sit on the cards moved here.
    static constexpr int STAT_ROWS = 8;
    Entity stat_name_[STAT_ROWS] = {};
    Entity stat_val_[STAT_ROWS] = {};
    Entity preview_glow_ = 0, preview_ship_ = 0;
    // D190: the 7 kit overlays on the preview drone — worn parts always show,
    // and hovering an upgrade row lights up the part that row would buy.
    Entity preview_kit_[7] = {};
    std::vector<int> visible_;           // card slot -> catalogue index
    std::string tip_name_text_, tip_detail_text_;
    std::string page_hint_;   // idle text for the detail pane
};

#endif // SHOP_SYSTEM_HPP
