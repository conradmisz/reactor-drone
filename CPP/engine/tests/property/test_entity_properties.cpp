/**
 * Property-based tests for EntityManager
 *
 * These tests verify universal properties that should hold across all possible
 * sequences of entity operations. Unlike unit tests that check specific examples,
 * property tests generate random operation sequences to ensure correctness under
 * arbitrary usage patterns.
 *
 * Testing Framework: Catch2 v3 with GENERATE-driven seed iteration
 *
 * Requirements tested: 11.1, 11.2, 11.5
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>
#include "engine/ecs/entity_manager.hpp"
#include <vector>
#include <random>
#include <algorithm>
#include <unordered_set>

static constexpr int NUM_OUTER_TESTS = 10;
[[maybe_unused]] static constexpr int NUM_INNER_TESTS = 5;  // required by property-test-bounds policy

/**
 * Property 4: Entity count invariant
 *
 * **Validates: Requirements 1.6, 11.2**
 *
 * Universal Property:
 * For any sequence of create_entity() and destroy_entity() operations,
 * the number of active entities must equal the total number of creates
 * minus the total number of destroys.
 *
 * Mathematical formulation:
 *   active_count() = creates - destroys
 */
TEST_CASE("Property: entity count invariant", "[entity_manager]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 1000000)));
    std::mt19937 gen(seed);
    std::uniform_int_distribution<> op_dist(0, 1);  // 0 = create, 1 = destroy
    std::uniform_int_distribution<> length_dist(10, 100);  // Sequence length

    EntityManager em;
    std::vector<Entity> entities;
    int creates = 0;
    int destroys = 0;

    // Generate random sequence length for this iteration
    int sequence_length = length_dist(gen);

    // Generate and execute random sequence of operations
    for (int op = 0; op < sequence_length; ++op) {
        int operation = op_dist(gen);

        if (operation == 0 || entities.empty()) {
            // Create entity
            Entity e = em.create_entity();
            entities.push_back(e);
            creates++;

            // Verify invariant after create
            REQUIRE(em.active_count() == static_cast<size_t>(creates - destroys));

        } else {
            // Destroy random entity
            std::uniform_int_distribution<> entity_dist(0, static_cast<int>(entities.size()) - 1);
            int idx = entity_dist(gen);
            Entity e = entities[idx];

            em.destroy_entity(e);
            entities.erase(entities.begin() + idx);
            destroys++;

            // Verify invariant after destroy
            REQUIRE(em.active_count() == static_cast<size_t>(creates - destroys));
        }
    }

    // Final verification: active_count should equal creates - destroys
    REQUIRE(em.active_count() == static_cast<size_t>(creates - destroys));

    // Additional verification: number of entities in our tracking vector
    // should match active_count
    REQUIRE(entities.size() == static_cast<size_t>(em.active_count()));
}

/**
 * Property 4 Extended: Entity count invariant with edge cases
 *
 * **Validates: Requirements 1.6, 11.2**
 *
 * This test extends Property 4 by specifically testing edge cases:
 * - All creates followed by all destroys
 * - Alternating create/destroy patterns
 * - Multiple destroys of the same entity (should be safe no-ops)
 * - Creating many entities then destroying them in random order
 */
TEST_CASE("Property: entity count invariant edge cases", "[entity_manager]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 1000000)));
    std::mt19937 gen(seed);

    EntityManager em;

    // Edge Case 1: Create many entities, then destroy all
    {
        std::vector<Entity> entities;
        int num_entities = 50;

        // Create phase
        for (int i = 0; i < num_entities; ++i) {
            entities.push_back(em.create_entity());
        }
        REQUIRE(em.active_count() == static_cast<size_t>(num_entities));

        // Destroy phase - in random order
        std::shuffle(entities.begin(), entities.end(), gen);
        for (int i = 0; i < num_entities; ++i) {
            em.destroy_entity(entities[i]);
            REQUIRE(em.active_count() == static_cast<size_t>(num_entities - i - 1));
        }
        REQUIRE(em.active_count() == 0u);
    }

    // Edge Case 2: Alternating create/destroy
    {
        for (int i = 0; i < 20; ++i) {
            Entity e = em.create_entity();
            REQUIRE(em.active_count() == 1u);
            em.destroy_entity(e);
            REQUIRE(em.active_count() == 0u);
        }
    }

    // Edge Case 3: Multiple destroys of same entity (should be safe)
    {
        Entity e = em.create_entity();
        REQUIRE(em.active_count() == 1u);

        em.destroy_entity(e);
        REQUIRE(em.active_count() == 0u);

        // Destroying again should be safe no-op
        em.destroy_entity(e);
        REQUIRE(em.active_count() == 0u);

        em.destroy_entity(e);
        REQUIRE(em.active_count() == 0u);
    }

    // Edge Case 4: Create, destroy some, create more, verify count
    {
        std::vector<Entity> entities;

        // Create 10 entities
        for (int i = 0; i < 10; ++i) {
            entities.push_back(em.create_entity());
        }
        REQUIRE(em.active_count() == 10u);

        // Destroy 5 random entities
        std::shuffle(entities.begin(), entities.end(), gen);
        for (int i = 0; i < 5; ++i) {
            em.destroy_entity(entities[i]);
        }
        entities.erase(entities.begin(), entities.begin() + 5);
        REQUIRE(em.active_count() == 5u);

        // Create 7 more entities
        for (int i = 0; i < 7; ++i) {
            entities.push_back(em.create_entity());
        }
        REQUIRE(em.active_count() == 12u);

        // Destroy all remaining
        for (Entity e : entities) {
            em.destroy_entity(e);
        }
        REQUIRE(em.active_count() == 0u);
    }
}

