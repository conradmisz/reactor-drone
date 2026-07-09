/**
 * Unit tests for DamageApplySystem — centralized damage processing.
 *
 * Validates: Requirements 13.1, 13.2, 13.3, 13.4, 13.5, 13.6, 13.7
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "game/tower_components.hpp"
#include "game/enemy_components.hpp"
#include "game/damage_apply_system.hpp"

// ===========================================================================
// Test 1: Damage applied — Health.current reduced by amount * armor_multiplier
// Validates: Requirements 13.1, 13.2, 13.3, 13.4
// ===========================================================================

TEST_CASE("DamageApplySystem: Health.current reduced by amount * armor_multiplier", "[Game][damage_apply][unit]") {
    ComponentStorage cs;
    EntityManager em;

    // Create target with 100 HP and armor_multiplier = 0.8
    Entity target = em.create_entity();
    cs.add_component<Health>(target, Health{100.0f, 100.0f, 0.8f});

    // Create DamageEvent entity
    Entity event_entity = em.create_entity();
    cs.add_component<DamageEvent>(event_entity, DamageEvent{target, 50.0f});

    DamageApplySystem system;
    system.update(em, cs);

    auto health_opt = cs.get_component<Health>(target);
    REQUIRE(health_opt.has_value());
    // effective = 50.0 * 0.8 = 40.0; health = 100 - 40 = 60
    REQUIRE_THAT(health_opt->get().current, Catch::Matchers::WithinAbs(60.0f, 0.001f));
}

// ===========================================================================
// Test 2: HP floor — Health.current never goes below 0.0f
// Validates: Requirement 13.5
// ===========================================================================

TEST_CASE("DamageApplySystem: Health.current never goes below 0.0f", "[Game][damage_apply][unit]") {
    ComponentStorage cs;
    EntityManager em;

    // Create target with 10 HP, no armor
    Entity target = em.create_entity();
    cs.add_component<Health>(target, Health{10.0f, 100.0f, 1.0f});

    // Create DamageEvent with damage far exceeding current HP
    Entity event_entity = em.create_entity();
    cs.add_component<DamageEvent>(event_entity, DamageEvent{target, 999.0f});

    DamageApplySystem system;
    system.update(em, cs);

    auto health_opt = cs.get_component<Health>(target);
    REQUIRE(health_opt.has_value());
    REQUIRE(health_opt->get().current >= 0.0f);
    REQUIRE_THAT(health_opt->get().current, Catch::Matchers::WithinAbs(0.0f, 0.001f));
}

// ===========================================================================
// Test 3: Armor reduction — with armor_multiplier=0.5, effective damage is half
// Validates: Requirements 13.3, 13.4
// ===========================================================================

TEST_CASE("DamageApplySystem: armor_multiplier=0.5 means half damage", "[Game][damage_apply][unit]") {
    ComponentStorage cs;
    EntityManager em;

    // Create target with 100 HP and armor_multiplier = 0.5
    Entity target = em.create_entity();
    cs.add_component<Health>(target, Health{100.0f, 100.0f, 0.5f});

    // Create DamageEvent with 60 damage
    Entity event_entity = em.create_entity();
    cs.add_component<DamageEvent>(event_entity, DamageEvent{target, 60.0f});

    DamageApplySystem system;
    system.update(em, cs);

    auto health_opt = cs.get_component<Health>(target);
    REQUIRE(health_opt.has_value());
    // effective = 60.0 * 0.5 = 30.0; health = 100 - 30 = 70
    REQUIRE_THAT(health_opt->get().current, Catch::Matchers::WithinAbs(70.0f, 0.001f));
}

// ===========================================================================
// Test 4: No armor (multiplier=1.0) — full damage applied
// Validates: Requirement 13.4
// ===========================================================================

TEST_CASE("DamageApplySystem: armor_multiplier=1.0 means full damage", "[Game][damage_apply][unit]") {
    ComponentStorage cs;
    EntityManager em;

    // Create target with 100 HP and no armor (multiplier = 1.0)
    Entity target = em.create_entity();
    cs.add_component<Health>(target, Health{100.0f, 100.0f, 1.0f});

    // Create DamageEvent with 25 damage
    Entity event_entity = em.create_entity();
    cs.add_component<DamageEvent>(event_entity, DamageEvent{target, 25.0f});

    DamageApplySystem system;
    system.update(em, cs);

    auto health_opt = cs.get_component<Health>(target);
    REQUIRE(health_opt.has_value());
    // effective = 25.0 * 1.0 = 25.0; health = 100 - 25 = 75
    REQUIRE_THAT(health_opt->get().current, Catch::Matchers::WithinAbs(75.0f, 0.001f));
}

// ===========================================================================
// Test 5: Event entity gets DestroyRequest after processing
// Validates: Requirement 13.6
// ===========================================================================

TEST_CASE("DamageApplySystem: event entity gets DestroyRequest after processing", "[Game][damage_apply][unit]") {
    ComponentStorage cs;
    EntityManager em;

    // Create target with health
    Entity target = em.create_entity();
    cs.add_component<Health>(target, Health{100.0f, 100.0f, 1.0f});

    // Create DamageEvent entity
    Entity event_entity = em.create_entity();
    cs.add_component<DamageEvent>(event_entity, DamageEvent{target, 10.0f});

    DamageApplySystem system;
    system.update(em, cs);

    // Event entity should have DestroyRequest
    REQUIRE(cs.has_component<DestroyRequest>(event_entity));
}

// ===========================================================================
// Test 6: Dead target — DestroyRequest on event entity, no damage applied
// Validates: Requirement 13.7
// ===========================================================================

TEST_CASE("DamageApplySystem: dead target gets DestroyRequest on event, no damage", "[Game][damage_apply][unit]") {
    ComponentStorage cs;
    EntityManager em;

    // Create event entity first so it doesn't recycle the target's ID
    Entity event_entity = em.create_entity();

    // Create and destroy target
    Entity target = em.create_entity();
    cs.add_component<Health>(target, Health{100.0f, 100.0f, 1.0f});
    em.destroy_entity(target);

    // Add DamageEvent pointing to dead target
    cs.add_component<DamageEvent>(event_entity, DamageEvent{target, 50.0f});

    DamageApplySystem system;
    system.update(em, cs);

    // Event entity should still get DestroyRequest
    REQUIRE(cs.has_component<DestroyRequest>(event_entity));
}

// ===========================================================================
// Test 7: Target lacks Health — DestroyRequest on event entity, no crash
// Validates: Requirement 13.7
// ===========================================================================

TEST_CASE("DamageApplySystem: target lacks Health gets DestroyRequest on event, no crash", "[Game][damage_apply][unit]") {
    ComponentStorage cs;
    EntityManager em;

    // Create target WITHOUT Health component
    Entity target = em.create_entity();
    cs.add_component<Position>(target, Position{50.0f, 50.0f});
    // Deliberately NOT adding Health

    // Create DamageEvent entity
    Entity event_entity = em.create_entity();
    cs.add_component<DamageEvent>(event_entity, DamageEvent{target, 30.0f});

    DamageApplySystem system;
    REQUIRE_NOTHROW(system.update(em, cs));

    // Event entity should still get DestroyRequest
    REQUIRE(cs.has_component<DestroyRequest>(event_entity));
}
