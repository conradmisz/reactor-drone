/**
 * Unit tests for EntityManager class
 * 
 * These tests verify the core functionality of entity lifecycle management:
 * - Entity creation returns valid IDs
 * - Entity destruction marks entities as inactive
 * - is_alive() correctly reports entity status
 * - Entity IDs are reused after destruction
 * - active_count() accurately tracks the number of active entities
 * 
 * Requirements tested: 10.1, 10.2
 */

#include <catch2/catch_test_macros.hpp>
#include "engine/ecs/entity_manager.hpp"
#include <unordered_set>
#include <vector>

TEST_CASE("EntityManager: create_entity returns valid IDs", "[Engine][entity_manager][unit]") {
    EntityManager em;

    Entity e1 = em.create_entity();
    REQUIRE(em.is_alive(e1));
    REQUIRE(em.active_count() == 1);

    Entity e2 = em.create_entity();
    REQUIRE(em.is_alive(e2));
    REQUIRE(em.active_count() == 2);

    // Both entities should still be alive
    REQUIRE(em.is_alive(e1));
    REQUIRE(em.is_alive(e2));

    // Entity IDs should be different
    REQUIRE(e1 != e2);
}

TEST_CASE("EntityManager: destroy_entity marks inactive", "[Engine][entity_manager][unit]") {
    EntityManager em;

    Entity e = em.create_entity();
    REQUIRE(em.is_alive(e));
    REQUIRE(em.active_count() == 1);

    em.destroy_entity(e);
    REQUIRE_FALSE(em.is_alive(e));
    REQUIRE(em.active_count() == 0);
}

TEST_CASE("EntityManager: is_alive returns correct status", "[Engine][entity_manager][unit]") {
    EntityManager em;

    Entity e1 = em.create_entity();
    REQUIRE(em.is_alive(e1));

    em.destroy_entity(e1);
    REQUIRE_FALSE(em.is_alive(e1));

    Entity e2 = em.create_entity();
    REQUIRE(em.is_alive(e2));
}

TEST_CASE("EntityManager: ID reuse after destruction", "[Engine][entity_manager][unit]") {
    EntityManager em;

    Entity e1 = em.create_entity();
    Entity original_id = e1;
    em.destroy_entity(e1);
    REQUIRE_FALSE(em.is_alive(original_id));

    Entity e2 = em.create_entity();
    REQUIRE(e2 == original_id);
    REQUIRE(em.is_alive(e2));
}

TEST_CASE("EntityManager: ID reuse with multiple entities", "[Engine][entity_manager][unit]") {
    EntityManager em;

    Entity e1 = em.create_entity();
    Entity e2 = em.create_entity();
    Entity e3 = em.create_entity();

    REQUIRE(em.active_count() == 3);

    em.destroy_entity(e2);
    REQUIRE(em.active_count() == 2);
    REQUIRE(em.is_alive(e1));
    REQUIRE_FALSE(em.is_alive(e2));
    REQUIRE(em.is_alive(e3));

    Entity e4 = em.create_entity();
    REQUIRE(e4 == e2);
    REQUIRE(em.active_count() == 3);
}

TEST_CASE("EntityManager: active_count tracks correctly", "[Engine][entity_manager][unit]") {
    EntityManager em;

    REQUIRE(em.active_count() == 0);

    Entity e1 = em.create_entity();
    REQUIRE(em.active_count() == 1);

    Entity e2 = em.create_entity();
    REQUIRE(em.active_count() == 2);

    Entity e3 = em.create_entity();
    REQUIRE(em.active_count() == 3);

    em.destroy_entity(e1);
    REQUIRE(em.active_count() == 2);

    em.destroy_entity(e2);
    REQUIRE(em.active_count() == 1);

    em.destroy_entity(e3);
    REQUIRE(em.active_count() == 0);
}

