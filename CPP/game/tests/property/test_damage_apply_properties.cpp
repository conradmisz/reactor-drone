/**
 * Property-based tests for DamageApplySystem.
 *
 * Property 11: DamageEvent Entity Cleanup
 *
 * For any DamageEvent entity processed by DamageApplySystem, the event entity
 * SHALL receive a DestroyRequest after processing, regardless of whether the
 * target exists or has a Health component.
 *
 * **Validates: Requirements 13.6, 13.7**
 *
 * Feature: 090-09-cannon-laser
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>
#include <cmath>

#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/components.hpp"
#include "game/tower_components.hpp"
#include "game/enemy_components.hpp"
#include "game/damage_apply_system.hpp"

// Configurable test iteration counts (MANDATORY — workspace policy)
constexpr int NUM_OUTER_TESTS = 10;
constexpr int NUM_INNER_TESTS = 5;

// ===========================================================================
// Feature: 090-09-cannon-laser, Property 11: DamageEvent Entity Cleanup
//
// For any DamageEvent entity processed by DamageApplySystem, the event entity
// SHALL receive a DestroyRequest after processing, regardless of whether the
// target exists or has a Health component.
//
// **Validates: Requirements 13.6, 13.7**
// ===========================================================================

TEST_CASE("DamageApplySystem: DamageEvent entity receives DestroyRequest with valid target",
          "[Game][cannon-laser][property]") {
    SECTION("Property 11: DestroyRequest on event entity when target has Health") {
        // Generate random damage amounts
        auto damage_int = GENERATE(take(NUM_OUTER_TESTS, random(1, 200)));
        auto health_int = GENERATE(take(NUM_INNER_TESTS, random(10, 500)));

        float damage = static_cast<float>(damage_int);
        float health_val = static_cast<float>(health_int);

        EntityManager em;
        ComponentStorage cs;

        // Create a valid target with Health
        Entity target = em.create_entity();
        cs.add_component<EnemyTag>(target, EnemyTag{});
        cs.add_component<Health>(target, Health{health_val, health_val, 1.0f});

        // Create a DamageEvent entity targeting the valid target
        Entity event_entity = em.create_entity();
        cs.add_component<DamageEvent>(event_entity, DamageEvent{target, damage});

        // Verify no DestroyRequest before system runs
        REQUIRE_FALSE(cs.has_component<DestroyRequest>(event_entity));

        // Run DamageApplySystem
        DamageApplySystem system;
        system.update(em, cs);

        // Property 11: event entity SHALL receive DestroyRequest after processing
        REQUIRE(cs.has_component<DestroyRequest>(event_entity));
    }
}

TEST_CASE("DamageApplySystem: DamageEvent entity receives DestroyRequest with invalid target",
          "[Game][cannon-laser][property]") {
    SECTION("Property 11: DestroyRequest on event entity when target does not exist") {
        // Generate random damage amounts
        auto damage_int = GENERATE(take(NUM_OUTER_TESTS, random(1, 200)));

        float damage = static_cast<float>(damage_int);

        EntityManager em;
        ComponentStorage cs;

        // Create a target and then destroy it to simulate a dead target
        Entity target = em.create_entity();
        em.destroy_entity(target);

        // Create a DamageEvent entity targeting the dead entity
        Entity event_entity = em.create_entity();
        cs.add_component<DamageEvent>(event_entity, DamageEvent{target, damage});

        // Verify no DestroyRequest before system runs
        REQUIRE_FALSE(cs.has_component<DestroyRequest>(event_entity));

        // Run DamageApplySystem
        DamageApplySystem system;
        system.update(em, cs);

        // Property 11: event entity SHALL receive DestroyRequest regardless of target validity
        REQUIRE(cs.has_component<DestroyRequest>(event_entity));
    }

    SECTION("Property 11: DestroyRequest on event entity when target lacks Health") {
        // Generate random damage amounts
        auto damage_int = GENERATE(take(NUM_OUTER_TESTS, random(1, 200)));

        float damage = static_cast<float>(damage_int);

        EntityManager em;
        ComponentStorage cs;

        // Create a target entity WITHOUT Health component
        Entity target = em.create_entity();
        cs.add_component<EnemyTag>(target, EnemyTag{});
        // Intentionally no Health component

        // Create a DamageEvent entity targeting the entity without Health
        Entity event_entity = em.create_entity();
        cs.add_component<DamageEvent>(event_entity, DamageEvent{target, damage});

        // Verify no DestroyRequest before system runs
        REQUIRE_FALSE(cs.has_component<DestroyRequest>(event_entity));

        // Run DamageApplySystem
        DamageApplySystem system;
        system.update(em, cs);

        // Property 11: event entity SHALL receive DestroyRequest regardless of Health presence
        REQUIRE(cs.has_component<DestroyRequest>(event_entity));
    }
}

// ===========================================================================
// Feature: 090-09-cannon-laser, Property 9: Armor Reduction Correctness
//
// For any DamageEvent with amount A > 0 and target with armor_multiplier M
// in [0.0, 1.0], DamageApplySystem SHALL reduce the target's Health.current
// by exactly A × M (within floating-point tolerance of 0.001). When M == 1.0
// (no armor), effective damage SHALL equal the raw amount A.
//
// **Validates: Requirements 13.3, 13.4, 21.1, 21.3**
// ===========================================================================

TEST_CASE("DamageApplySystem: Armor Reduction Correctness",
          "[Game][cannon-laser][property]") {
    SECTION("Property 9: effective damage = amount * armor_multiplier within tolerance") {
        // Generate random damage amounts (ensure health > damage so floor doesn't interfere)
        auto damage_int = GENERATE(take(NUM_OUTER_TESTS, random(1, 100)));
        auto armor_pct = GENERATE(take(NUM_INNER_TESTS, random(0, 100)));

        float damage = static_cast<float>(damage_int);
        float armor_multiplier = static_cast<float>(armor_pct) / 100.0f; // [0.0, 1.0]

        // Set health high enough that floor at 0 won't interfere
        float initial_health = damage + 200.0f;

        EntityManager em;
        ComponentStorage cs;

        // Create target with Health and specific armor_multiplier
        Entity target = em.create_entity();
        cs.add_component<EnemyTag>(target, EnemyTag{});
        cs.add_component<Health>(target, Health{initial_health, initial_health, armor_multiplier});

        // Create DamageEvent
        Entity event_entity = em.create_entity();
        cs.add_component<DamageEvent>(event_entity, DamageEvent{target, damage});

        // Run DamageApplySystem
        DamageApplySystem system;
        system.update(em, cs);

        // Verify: health_after == health_before - (amount * armor_multiplier)
        auto health_opt = cs.get_component<Health>(target);
        REQUIRE(health_opt.has_value());
        float health_after = health_opt->get().current;

        float expected_effective = damage * armor_multiplier;
        float expected_health = initial_health - expected_effective;

        REQUIRE(std::abs(health_after - expected_health) < 0.001f);
    }

    SECTION("Property 9: When armor_multiplier == 1.0, effective damage equals raw amount") {
        // Generate random damage amounts
        auto damage_int = GENERATE(take(NUM_OUTER_TESTS, random(1, 100)));

        float damage = static_cast<float>(damage_int);
        float initial_health = damage + 200.0f; // Ensure no floor interference

        EntityManager em;
        ComponentStorage cs;

        // Create target with armor_multiplier = 1.0 (no armor)
        Entity target = em.create_entity();
        cs.add_component<EnemyTag>(target, EnemyTag{});
        cs.add_component<Health>(target, Health{initial_health, initial_health, 1.0f});

        // Create DamageEvent
        Entity event_entity = em.create_entity();
        cs.add_component<DamageEvent>(event_entity, DamageEvent{target, damage});

        // Run DamageApplySystem
        DamageApplySystem system;
        system.update(em, cs);

        // Verify: effective damage == raw amount (health reduced by exactly damage)
        auto health_opt = cs.get_component<Health>(target);
        REQUIRE(health_opt.has_value());
        float health_after = health_opt->get().current;

        float expected_health = initial_health - damage;
        REQUIRE(std::abs(health_after - expected_health) < 0.001f);
    }
}

// ===========================================================================
// Feature: 090-09-cannon-laser, Property 10: HP Floor
//
// For any DamageEvent processing, the target's Health.current after
// DamageApplySystem executes SHALL be >= 0.0f. No damage application SHALL
// reduce health below zero.
//
// **Validates: Requirements 13.5, 21.2**
// ===========================================================================

TEST_CASE("DamageApplySystem: HP Floor",
          "[Game][cannon-laser][property]") {
    SECTION("Property 10: Health.current >= 0.0f after damage exceeds health") {
        // Generate damage that EXCEEDS health to test floor behavior
        auto damage_int = GENERATE(take(NUM_OUTER_TESTS, random(200, 500)));
        auto health_int = GENERATE(take(NUM_INNER_TESTS, random(1, 50)));

        float damage = static_cast<float>(damage_int);
        float initial_health = static_cast<float>(health_int);

        EntityManager em;
        ComponentStorage cs;

        // Create target with low health (damage will exceed it)
        Entity target = em.create_entity();
        cs.add_component<EnemyTag>(target, EnemyTag{});
        cs.add_component<Health>(target, Health{initial_health, initial_health, 1.0f});

        // Create DamageEvent with damage >> health
        Entity event_entity = em.create_entity();
        cs.add_component<DamageEvent>(event_entity, DamageEvent{target, damage});

        // Run DamageApplySystem
        DamageApplySystem system;
        system.update(em, cs);

        // Property 10: Health.current SHALL be >= 0.0f
        auto health_opt = cs.get_component<Health>(target);
        REQUIRE(health_opt.has_value());
        REQUIRE(health_opt->get().current >= 0.0f);
    }

    SECTION("Property 10: HP floor with armor_multiplier < 1.0") {
        // Even with armor, if effective damage exceeds health, floor at 0
        auto damage_int = GENERATE(take(NUM_OUTER_TESTS, random(200, 500)));
        auto armor_pct = GENERATE(take(NUM_INNER_TESTS, random(50, 100)));

        float damage = static_cast<float>(damage_int);
        float armor_multiplier = static_cast<float>(armor_pct) / 100.0f;
        float initial_health = 10.0f; // Very low health to ensure floor triggers

        EntityManager em;
        ComponentStorage cs;

        // Create target with very low health and variable armor
        Entity target = em.create_entity();
        cs.add_component<EnemyTag>(target, EnemyTag{});
        cs.add_component<Health>(target, Health{initial_health, initial_health, armor_multiplier});

        // Create DamageEvent with damage >> health
        Entity event_entity = em.create_entity();
        cs.add_component<DamageEvent>(event_entity, DamageEvent{target, damage});

        // Run DamageApplySystem
        DamageApplySystem system;
        system.update(em, cs);

        // Property 10: Health.current SHALL be >= 0.0f regardless of armor
        auto health_opt = cs.get_component<Health>(target);
        REQUIRE(health_opt.has_value());
        REQUIRE(health_opt->get().current >= 0.0f);
    }
}
