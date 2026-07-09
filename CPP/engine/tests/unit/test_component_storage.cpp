/**
 * Unit tests for ComponentStorage class
 *
 * These tests verify the core functionality of component storage:
 * - Adding and retrieving components
 * - get_component returns std::nullopt for non-existent components
 * - Removing components
 * - has_component returns correct boolean
 * - Multiple component types on single entity
 * - Component replacement (adding same type twice)
 *
 * Requirements tested: 10.1, 10.3
 */

#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"

/**
 * Test: Adding and retrieving components
 *
 * Verifies that add_component() stores component data and get_component()
 * retrieves the same data.
 */
TEST_CASE("Component storage adds and retrieves components", "[component_storage]") {
    ComponentStorage storage;
    Entity entity = 1;

    // Add a Position component
    Position pos{100.0f, 200.0f};
    storage.add_component(entity, pos);

    // Retrieve the Position component
    auto retrieved_pos = storage.get_component<Position>(entity);
    REQUIRE(retrieved_pos.has_value());
    CHECK(retrieved_pos->get().x == 100.0f);
    CHECK(retrieved_pos->get().y == 200.0f);
}

/**
 * Test: get_component returns std::nullopt for non-existent components
 *
 * Verifies that attempting to retrieve a component that doesn't exist
 * returns std::nullopt rather than crashing or returning invalid data.
 */
TEST_CASE("Get non-existent component returns nullopt", "[component_storage]") {
    ComponentStorage storage;
    Entity entity = 1;

    // Try to get a component that was never added
    auto pos = storage.get_component<Position>(entity);
    CHECK_FALSE(pos.has_value());

    auto size = storage.get_component<Size>(entity);
    CHECK_FALSE(size.has_value());

    auto color = storage.get_component<Color>(entity);
    CHECK_FALSE(color.has_value());
}

/**
 * Test: Removing components
 *
 * Verifies that remove_component() properly removes a component from an entity.
 */
TEST_CASE("Component storage removes components", "[component_storage]") {
    ComponentStorage storage;
    Entity entity = 1;

    // Add a Position component
    Position pos{100.0f, 200.0f};
    storage.add_component(entity, pos);

    // Verify it exists
    auto retrieved_pos = storage.get_component<Position>(entity);
    REQUIRE(retrieved_pos.has_value());

    // Remove the component
    storage.remove_component<Position>(entity);

    // Verify it no longer exists
    auto after_removal = storage.get_component<Position>(entity);
    CHECK_FALSE(after_removal.has_value());
}

/**
 * Test: Removing non-existent component is safe
 *
 * Verifies that calling remove_component() on a component that doesn't exist
 * is a safe no-op.
 */
TEST_CASE("Removing non-existent component is safe", "[component_storage]") {
    ComponentStorage storage;
    Entity entity = 1;

    // Remove a component that was never added - should not crash
    REQUIRE_NOTHROW(storage.remove_component<Position>(entity));
    REQUIRE_NOTHROW(storage.remove_component<Size>(entity));
    REQUIRE_NOTHROW(storage.remove_component<Color>(entity));
}

/**
 * Test: has_component returns correct boolean
 *
 * Verifies that has_component() correctly reports whether an entity
 * has a specific component type.
 */
TEST_CASE("has_component returns correct boolean", "[component_storage]") {
    ComponentStorage storage;
    Entity entity = 1;

    // Initially, entity has no components
    CHECK_FALSE(storage.has_component<Position>(entity));
    CHECK_FALSE(storage.has_component<Size>(entity));
    CHECK_FALSE(storage.has_component<Color>(entity));

    // Add a Position component
    Position pos{100.0f, 200.0f};
    storage.add_component(entity, pos);

    // Now entity has Position but not Size or Color
    CHECK(storage.has_component<Position>(entity));
    CHECK_FALSE(storage.has_component<Size>(entity));
    CHECK_FALSE(storage.has_component<Color>(entity));

    // Add a Size component
    Size size{50.0f, 75.0f};
    storage.add_component(entity, size);

    // Now entity has Position and Size but not Color
    CHECK(storage.has_component<Position>(entity));
    CHECK(storage.has_component<Size>(entity));
    CHECK_FALSE(storage.has_component<Color>(entity));

    // Remove Position
    storage.remove_component<Position>(entity);

    // Now entity has only Size
    CHECK_FALSE(storage.has_component<Position>(entity));
    CHECK(storage.has_component<Size>(entity));
    CHECK_FALSE(storage.has_component<Color>(entity));
}