/**
 * Property 4 Stress Test: Large-scale entity count invariant
 *
 * **Validates: Requirements 1.6, 11.2**
 *
 * This test verifies the entity count invariant holds even with large numbers
 * of entities and operations.
 */
TEST_CASE("Property: entity count invariant large scale", "[entity_manager]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 1000000)));
    std::mt19937 gen(seed);
    std::uniform_int_distribution<> op_dist(0, 1);

    const int OPERATIONS_PER_ITERATION = 1000;

    EntityManager em;
    std::vector<Entity> entities;
    int creates = 0;
    int destroys = 0;

    for (int op = 0; op < OPERATIONS_PER_ITERATION; ++op) {
        int operation = op_dist(gen);

        if (operation == 0 || entities.empty()) {
            // Create entity
            entities.push_back(em.create_entity());
            creates++;
        } else {
            // Destroy random entity
            std::uniform_int_distribution<> entity_dist(0, static_cast<int>(entities.size()) - 1);
            int idx = entity_dist(gen);
            em.destroy_entity(entities[idx]);
            entities.erase(entities.begin() + idx);
            destroys++;
        }

        // Verify invariant periodically (every 100 operations)
        if (op % 100 == 0) {
            REQUIRE(em.active_count() == static_cast<size_t>(creates - destroys));
        }
    }

    // Final verification
    REQUIRE(em.active_count() == static_cast<size_t>(creates - destroys));
}

/**
 * Property 1: Entity ID uniqueness
 *
 * **Validates: Requirements 1.1, 1.3**
 *
 * Universal Property:
 * For any sequence of entity creation operations, all returned entity IDs
 * must be unique among currently active entities.
 */
TEST_CASE("Property: entity ID uniqueness", "[entity_manager]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 1000000)));
    std::mt19937 gen(seed);
    std::uniform_int_distribution<> op_dist(0, 1);  // 0 = create, 1 = destroy
    std::uniform_int_distribution<> length_dist(50, 200);  // Sequence length

    EntityManager em;
    std::vector<Entity> active_entities;
    std::unordered_set<Entity> active_ids;

    // Generate random sequence length for this iteration
    int sequence_length = length_dist(gen);

    // Generate and execute random sequence of operations
    for (int op = 0; op < sequence_length; ++op) {
        int operation = op_dist(gen);

        if (operation == 0 || active_entities.empty()) {
            // Create entity
            Entity e = em.create_entity();

            // Verify the new ID is not already in use
            REQUIRE(active_ids.count(e) == 0);

            // Verify the entity is marked as alive
            REQUIRE(em.is_alive(e));

            // Add to tracking structures
            active_entities.push_back(e);
            active_ids.insert(e);

            // Verify uniqueness: set size should equal vector size
            REQUIRE(active_ids.size() == active_entities.size());

        } else {
            // Destroy random entity
            std::uniform_int_distribution<> entity_dist(0, static_cast<int>(active_entities.size()) - 1);
            int idx = entity_dist(gen);
            Entity e = active_entities[idx];

            // Verify the entity is currently alive before destroying
            REQUIRE(em.is_alive(e));

            // Destroy the entity
            em.destroy_entity(e);

            // Verify the entity is no longer alive
            REQUIRE_FALSE(em.is_alive(e));

            // Remove from tracking structures
            active_entities.erase(active_entities.begin() + idx);
            active_ids.erase(e);

            // Verify uniqueness still holds
            REQUIRE(active_ids.size() == active_entities.size());
        }
    }

    // Final verification: all active entities should have unique IDs
    REQUIRE(active_ids.size() == active_entities.size());

    // Verify active_count matches our tracking
    REQUIRE(em.active_count() == active_entities.size());

    // Verify all tracked entities are marked as alive
    for (Entity e : active_entities) {
        REQUIRE(em.is_alive(e));
    }
}

/**
 * Property 1 Extended: Entity ID uniqueness with ID reuse
 *
 * **Validates: Requirements 1.1, 1.3, 1.4**
 *
 * This test specifically focuses on ID reuse scenarios.
 */
