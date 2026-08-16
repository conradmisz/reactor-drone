#include "bullet_pattern.hpp"

#include <algorithm>
#include <cmath>

#include "enemy_components.hpp"
#include "enemy_fire_system.hpp"   // enemy_fire::spawn_shot, player_centre

namespace bullet_pattern {

namespace {

constexpr float TAU = 6.28318530717958647692f;
constexpr float DEG2RAD = TAU / 360.0f;

}  // namespace

int pattern_index(const std::vector<BulletPatternDef>& patterns,
                  const std::string& name) {
    if (name.empty()) return -1;
    for (size_t i = 0; i < patterns.size(); ++i)
        if (patterns[i].name == name) return static_cast<int>(i);
    return -1;
}

void op_angles(const BulletPatternOp& op, float phase, float aim_angle,
               std::vector<float>& out) {
    out.clear();
    const OpKind kind = op_kind_for(op.type);
    if (kind == OpKind::Wait || kind == OpKind::Unknown) return;

    const int n = std::min(std::max(op.count, 0), MAX_SHOTS_PER_OP);
    if (n == 0) return;
    out.reserve(static_cast<size_t>(n));

    switch (kind) {
        case OpKind::Ring: {
            // Even spacing over the full circle, rotated by the running phase.
            // The phase is what turns a stack of rings into a rotating flower.
            for (int i = 0; i < n; ++i)
                out.push_back(phase + TAU * static_cast<float>(i) / static_cast<float>(n));
            break;
        }
        case OpKind::Fan:
        case OpKind::Aimed: {
            // A fan centred on the emitter's phase; an aimed fan is the same
            // shape centred on the player instead — one code path, because the
            // only difference IS where the centre comes from.
            const float centre = kind == OpKind::Aimed ? aim_angle : phase;
            const float spread = op.spread_deg * DEG2RAD;
            if (n == 1) { out.push_back(centre); break; }
            const float step = spread / static_cast<float>(n - 1);
            for (int i = 0; i < n; ++i)
                out.push_back(centre - spread * 0.5f + step * static_cast<float>(i));
            break;
        }
        case OpKind::Spiral: {
            // Arms, not a ring: `count` shots evenly spaced, all riding the phase,
            // which the caller advances by angular_vel every step. With count 1
            // this is the classic single-arm spiral.
            for (int i = 0; i < n; ++i)
                out.push_back(phase + TAU * static_cast<float>(i) / static_cast<float>(n));
            break;
        }
        default: break;
    }
}

void tick(ComponentStorage& storage, EntityManager& entity_manager,
          Blackboard& blackboard, const GameConfig& cfg, float dt) {
    (void)blackboard;
    if (cfg.patterns.empty() || dt <= 0.0f) return;

    float px = 0.0f, py = 0.0f;
    const bool have_player = enemy_fire::player_centre(storage, px, py);

    // Scratch, reused across every emitter this frame: the op angle list is the
    // only allocation the interpreter would otherwise make, and it is bounded by
    // MAX_SHOTS_PER_OP.
    static std::vector<float> angles;

    for (Entity e : storage.entities_with_component<EnemyBehavior>()) {
        if (storage.has_component<EnemyShot>(e)) continue;   // shots carry one too
        auto beh_opt = storage.get_component<EnemyBehavior>(e);
        if (!beh_opt.has_value()) continue;
        EnemyBehavior& beh = beh_opt->get();
        if (beh.pattern < 0 || beh.pattern >= static_cast<int>(cfg.patterns.size()))
            continue;

        const BulletPatternDef& pat = cfg.patterns[static_cast<size_t>(beh.pattern)];
        if (pat.ops.empty()) continue;

        auto pos = storage.get_component<Position>(e);
        auto sz = storage.get_component<Size>(e);
        if (!pos.has_value() || !sz.has_value()) continue;
        const float cx = pos->get().x + sz->get().width * 0.5f;
        const float cy = pos->get().y + sz->get().height * 0.5f;
        const float aim = have_player ? std::atan2(py - cy, px - cx) : beh.phase;

        if (beh.cursor < 0 || beh.cursor >= static_cast<int>(pat.ops.size()))
            beh.cursor = 0;
        const BulletPatternOp& op = pat.ops[static_cast<size_t>(beh.cursor)];

        // The spiral phase advances every frame, whether or not this frame fires:
        // that is what makes the arm sweep smoothly instead of stepping.
        beh.phase += op.angular_vel_deg * DEG2RAD * dt;
        if (beh.phase > TAU) beh.phase -= TAU;
        if (beh.phase < -TAU) beh.phase += TAU;

        beh.timer -= dt;
        if (beh.timer > 0.0f) continue;

        const OpKind kind = op_kind_for(op.type);
        if (kind == OpKind::Wait) {
            beh.timer = std::max(0.0f, op.wait);
        } else {
            op_angles(op, beh.phase, aim, angles);
            const float speed = op.speed > 0.0f ? op.speed : 220.0f;
            for (float a : angles) {
                // Through the EXISTING spawn path: a pattern shot is an enemy shot
                // in every respect (layer, ContactDamage, lifetime, trail), so it
                // needs no damage system and no new component of its own.
                enemy_fire::spawn_shot(storage, entity_manager, cx, cy, a, speed,
                                       /*damage=*/8.0f, /*tier=*/1);
            }
            // `interval` is the gap to the NEXT step of this op; a burst op
            // (interval 0) falls straight through to the next op instead.
            beh.timer = std::max(0.0f, op.interval);
        }

        if (op.interval <= 0.0f || kind == OpKind::Wait) {
            ++beh.cursor;
            if (beh.cursor >= static_cast<int>(pat.ops.size())) {
                beh.cursor = 0;
                // A non-looping pattern RETIRES its emitter rather than parking on
                // an out-of-range cursor: the range guard at the top of the loop
                // would otherwise wrap it back to op 0 and loop it forever.
                if (!pat.loop) beh.pattern = -1;
            }
        }
    }
}

}  // namespace bullet_pattern
