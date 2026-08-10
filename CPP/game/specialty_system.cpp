#include "specialty_system.hpp"

#include "enemy_components.hpp"
#include "enemy_fire_system.hpp"   // type_for, player_centre
#include "hazard_patch.hpp"
#include <algorithm>

void SpecialtySystem::update(ComponentStorage& storage, EntityManager& entity_manager,
                             Blackboard& blackboard) {
    if (!blackboard.has("delta_time")) return;
    const float dt = static_cast<float>(blackboard.get<double>("delta_time"));

    float px = 0.0f, py = 0.0f;
    const bool have_player = enemy_fire::player_centre(storage, px, py);

    for (Entity e : storage.entities_with_component<EnemyBehavior>()) {
        if (storage.has_component<EnemyShot>(e)) continue;      // a shot, not a unit
        if (storage.has_component<DestroyRequest>(e)) continue;
        auto beh_opt = storage.get_component<EnemyBehavior>(e);
        auto pos = storage.get_component<Position>(e);
        auto sz = storage.get_component<Size>(e);
        if (!beh_opt.has_value() || !pos.has_value() || !sz.has_value()) continue;
        EnemyBehavior& beh = beh_opt->get();
        const float cx = pos->get().x + sz->get().width * 0.5f;
        const float cy = pos->get().y + sz->get().height * 0.5f;

        switch (beh.kind) {
        case behavior_kinds::SPITTER: {
            if (beh.timer > 0.0f) { beh.timer -= dt; break; }
            beh.timer = beh.cooldown;
            hazard::PatchSpec spec;
            spec.size = specialty::PATCH_SIZE;
            spec.lifetime = specialty::PATCH_LIFETIME;
            const EnemyType* t = enemy_fire::type_for(cfg_, behavior_kinds::SPITTER, beh.tier);
            spec.damage = t != nullptr ? t->shot_damage : 9.0f;
            if (beh.tier >= 2) { spec.lifetime *= 1.6f; spec.size *= 1.2f; }
            hazard::spawn_patch(storage, entity_manager, cx, cy, spec);
            break;
        }

        case behavior_kinds::MINER: {
            // tier 0 is a *deployed mine*, not a dropper. It arms, then detonates
            // on proximity — no collider until it does, so it cannot be shot or
            // walked through by accident.
            if (beh.tier == 0) {
                if (beh.timer > 0.0f) { beh.timer -= dt; break; }
                if (!have_player) break;
                const float dx = px - cx, dy = py - cy;
                if (dx * dx + dy * dy > beh.aim * beh.aim) break;
                hazard::PatchSpec blast;
                blast.size = specialty::MINE_BLAST_SIZE;
                blast.lifetime = specialty::MINE_BLAST_TIME;
                blast.damage = beh.cooldown;   // the dropper stored blast damage here
                blast.r = 255; blast.g = 170; blast.b = 60;
                blast.emission_rate = 260.0f;  // one-shot: ~90 live particles for 0.35s
                hazard::spawn_patch(storage, entity_manager, cx, cy, blast);
                storage.add_component<DestroyRequest>(e, DestroyRequest{});
                break;
            }
            if (beh.timer > 0.0f) { beh.timer -= dt; break; }
            beh.timer = beh.cooldown;
            const EnemyType* t = enemy_fire::type_for(cfg_, behavior_kinds::MINER, beh.tier);
            const float blast_damage = t != nullptr ? t->shot_damage : 22.0f;
            const float trigger = specialty::MINE_TRIGGER * (beh.tier >= 2 ? 1.35f : 1.0f);

            Entity mine = entity_manager.create_entity();
            const float half = specialty::MINE_SIZE * 0.5f;
            storage.add_component<Position>(mine, Position{cx - half, cy - half});
            storage.add_component<Size>(mine, Size{specialty::MINE_SIZE, specialty::MINE_SIZE});
            // D96: an actual bomb — round body, fuse, spark — instead of the
            // orange square. Images beats Color in the render chain; Color stays
            // as the fallback if the texture fails to load.
            storage.add_component<Color>(mine, Color{255, 140, 50, 230});
            storage.add_component<Images>(mine, Images{{"v2/hazard_mine.png"}, 0});
            storage.add_component<Lifetime>(mine, Lifetime{specialty::MINE_LIFETIME});
            storage.add_component<RenderLayer>(mine, RenderLayer{3});
            // tier 0 = the mine. timer = arm delay, cooldown = its blast damage,
            // aim = its trigger radius. Three numbers on the component that is
            // already registered, rather than a fifth component type.
            storage.add_component<EnemyBehavior>(mine,
                EnemyBehavior{behavior_kinds::MINER, 0, specialty::MINE_ARM_DELAY,
                              blast_damage, trigger});
            ParticleEmitter blink;
            blink.shape = EmitterShape::Point;
            blink.additive = true;
            blink.emission_rate = 8.0f;      // ~5 live particles per mine
            blink.particle_lifetime = 0.6f;
            blink.min_speed = 0.0f; blink.max_speed = 10.0f;
            blink.cone_half_angle = 180.0f;
            blink.start_size = 6.0f; blink.end_size = 0.0f;
            blink.start_r = 255; blink.start_g = 120; blink.start_b = 40; blink.start_a = 230;
            blink.end_r = 120; blink.end_g = 20; blink.end_b = 10; blink.end_a = 0;
            blink.offset_x = half; blink.offset_y = half;
            storage.add_component<ParticleEmitter>(mine, blink);
            break;
        }

        case behavior_kinds::BULWARK: {
            // Slow facing + frontal armour. The counterplay is flanking, so the
            // turn rate is the actual difficulty knob, not the armour value.
            if (!have_player) break;
            const float want = std::atan2(py - cy, px - cx);
            const float turn = specialty::BULWARK_TURN / (beh.tier >= 2 ? 0.7f : 1.0f);
            beh.aim = enemy_fire::turn_toward(beh.aim, want, turn * dt);
            auto h = storage.get_component<Health>(e);
            if (!h.has_value()) break;
            const float armor = beh.tier >= 2 ? specialty::BULWARK_ARMOR * 0.7f
                                              : specialty::BULWARK_ARMOR;
            h->get().armor_multiplier =
                specialty::inside_arc(beh.aim, want, specialty::BULWARK_ARC) ? armor : 1.0f;
            break;
        }

        default:
            break;   // SEEKER / SHOOTER / SPLITTER / BOSS are owned elsewhere
        }
    }
}