/**
 * Test: Multiple component types on single entity
 *
 * Verifies that an entity can have multiple different component types
 * attached simultaneously, and that each can be retrieved independently.
 */
TEST_CASE("Multiple component types on single entity", "[component_storage]") {
    ComponentStorage storage;
    Entity entity = 1;

    // Add all three component types
    Position pos{350.0f, 250.0f};
    Size size{100.0f, 100.0f};
    Color color{255, 0, 0, 255};  // Red

    storage.add_component(entity, pos);
    storage.add_component(entity, size);
    storage.add_component(entity, color);

    // Verify all three components exist
    CHECK(storage.has_component<Position>(entity));
    CHECK(storage.has_component<Size>(entity));
    CHECK(storage.has_component<Color>(entity));

    // Retrieve and verify each component
    auto retrieved_pos = storage.get_component<Position>(entity);
    REQUIRE(retrieved_pos.has_value());
    CHECK(retrieved_pos->get().x == 350.0f);
    CHECK(retrieved_pos->get().y == 250.0f);

    auto retrieved_size = storage.get_component<Size>(entity);
    REQUIRE(retrieved_size.has_value());
    CHECK(retrieved_size->get().width == 100.0f);
    CHECK(retrieved_size->get().height == 100.0f);

    auto retrieved_color = storage.get_component<Color>(entity);
    REQUIRE(retrieved_color.has_value());
    CHECK(retrieved_color->get().r == 255);
    CHECK(retrieved_color->get().g == 0);
    CHECK(retrieved_color->get().b == 0);
    CHECK(retrieved_color->get().a == 255);
}

/**
 * Test: Component replacement (adding same type twice)
 *
 * Verifies that adding a component of a type the entity already has
 * replaces the old value rather than creating a duplicate.
 */
TEST_CASE("Component replacement when adding same type twice", "[component_storage]") {
    ComponentStorage storage;
    Entity entity = 1;

    // Add initial Position
    Position pos1{100.0f, 200.0f};
    storage.add_component(entity, pos1);

    // Verify initial value
    auto retrieved1 = storage.get_component<Position>(entity);
    REQUIRE(retrieved1.has_value());
    CHECK(retrieved1->get().x == 100.0f);
    CHECK(retrieved1->get().y == 200.0f);

    // Add a different Position (replacement)
    Position pos2{300.0f, 400.0f};
    storage.add_component(entity, pos2);

    // Verify the new value replaced the old one
    auto retrieved2 = storage.get_component<Position>(entity);
    REQUIRE(retrieved2.has_value());
    CHECK(retrieved2->get().x == 300.0f);
    CHECK(retrieved2->get().y == 400.0f);
}

/**
 * Test: Multiple entities with same component type
 *
 * Verifies that multiple entities can have the same component type
 * with different values, and that they don't interfere with each other.
 */
TEST_CASE("Multiple entities with same component type", "[component_storage]") {
    ComponentStorage storage;
    Entity entity1 = 1;
    Entity entity2 = 2;
    Entity entity3 = 3;

    // Add Position to all three entities with different values
    storage.add_component(entity1, Position{100.0f, 100.0f});
    storage.add_component(entity2, Position{200.0f, 200.0f});
    storage.add_component(entity3, Position{300.0f, 300.0f});

    // Verify each entity has its own Position
    auto pos1 = storage.get_component<Position>(entity1);
    REQUIRE(pos1.has_value());
    CHECK(pos1->get().x == 100.0f);
    CHECK(pos1->get().y == 100.0f);

    auto pos2 = storage.get_component<Position>(entity2);
    REQUIRE(pos2.has_value());
    CHECK(pos2->get().x == 200.0f);
    CHECK(pos2->get().y == 200.0f);

    auto pos3 = storage.get_component<Position>(entity3);
    REQUIRE(pos3.has_value());
    CHECK(pos3->get().x == 300.0f);
    CHECK(pos3->get().y == 300.0f);
}

/**
 * Test: Modifying retrieved component reference
 *
 * Verifies that the reference returned by get_component() can be used
 * to modify the stored component data.
 */
TEST_CASE("Modify retrieved component reference", "[component_storage]") {
    ComponentStorage storage;
    Entity entity = 1;

    // Add a Position component
    Position pos{100.0f, 200.0f};
    storage.add_component(entity, pos);

    // Get a mutable reference and modify it
    auto retrieved = storage.get_component<Position>(entity);
    REQUIRE(retrieved.has_value());
    retrieved->get().x = 500.0f;
    retrieved->get().y = 600.0f;

    // Verify the modification persisted
    auto after_modification = storage.get_component<Position>(entity);
    REQUIRE(after_modification.has_value());
    CHECK(after_modification->get().x == 500.0f);
    CHECK(after_modification->get().y == 600.0f);
}

