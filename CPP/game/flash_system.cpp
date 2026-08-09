#include "flash_system.hpp"
#include "player_components.hpp"  // Flash
#include "enemy_components.hpp"   // EnemyTag
#include "feedback.hpp"           // flash_tint

/**
 * The entity's resting tint — what a finished flash must fade back to.
 *
 * v2 Phase 5a: enemies carry a persistent per-arena Tint, and Color is the
 * component that records what it should be (the spawner already added a Color as
 * the no-sprite fallback, and RenderSystem ignores Color whenever a SpriteSheet
 * is present, so it is free to reuse as the base colour). Everything else —
 * notably the player, whose Color is a green fallback rect it never renders —
 * keeps the old identity behaviour, so a flash there still fades to white and
 * the Tint is removed on expiry.
 */
static Tint base_tint(ComponentStorage& storage, Entity e) {
    if (!storage.has_component<EnemyTag>(e)) return Tint{255, 255, 255, 255, false};
    auto c = storage.get_component<Color>(e);
    if (!c.has_value()) return Tint{255, 255, 255, 255, false};
    return Tint{c->get().r, c->get().g, c->get().b, 255, false};
}

void FlashSystem::update(ComponentStorage& storage, const Blackboard& blackboard) {
    float dt = static_cast<float>(blackboard.get_or<double>("delta_time", 0.0));

    for (Entity e : storage.entities_with_component<Flash>()) {
        auto f = storage.get_component<Flash>(e);
        if (!f.has_value()) continue;
        Flash& flash = f->get();

        const Tint base = base_tint(storage, e);

        flash.time_left -= dt;
        if (flash.time_left <= 0.0f) {
            storage.remove_component<Flash>(e);
            // Restore the entity's normal look: its base tint if it owns one,
            // otherwise no Tint at all (identity), exactly as before Phase 5a.
            if (storage.has_component<EnemyTag>(e) && storage.has_component<Color>(e))
                storage.add_component<Tint>(e, base);
            else
                storage.remove_component<Tint>(e);
            continue;
        }
        storage.add_component<Tint>(e, feedback::flash_tint(flash, base));
    }
}
