#include "flash_system.hpp"
#include "player_components.hpp"  // Flash
#include "feedback.hpp"           // flash_tint

void FlashSystem::update(ComponentStorage& storage, const Blackboard& blackboard) {
    float dt = static_cast<float>(blackboard.get_or<double>("delta_time", 0.0));

    for (Entity e : storage.entities_with_component<Flash>()) {
        auto f = storage.get_component<Flash>(e);
        if (!f.has_value()) continue;
        Flash& flash = f->get();

        flash.time_left -= dt;
        if (flash.time_left <= 0.0f) {
            storage.remove_component<Flash>(e);
            storage.remove_component<Tint>(e);  // back to the entity's normal look
            continue;
        }
        storage.add_component<Tint>(e, feedback::flash_tint(flash));
    }
}
