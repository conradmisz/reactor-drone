/**
 * Property-based tests for ComponentStorage
 *
 * These tests verify universal properties that should hold across all possible
 * sequences of component operations. Unlike unit tests that check specific examples,
 * property tests generate random operation sequences to ensure correctness under
 * arbitrary usage patterns.
 *
 * Testing Framework: Catch2 v3 with GENERATE-driven seed iteration
 *
 * Requirements tested: 11.1, 11.3, 11.4, 11.5
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include <vector>
#include <random>
#include <algorithm>
#include <limits>

static constexpr int NUM_OUTER_TESTS = 10;
static constexpr int NUM_INNER_TESTS = 5;

/**
 * Helper function to generate random float values
 * Returns values in range [-1000.0, 1000.0] for position/size testing
 */
static float rand_float(std::mt19937& gen) {
    std::uniform_real_distribution<float> dist(-1000.0f, 1000.0f);
    return dist(gen);
}

/**
 * Helper function to generate random uint8_t values
 * Returns values in range [0, 255] for color channel testing
 */
static uint8_t rand_byte(std::mt19937& gen) {
    std::uniform_int_distribution<int> dist(0, 255);
    return static_cast<uint8_t>(dist(gen));
}

/**
 * Helper function to compare Position components for equality
 */
static bool positions_equal(const Position& a, const Position& b) {
    return a.x == b.x && a.y == b.y;
}

/**
 * Helper function to compare Size components for equality
 */
static bool sizes_equal(const Size& a, const Size& b) {
    return a.width == b.width && a.height == b.height;
}

/**
 * Helper function to compare Color components for equality
 */