TEST_CASE("Property: entity ID uniqueness with reuse", "[entity_manager]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 1000000)));
    std::mt19937 gen(seed);

    EntityManager em;

    // Pattern 1: Create N entities, destroy all, create N more
    // This should trigger ID reuse
    {
        std::vector<Entity> first_batch;
        std::unordered_set<Entity> first_batch_ids;

        // Create first batch
        for (int i = 0; i < 20; ++i) {
            Entity e = em.create_entity();
            REQUIRE(first_batch_ids.count(e) == 0);
            first_batch.push_back(e);
            first_batch_ids.insert(e);
        }
        REQUIRE(first_batch_ids.size() == 20);

        // Destroy all from first batch
        for (Entity e : first_batch) {
            em.destroy_entity(e);
            REQUIRE_FALSE(em.is_alive(e));
        }
        REQUIRE(em.active_count() == 0u);

        // Create second batch - IDs may be reused from first batch
        std::vector<Entity> second_batch;
        std::unordered_set<Entity> second_batch_ids;

        for (int i = 0; i < 20; ++i) {
            Entity e = em.create_entity();

            // Verify uniqueness within second batch
            REQUIRE(second_batch_ids.count(e) == 0);

            // Verify entity is alive
            REQUIRE(em.is_alive(e));

            second_batch.push_back(e);
            second_batch_ids.insert(e);
        }
        REQUIRE(second_batch_ids.size() == 20);
        REQUIRE(em.active_count() == 20u);

        // Clean up
        for (Entity e : second_batch) {
            em.destroy_entity(e);
        }
    }

    // Pattern 2: Interleaved create/destroy with uniqueness checks
    {
        std::vector<Entity> active;
        std::unordered_set<Entity> active_ids;

        for (int cycle = 0; cycle < 10; ++cycle) {
            // Create 5 entities
            for (int i = 0; i < 5; ++i) {
                Entity e = em.create_entity();
                REQUIRE(active_ids.count(e) == 0);
                active.push_back(e);
                active_ids.insert(e);
            }

            // Destroy 3 random entities
            std::shuffle(active.begin(), active.end(), gen);
            for (int i = 0; i < 3 && !active.empty(); ++i) {
                Entity e = active.back();
                active.pop_back();
                active_ids.erase(e);
                em.destroy_entity(e);
            }

            // Verify uniqueness after each cycle
            REQUIRE(active_ids.size() == active.size());
            REQUIRE(em.active_count() == active.size());
        }

        // Clean up remaining entities
        for (Entity e : active) {
            em.destroy_entity(e);
        }
    }

    // Pattern 3: Create many, destroy in specific order, verify uniqueness throughout
    {
        std::vector<Entity> entities;
        std::unordered_set<Entity> entity_ids;

        // Create 50 entities
        for (int i = 0; i < 50; ++i) {
            Entity e = em.create_entity();
            REQUIRE(entity_ids.count(e) == 0);
            entities.push_back(e);
            entity_ids.insert(e);
        }
        REQUIRE(entity_ids.size() == 50);

        // Destroy every other entity
        for (size_t i = 0; i < entities.size(); i += 2) {
            Entity e = entities[i];
            em.destroy_entity(e);
            entity_ids.erase(e);
        }

        // Verify remaining entities still have unique IDs
        REQUIRE(entity_ids.size() == 25);

        // Create 25 more entities (may reuse IDs from destroyed entities)
        for (int i = 0; i < 25; ++i) {
            Entity e = em.create_entity();
            REQUIRE(entity_ids.count(e) == 0);
            entity_ids.insert(e);
        }
        REQUIRE(entity_ids.size() == 50);
        REQUIRE(em.active_count() == 50u);
    }
}

/**
 * Property 1 Stress Test: Entity ID uniqueness at scale
 *
 * **Validates: Requirements 1.1, 1.3**
 */
TEST_CASE("Property: entity ID uniqueness large scale", "[entity_manager]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 1000000)));
    std::mt19937 gen(seed);
    std::uniform_int_distribution<> op_dist(0, 1);

    const int OPERATIONS_PER_ITERATION = 1000;

    EntityManager em;
    std::unordered_set<Entity> active_ids;
    std::vector<Entity> active_entities;

    for (int op = 0; op < OPERATIONS_PER_ITERATION; ++op) {
        int operation = op_dist(gen);

        if (operation == 0 || active_entities.empty()) {
            // Create entity
            Entity e = em.create_entity();

            // Verify uniqueness
            REQUIRE(active_ids.count(e) == 0);

            active_entities.push_back(e);
            active_ids.insert(e);

        } else {
            // Destroy random entity
            std::uniform_int_distribution<> entity_dist(0, static_cast<int>(active_entities.size()) - 1);
            int idx = entity_dist(gen);
            Entity e = active_entities[idx];

            em.destroy_entity(e);
            active_entities.erase(active_entities.begin() + idx);
            active_ids.erase(e);
        }

        // Verify uniqueness periodically (every 100 operations)
        if (op % 100 == 0) {
            REQUIRE(active_ids.size() == active_entities.size());
            REQUIRE(em.active_count() == active_entities.size());
        }
    }

    // Final verification
    REQUIRE(active_ids.size() == active_entities.size());
}