TEST_CASE("EntityManager: destroying non-existent entity is safe", "[Engine][entity_manager][unit]") {
    EntityManager em;

    Entity e = em.create_entity();
    em.destroy_entity(e);

    // Destroying again should be safe (no-op)
    REQUIRE_NOTHROW(em.destroy_entity(e));
    REQUIRE(em.active_count() == 0);

    // Destroying an entity that never existed should also be safe
    Entity fake_entity = 9999;
    REQUIRE_NOTHROW(em.destroy_entity(fake_entity));
    REQUIRE(em.active_count() == 0);
}

TEST_CASE("EntityManager: multiple creates and destroys maintain count", "[Engine][entity_manager][unit]") {
    EntityManager em;
    std::vector<Entity> entities;

    for (int i = 0; i < 10; ++i) {
        entities.push_back(em.create_entity());
    }
    REQUIRE(em.active_count() == 10);

    for (int i = 0; i < 5; ++i) {
        em.destroy_entity(entities[i]);
    }
    REQUIRE(em.active_count() == 5);

    for (int i = 0; i < 3; ++i) {
        entities.push_back(em.create_entity());
    }
    REQUIRE(em.active_count() == 8);

    for (size_t i = 5; i < entities.size(); ++i) {
        em.destroy_entity(entities[i]);
    }
    REQUIRE(em.active_count() == 0);
}

TEST_CASE("EntityManager: entity IDs are unique among active entities", "[Engine][entity_manager][unit]") {
    EntityManager em;
    std::unordered_set<Entity> created_ids;

    for (int i = 0; i < 20; ++i) {
        Entity e = em.create_entity();
        REQUIRE(created_ids.find(e) == created_ids.end());
        created_ids.insert(e);
    }

    REQUIRE(em.active_count() == 20);
    REQUIRE(created_ids.size() == 20);
}

TEST_CASE("EntityManager: create destroy cycle", "[Engine][entity_manager][unit]") {
    EntityManager em;

    for (int cycle = 0; cycle < 5; ++cycle) {
        std::vector<Entity> entities;
        for (int i = 0; i < 5; ++i) {
            entities.push_back(em.create_entity());
        }
        REQUIRE(em.active_count() == 5);

        for (Entity e : entities) {
            REQUIRE(em.is_alive(e));
        }

        for (Entity e : entities) {
            em.destroy_entity(e);
        }
        REQUIRE(em.active_count() == 0);

        for (Entity e : entities) {
            REQUIRE_FALSE(em.is_alive(e));
        }
    }
}

TEST_CASE("EntityManager: all_entities returns every alive entity",
          "[Engine][entity_manager][unit]") {
    EntityManager em;

    SECTION("empty manager returns no entities") {
        REQUIRE(em.all_entities().empty());
    }

    SECTION("returns exactly the alive set, reflecting destroys") {
        Entity a = em.create_entity();
        Entity b = em.create_entity();
        Entity c = em.create_entity();

        auto all = em.all_entities();
        REQUIRE(all.size() == 3);
        std::unordered_set<Entity> s(all.begin(), all.end());
        REQUIRE(s.count(a) == 1);
        REQUIRE(s.count(b) == 1);
        REQUIRE(s.count(c) == 1);

        em.destroy_entity(b);
        auto remaining = em.all_entities();
        REQUIRE(remaining.size() == 2);
        std::unordered_set<Entity> s2(remaining.begin(), remaining.end());
        REQUIRE(s2.count(a) == 1);
        REQUIRE(s2.count(b) == 0);
        REQUIRE(s2.count(c) == 1);
    }

    SECTION("supports bulk teardown — mark all, then count is zero") {
        std::vector<Entity> made;
        for (int i = 0; i < 10; ++i) made.push_back(em.create_entity());
        for (Entity e : em.all_entities()) em.destroy_entity(e);
        REQUIRE(em.active_count() == 0);
        REQUIRE(em.all_entities().empty());
    }
}