static bool colors_equal(const Color& a, const Color& b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

/**
 * Property 5: Component storage round-trip
 *
 * **Validates: Requirements 2.4, 2.5**
 *
 * Universal Property:
 * For any entity and any component value, after calling add_component(entity, component),
 * calling get_component(entity) must return a component equal to the one that was added.
 */
TEST_CASE("Property: component storage round-trip", "[component_storage]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 1000000)));
    std::mt19937 gen(seed);

    EntityManager em;
    ComponentStorage storage;

    // Create a random number of entities for this iteration
    std::uniform_int_distribution<> entity_count_dist(1, 20);
    int num_entities = entity_count_dist(gen);

    std::vector<Entity> entities;
    for (int i = 0; i < num_entities; ++i) {
        entities.push_back(em.create_entity());
    }

    // For each entity, test round-trip for all component types
    for (Entity e : entities) {
        // Test Position round-trip
        Position pos{rand_float(gen), rand_float(gen)};
        storage.add_component(e, pos);

        auto retrieved_pos = storage.get_component<Position>(e);
        REQUIRE(retrieved_pos.has_value());
        REQUIRE(positions_equal(retrieved_pos->get(), pos));

        // Test Size round-trip
        Size size{rand_float(gen), rand_float(gen)};
        storage.add_component(e, size);

        auto retrieved_size = storage.get_component<Size>(e);
        REQUIRE(retrieved_size.has_value());
        REQUIRE(sizes_equal(retrieved_size->get(), size));

        // Test Color round-trip
        Color color{rand_byte(gen), rand_byte(gen), rand_byte(gen), rand_byte(gen)};
        storage.add_component(e, color);

        auto retrieved_color = storage.get_component<Color>(e);
        REQUIRE(retrieved_color.has_value());
        REQUIRE(colors_equal(retrieved_color->get(), color));
    }

    // Verify all components are still retrievable after adding to all entities
    for (Entity e : entities) {
        REQUIRE(storage.has_component<Position>(e));
        REQUIRE(storage.has_component<Size>(e));
        REQUIRE(storage.has_component<Color>(e));
    }
}

/**
 * Property 5 Extended: Component storage round-trip with edge cases
 *
 * **Validates: Requirements 2.4, 2.5**
 *
 * This test extends Property 5 by specifically testing edge cases:
 * - Zero values (0.0f for floats, 0 for color channels)
 * - Negative values for position/size
 * - Very large values (near float limits)
 * - All combinations of color channels (all 0, all 255, mixed)
 */
TEST_CASE("Property: component storage round-trip edge cases", "[component_storage]") {
    auto _iter = GENERATE(take(NUM_OUTER_TESTS, random(0, 1)));
    (void)_iter;

    EntityManager em;
    ComponentStorage storage;

    for (int i = 0; i < NUM_INNER_TESTS; ++i) {
        Entity e = em.create_entity();

        // Edge Case 1: Zero values
        {
            Position pos{0.0f, 0.0f};
            storage.add_component(e, pos);
            auto retrieved = storage.get_component<Position>(e);
            REQUIRE(retrieved.has_value());
            REQUIRE(positions_equal(retrieved->get(), pos));

            Size size{0.0f, 0.0f};
            storage.add_component(e, size);
            auto retrieved_size = storage.get_component<Size>(e);
            REQUIRE(retrieved_size.has_value());
            REQUIRE(sizes_equal(retrieved_size->get(), size));

            Color color{0, 0, 0, 0};
            storage.add_component(e, color);
            auto retrieved_color = storage.get_component<Color>(e);
            REQUIRE(retrieved_color.has_value());
            REQUIRE(colors_equal(retrieved_color->get(), color));
        }

        // Edge Case 2: Negative values
        {
            Position pos{-999.9f, -888.8f};
            storage.add_component(e, pos);
            auto retrieved = storage.get_component<Position>(e);
            REQUIRE(retrieved.has_value());
            REQUIRE(positions_equal(retrieved->get(), pos));

            Size size{-100.0f, -200.0f};
            storage.add_component(e, size);
            auto retrieved_size = storage.get_component<Size>(e);
            REQUIRE(retrieved_size.has_value());
            REQUIRE(sizes_equal(retrieved_size->get(), size));
        }

        // Edge Case 3: Very large values
        {
            Position pos{10000.0f, 20000.0f};
            storage.add_component(e, pos);
            auto retrieved = storage.get_component<Position>(e);
            REQUIRE(retrieved.has_value());
            REQUIRE(positions_equal(retrieved->get(), pos));

            Size size{5000.0f, 6000.0f};
            storage.add_component(e, size);
            auto retrieved_size = storage.get_component<Size>(e);
            REQUIRE(retrieved_size.has_value());
            REQUIRE(sizes_equal(retrieved_size->get(), size));
        }

        // Edge Case 4: Maximum color values
        {
            Color color{255, 255, 255, 255};
            storage.add_component(e, color);
            auto retrieved = storage.get_component<Color>(e);
            REQUIRE(retrieved.has_value());
            REQUIRE(colors_equal(retrieved->get(), color));
        }

        // Edge Case 5: Mixed color values
        {
            Color color{128, 64, 192, 32};
            storage.add_component(e, color);
            auto retrieved = storage.get_component<Color>(e);
            REQUIRE(retrieved.has_value());
            REQUIRE(colors_equal(retrieved->get(), color));
        }

        em.destroy_entity(e);
    }
}

/**
 * Property 6: Component add-remove round-trip
 *
 * **Validates: Requirements 2.6, 11.3**
 *
 * Universal Property:
 * For any entity, if the entity initially has no component of type T, then after
 * add_component<T>() followed by remove_component<T>(), get_component<T>() must
 * return std::nullopt (the entity returns to its previous state).
 */
TEST_CASE("Property: component add-remove round-trip", "[component_storage]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 1000000)));
    std::mt19937 gen(seed);

    EntityManager em;
    ComponentStorage storage;

    // Create a random number of entities for this iteration
    std::uniform_int_distribution<> entity_count_dist(1, 20);
    int num_entities = entity_count_dist(gen);

    std::vector<Entity> entities;
    for (int i = 0; i < num_entities; ++i) {
        entities.push_back(em.create_entity());
    }

    // For each entity, test add-remove round-trip for all component types
    for (Entity e : entities) {
        // Verify entity starts with no components
        REQUIRE_FALSE(storage.has_component<Position>(e));
        REQUIRE_FALSE(storage.has_component<Size>(e));
        REQUIRE_FALSE(storage.has_component<Color>(e));

        // Test Position add-remove round-trip
        Position pos{rand_float(gen), rand_float(gen)};
        storage.add_component(e, pos);
        REQUIRE(storage.has_component<Position>(e));

        storage.remove_component<Position>(e);
        REQUIRE_FALSE(storage.has_component<Position>(e));

        auto retrieved_pos = storage.get_component<Position>(e);
        REQUIRE_FALSE(retrieved_pos.has_value());

        // Test Size add-remove round-trip
        Size size{rand_float(gen), rand_float(gen)};
        storage.add_component(e, size);
        REQUIRE(storage.has_component<Size>(e));

        storage.remove_component<Size>(e);
        REQUIRE_FALSE(storage.has_component<Size>(e));

        auto retrieved_size = storage.get_component<Size>(e);
        REQUIRE_FALSE(retrieved_size.has_value());

        // Test Color add-remove round-trip
        Color color{rand_byte(gen), rand_byte(gen), rand_byte(gen), rand_byte(gen)};
        storage.add_component(e, color);
        REQUIRE(storage.has_component<Color>(e));

        storage.remove_component<Color>(e);
        REQUIRE_FALSE(storage.has_component<Color>(e));

        auto retrieved_color = storage.get_component<Color>(e);
        REQUIRE_FALSE(retrieved_color.has_value());
    }

    // Verify all entities still have no components
    for (Entity e : entities) {
        REQUIRE_FALSE(storage.has_component<Position>(e));
        REQUIRE_FALSE(storage.has_component<Size>(e));
        REQUIRE_FALSE(storage.has_component<Color>(e));
    }
}

/**
 * Property 6 Extended: Component add-remove round-trip with multiple cycles
 *
 * **Validates: Requirements 2.6, 11.3**
 *
 * This test extends Property 6 by testing multiple add-remove cycles on the same entity.
 */
TEST_CASE("Property: component add-remove multiple cycles", "[component_storage]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 1000000)));
    std::mt19937 gen(seed);

    const int CYCLES_PER_ITERATION = 10;

    EntityManager em;
    ComponentStorage storage;
    Entity e = em.create_entity();

    // Perform multiple add-remove cycles
    for (int cycle = 0; cycle < CYCLES_PER_ITERATION; ++cycle) {
        // Verify entity starts cycle with no components
        REQUIRE_FALSE(storage.has_component<Position>(e));

        // Add Position
        Position pos{rand_float(gen), rand_float(gen)};
        storage.add_component(e, pos);
        REQUIRE(storage.has_component<Position>(e));

        // Verify we can retrieve it
        auto retrieved = storage.get_component<Position>(e);
        REQUIRE(retrieved.has_value());
        REQUIRE(positions_equal(retrieved->get(), pos));

        // Remove Position
        storage.remove_component<Position>(e);
        REQUIRE_FALSE(storage.has_component<Position>(e));

        // Verify it's really gone
        auto after_remove = storage.get_component<Position>(e);
        REQUIRE_FALSE(after_remove.has_value());
    }

    // Final verification: entity should have no components
    REQUIRE_FALSE(storage.has_component<Position>(e));
    REQUIRE_FALSE(storage.has_component<Size>(e));
    REQUIRE_FALSE(storage.has_component<Color>(e));
}

/**
 * Property 6 Complex: Add-remove with multiple component types
 *
 * **Validates: Requirements 2.6, 2.7, 11.3**
 *
 * This test verifies that add-remove operations on one component type don't
 * affect other component types on the same entity.
 */
TEST_CASE("Property: component add-remove independence", "[component_storage]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 1000000)));
    std::mt19937 gen(seed);

    EntityManager em;
    ComponentStorage storage;
    Entity e = em.create_entity();

    // Add all three component types
    Position pos{rand_float(gen), rand_float(gen)};
    Size size{rand_float(gen), rand_float(gen)};
    Color color{rand_byte(gen), rand_byte(gen), rand_byte(gen), rand_byte(gen)};

    storage.add_component(e, pos);
    storage.add_component(e, size);
    storage.add_component(e, color);

    // Verify all exist
    REQUIRE(storage.has_component<Position>(e));
    REQUIRE(storage.has_component<Size>(e));
    REQUIRE(storage.has_component<Color>(e));

    // Remove Position - Size and Color should remain
    storage.remove_component<Position>(e);
    REQUIRE_FALSE(storage.has_component<Position>(e));
    REQUIRE(storage.has_component<Size>(e));
    REQUIRE(storage.has_component<Color>(e));

    // Verify Size and Color values are unchanged
    auto retrieved_size = storage.get_component<Size>(e);
    REQUIRE(retrieved_size.has_value());
    REQUIRE(sizes_equal(retrieved_size->get(), size));

    auto retrieved_color = storage.get_component<Color>(e);
    REQUIRE(retrieved_color.has_value());
    REQUIRE(colors_equal(retrieved_color->get(), color));

    // Remove Size - Color should remain
    storage.remove_component<Size>(e);
    REQUIRE_FALSE(storage.has_component<Position>(e));
    REQUIRE_FALSE(storage.has_component<Size>(e));
    REQUIRE(storage.has_component<Color>(e));

    // Verify Color value is still unchanged
    retrieved_color = storage.get_component<Color>(e);
    REQUIRE(retrieved_color.has_value());
    REQUIRE(colors_equal(retrieved_color->get(), color));

    // Remove Color - entity should have no components
    storage.remove_component<Color>(e);
    REQUIRE_FALSE(storage.has_component<Position>(e));
    REQUIRE_FALSE(storage.has_component<Size>(e));
    REQUIRE_FALSE(storage.has_component<Color>(e));
}

/**
 * Property 11: Component retrieval idempotence
 *
 * **Validates: Requirements 11.4**
 *
 * Universal Property:
 * For any entity and component type, calling get_component() twice in succession
 * must return the same result both times (either the same component value or
 * std::nullopt both times).
 */
TEST_CASE("Property: component retrieval idempotence", "[component_storage]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 1000000)));
    std::mt19937 gen(seed);

    EntityManager em;
    ComponentStorage storage;

    // Create entities with and without components
    std::vector<Entity> entities_with_components;
    std::vector<Entity> entities_without_components;

    // Create 10 entities with all components
    for (int i = 0; i < 10; ++i) {
        Entity e = em.create_entity();
        storage.add_component(e, Position{rand_float(gen), rand_float(gen)});
        storage.add_component(e, Size{rand_float(gen), rand_float(gen)});
        storage.add_component(e, Color{rand_byte(gen), rand_byte(gen), rand_byte(gen), rand_byte(gen)});
        entities_with_components.push_back(e);
    }

    // Create 10 entities without components
    for (int i = 0; i < 10; ++i) {
        Entity e = em.create_entity();
        entities_without_components.push_back(e);
    }

    // Test idempotence for entities WITH components
    for (Entity e : entities_with_components) {
        // Test Position idempotence
        auto pos1 = storage.get_component<Position>(e);
        auto pos2 = storage.get_component<Position>(e);
        auto pos3 = storage.get_component<Position>(e);

        REQUIRE(pos1.has_value());
        REQUIRE(pos2.has_value());
        REQUIRE(pos3.has_value());

        REQUIRE(positions_equal(pos1->get(), pos2->get()));
        REQUIRE(positions_equal(pos2->get(), pos3->get()));

        // Test Size idempotence
        auto size1 = storage.get_component<Size>(e);
        auto size2 = storage.get_component<Size>(e);
        auto size3 = storage.get_component<Size>(e);

        REQUIRE(size1.has_value());
        REQUIRE(size2.has_value());
        REQUIRE(size3.has_value());

        REQUIRE(sizes_equal(size1->get(), size2->get()));
        REQUIRE(sizes_equal(size2->get(), size3->get()));

        // Test Color idempotence
        auto color1 = storage.get_component<Color>(e);
        auto color2 = storage.get_component<Color>(e);
        auto color3 = storage.get_component<Color>(e);

        REQUIRE(color1.has_value());
        REQUIRE(color2.has_value());
        REQUIRE(color3.has_value());

        REQUIRE(colors_equal(color1->get(), color2->get()));
        REQUIRE(colors_equal(color2->get(), color3->get()));
    }

    // Test idempotence for entities WITHOUT components
    for (Entity e : entities_without_components) {
        // Test Position idempotence (should return nullopt consistently)
        auto pos1 = storage.get_component<Position>(e);
        auto pos2 = storage.get_component<Position>(e);
        auto pos3 = storage.get_component<Position>(e);

        REQUIRE_FALSE(pos1.has_value());
        REQUIRE_FALSE(pos2.has_value());
        REQUIRE_FALSE(pos3.has_value());

        // Test Size idempotence (should return nullopt consistently)
        auto size1 = storage.get_component<Size>(e);
        auto size2 = storage.get_component<Size>(e);
        auto size3 = storage.get_component<Size>(e);

        REQUIRE_FALSE(size1.has_value());
        REQUIRE_FALSE(size2.has_value());
        REQUIRE_FALSE(size3.has_value());

        // Test Color idempotence (should return nullopt consistently)
        auto color1 = storage.get_component<Color>(e);
        auto color2 = storage.get_component<Color>(e);
        auto color3 = storage.get_component<Color>(e);

        REQUIRE_FALSE(color1.has_value());
        REQUIRE_FALSE(color2.has_value());
        REQUIRE_FALSE(color3.has_value());
    }
}

/**
 * Property 11 Extended: Component retrieval idempotence with modifications
 *
 * **Validates: Requirements 11.4**
 *
 * This test verifies that idempotence holds even when we modify the component
 * through the reference returned by get_component(). After modification, subsequent
 * get_component() calls should return the modified value consistently.
 */
TEST_CASE("Property: component retrieval idempotence with modification", "[component_storage]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 1000000)));
    std::mt19937 gen(seed);

    EntityManager em;
    ComponentStorage storage;
    Entity e = em.create_entity();

    // Add a Position component
    Position initial_pos{rand_float(gen), rand_float(gen)};
    storage.add_component(e, initial_pos);

    // Get the component and modify it
    auto pos_ref = storage.get_component<Position>(e);
    REQUIRE(pos_ref.has_value());

    float new_x = rand_float(gen);
    float new_y = rand_float(gen);
    pos_ref->get().x = new_x;
    pos_ref->get().y = new_y;

    // Now verify idempotence with the modified value
    auto pos1 = storage.get_component<Position>(e);
    auto pos2 = storage.get_component<Position>(e);
    auto pos3 = storage.get_component<Position>(e);

    REQUIRE(pos1.has_value());
    REQUIRE(pos2.has_value());
    REQUIRE(pos3.has_value());

    // All retrievals should return the modified value
    REQUIRE(pos1->get().x == new_x);
    REQUIRE(pos1->get().y == new_y);

    REQUIRE(positions_equal(pos1->get(), pos2->get()));
    REQUIRE(positions_equal(pos2->get(), pos3->get()));
}

/**
 * Property 11 Stress Test: Component retrieval idempotence at scale
 *
 * **Validates: Requirements 11.4**
 *
 * This test verifies idempotence holds with many entities and many retrievals.
 */
TEST_CASE("Property: component retrieval idempotence large scale", "[component_storage]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 1000000)));
    std::mt19937 gen(seed);

    const int NUM_ENTITIES = 1000;
    const int RETRIEVALS_PER_ENTITY = 10;

    EntityManager em;
    ComponentStorage storage;

    // Create many entities with components
    std::vector<Entity> entities;
    for (int i = 0; i < NUM_ENTITIES; ++i) {
        Entity e = em.create_entity();
        storage.add_component(e, Position{rand_float(gen), rand_float(gen)});
        entities.push_back(e);
    }

    // For each entity, retrieve component multiple times and verify idempotence
    for (Entity e : entities) {
        std::vector<Position> retrieved_positions;

        // Retrieve the component multiple times
        for (int i = 0; i < RETRIEVALS_PER_ENTITY; ++i) {
            auto pos = storage.get_component<Position>(e);
            REQUIRE(pos.has_value());
            retrieved_positions.push_back(pos->get());
        }

        // Verify all retrievals returned the same value
        for (int i = 1; i < RETRIEVALS_PER_ENTITY; ++i) {
            REQUIRE(positions_equal(retrieved_positions[0], retrieved_positions[i]));
        }
    }
}
