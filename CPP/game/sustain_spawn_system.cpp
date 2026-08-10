#include "sustain_spawn_system.hpp"

#include <algorithm>
#include <cmath>

#include "player_components.hpp"   // PlayerTag, ShipState, Pickup, PickupKind

namespace {

// The golden angle. Successive multiples of it are the classic low-discrepancy
// spread over a disc (the sunflower packing) — it never lines placements up into
// spokes the way a rational fraction of a turn does.
constexpr float GOLDEN_ANGLE = 2.39996322972865332f;
// Conjugate of the golden ratio, used the same way for the radial coordinate.
constexpr float GOLDEN_FRAC = 0.61803398874989484f;
// Keep placements off the wall so a pickup is never half-buried in the boundary.
constexpr float RIM_MARGIN = 0.88f;
// Attempts to dodge the player before giving up and placing anyway. A run of
// eight golden-angle steps sweeps most of the arena, so failing all eight means
// min_player_dist is simply too large for the arena and a placement is better
// than none.
constexpr int MAX_TRIES = 8;

int live_sustain_pickups(const ComponentStorage& cs) {
    int n = 0;
    for (Entity e : cs.entities_with_component<Pickup>()) {
        if (cs.has_component<DestroyRequest>(e)) continue;
        auto pk = cs.get_component<Pickup>(e);
        if (!pk.has_value()) continue;
        const int k = pk->get().kind;
        if (k == static_cast<int>(PickupKind::Health) ||
            k == static_cast<int>(PickupKind::Shield)) {
            ++n;
        }
    }
    return n;
}

}  // namespace

void sustain_placement_point(int n, const ArenaConfig& arena, float& out_x, float& out_y) {
    const float angle = static_cast<float>(n) * GOLDEN_ANGLE;
    // sqrt of a low-discrepancy fraction spreads points EVENLY over the disc's
    // area; using the fraction directly would crowd them into the centre.
    float u = std::fmod(static_cast<float>(n) * GOLDEN_FRAC + 0.5f, 1.0f);
    if (u < 0.0f) u += 1.0f;
    const float r = arena.radius * RIM_MARGIN * std::sqrt(u);
    out_x = arena.center_x + std::cos(angle) * r;
    out_y = arena.center_y + std::sin(angle) * r;
}

bool sustain_is_shield(int n, float shield_weight) {
    if (!(shield_weight > 0.0f)) return false;
    if (shield_weight >= 1.0f) return true;
    const float a = static_cast<float>(n) * shield_weight;
    const float b = static_cast<float>(n + 1) * shield_weight;
    return std::floor(b) > std::floor(a);
}

void sustain_spawn(ComponentStorage& component_storage,
                   EntityManager& entity_manager,
                   Blackboard& blackboard,
                   const SustainConfig& cfg,
                   const ArenaConfig& arena,
                   const EconomyConfig& economy) {
    if (!(cfg.interval > 0.0f) || cfg.max_live <= 0) return;   // feature off

    // Restart probe: the wave counter only ever decreases when spawn_world has
    // rebuilt the world, so this is the cheapest honest "new run" signal there is
    // and it needs no edit to spawn_world (which this lane does not own).
    const int wave = blackboard.get_or<int>("wave", 0);
    if (wave < blackboard.get_or<int>(sustain_keys::WAVE, 0)) {
        blackboard.set<int>(sustain_keys::COUNT, 0);
        blackboard.set<float>(sustain_keys::TIMER, cfg.interval);
    }
    blackboard.set<int>(sustain_keys::WAVE, wave);

    const float dt = static_cast<float>(blackboard.get_or<double>("delta_time", 0.0));
    float timer = blackboard.get_or<float>(sustain_keys::TIMER, cfg.interval) - dt;
    // TIMER_EPS: a countdown of N equal float steps lands a hair either side of
    // zero, so a bare `> 0` test slips the placement to the next frame about half
    // the time. The epsilon is 0.006 of a frame — far too small to see, far too
    // large for the residue to hide in — and makes the cadence an exact frame
    // count, which is what the determinism test can actually pin down.
    constexpr float TIMER_EPS = 1e-4f;
    if (timer > TIMER_EPS) { blackboard.set<float>(sustain_keys::TIMER, timer); return; }

    // The cadence is a fixed grid: the timer resets whether or not a pickup is
    // actually placed, so a capped arena does not bank up a burst of placements
    // the moment the player collects one.
    blackboard.set<float>(sustain_keys::TIMER, cfg.interval);

    if (live_sustain_pickups(component_storage) >= cfg.max_live) return;

    // The player, for the keep-away test and for the "shields are useless without
    // a capacitor" substitution below.
    float px = arena.center_x, py = arena.center_y;
    bool have_player = false, has_shield_bank = false;
    for (Entity p : component_storage.entities_with_component<PlayerTag>()) {
        auto pos = component_storage.get_component<Position>(p);
        auto sz = component_storage.get_component<Size>(p);
        if (!pos.has_value()) continue;
        px = pos->get().x + (sz.has_value() ? sz->get().width * 0.5f : 0.0f);
        py = pos->get().y + (sz.has_value() ? sz->get().height * 0.5f : 0.0f);
        have_player = true;
        if (auto s = component_storage.get_component<ShipState>(p); s.has_value()) {
            has_shield_bank = s->get().shield_max > 0.0f;
        }
        break;
    }
    if (!have_player) return;   // nothing to place a pickup away from

    int n = blackboard.get_or<int>(sustain_keys::COUNT, 0);
    float x = 0.0f, y = 0.0f;
    for (int i = 0; i < MAX_TRIES; ++i) {
        sustain_placement_point(n, arena, x, y);
        const float dx = x - px, dy = y - py;
        if (std::sqrt(dx * dx + dy * dy) >= cfg.min_player_dist) break;
        ++n;   // step the spiral, not a random retry — still fully deterministic
    }

    // A shield cell is worthless without a capacitor to put it in, so before the
    // player has bought one every placement is hull. Deterministic: it depends on
    // sim state, never on a draw.
    const bool shield = has_shield_bank && sustain_is_shield(n, cfg.shield_weight);
    const float amount = shield ? cfg.shield_amount : cfg.health_amount;

    const float sz = economy.pickup_size;
    Entity e = entity_manager.create_entity();
    component_storage.add_component<Position>(e, Position{x - sz * 0.5f, y - sz * 0.5f});
    component_storage.add_component<Size>(e, Size{sz, sz});
    // D94: generated sprites — a medical cross for hull, a barrier crest for
    // shield. Color stays underneath as the render chain's load fallback.
    component_storage.add_component<Color>(e, shield ? Color{120, 200, 255, 255}
                                                     : Color{110, 235, 130, 255});
    component_storage.add_component<Images>(e, Images{
        {shield ? "v2/pickup_shield.png" : "v2/pickup_health.png"}, 0});
    component_storage.add_component<Pickup>(e, Pickup{
        static_cast<int>(shield ? PickupKind::Shield : PickupKind::Health),
        static_cast<int>(amount + 0.5f),
        economy.pickup_magnet_speed});
    component_storage.add_component<RenderLayer>(e, RenderLayer{4});
    // Deliberately NO Lifetime: unlike coins (D52, which despawn to make grabbing
    // them a risk/reward call), a sustain pickup is the arena's standing offer of
    // a heal. max_live is what bounds them.

    blackboard.set<int>(sustain_keys::COUNT, n + 1);
}