/**
 * Test: Const get_component for read-only access
 *
 * Verifies that the const version of get_component() works correctly
 * for read-only access to components.
 */
TEST_CASE("Const get_component for read-only access", "[component_storage]") {
    ComponentStorage storage;
    Entity entity = 1;

    // Add a Position component
    Position pos{100.0f, 200.0f};
    storage.add_component(entity, pos);

    // Use const reference to storage
    const ComponentStorage& const_storage = storage;

    // Get component through const interface
    auto retrieved = const_storage.get_component<Position>(entity);
    REQUIRE(retrieved.has_value());
    CHECK(retrieved->get().x == 100.0f);
    CHECK(retrieved->get().y == 200.0f);

    // Verify non-existent component returns nullopt
    auto non_existent = const_storage.get_component<Size>(entity);
    CHECK_FALSE(non_existent.has_value());
}

/**
 * Test: entities_with_component returns correct entities
 *
 * Verifies that entities_with_component() returns all entities that have
 * a specific component type.
 */
TEST_CASE("entities_with_component returns correct entities", "[component_storage]") {
    ComponentStorage storage;

    // Create entities with various component combinations
    Entity e1 = 1;
    Entity e2 = 2;
    Entity e3 = 3;
    Entity e4 = 4;

    // e1 has Position
    storage.add_component(e1, Position{100.0f, 100.0f});

    // e2 has Position and Size
    storage.add_component(e2, Position{200.0f, 200.0f});
    storage.add_component(e2, Size{50.0f, 50.0f});

    // e3 has Size only
    storage.add_component(e3, Size{75.0f, 75.0f});

    // e4 has Color only
    storage.add_component(e4, Color{255, 0, 0, 255});

    // Get entities with Position
    auto entities_with_pos = storage.entities_with_component<Position>();
    CHECK(entities_with_pos.size() == 2);
    CHECK(std::find(entities_with_pos.begin(), entities_with_pos.end(), e1) != entities_with_pos.end());
    CHECK(std::find(entities_with_pos.begin(), entities_with_pos.end(), e2) != entities_with_pos.end());

    // Get entities with Size
    auto entities_with_size = storage.entities_with_component<Size>();
    CHECK(entities_with_size.size() == 2);
    CHECK(std::find(entities_with_size.begin(), entities_with_size.end(), e2) != entities_with_size.end());
    CHECK(std::find(entities_with_size.begin(), entities_with_size.end(), e3) != entities_with_size.end());

    // Get entities with Color
    auto entities_with_color = storage.entities_with_component<Color>();
    CHECK(entities_with_color.size() == 1);
    CHECK(std::find(entities_with_color.begin(), entities_with_color.end(), e4) != entities_with_color.end());
}

/**
 * Test: Integration with EntityManager
 *
 * Verifies that ComponentStorage works correctly with entities created
 * by EntityManager, including entity ID reuse scenarios.
 */
TEST_CASE("Integration with EntityManager", "[component_storage]") {
    EntityManager em;
    ComponentStorage storage;

    // Create entities using EntityManager
    Entity e1 = em.create_entity();
    Entity e2 = em.create_entity();

    // Add components to entities
    storage.add_component(e1, Position{100.0f, 100.0f});
    storage.add_component(e2, Position{200.0f, 200.0f});

    // Verify components exist
    CHECK(storage.has_component<Position>(e1));
    CHECK(storage.has_component<Position>(e2));

    // Destroy e1 and create a new entity (may reuse e1's ID)
    em.destroy_entity(e1);
    Entity e3 = em.create_entity();

    // e3 might have the same ID as e1, but should not have e1's components
    // (unless we explicitly add them)
    if (e3 == e1) {
        // ID was reused - old component data might still be there
        // This demonstrates that ComponentStorage and EntityManager are independent
        // In a production system, you'd want to clean up components when entities are destroyed
        CHECK(storage.has_component<Position>(e3));  // Old data still present

        // Remove the old component
        storage.remove_component<Position>(e3);
        CHECK_FALSE(storage.has_component<Position>(e3));
    } else {
        // Different ID - should not have any components
        CHECK_FALSE(storage.has_component<Position>(e3));
    }
}
