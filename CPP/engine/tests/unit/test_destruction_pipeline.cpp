/**
 * Unit tests for the destruction pipeline
 *
 * These tests verify that destroy_marked_entities() correctly removes
 * marked entities and preserves unmarked entities, and that new component
 * types have correct default values.
 *
 * Requirements tested: 13.1, 13.2, 13.3, 13.4, 1.3, 2.4, 3.2
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/destruction.hpp"

using Catch::Approx;

/**
 * Test: Single entity marked with DestroyRequest is fully destroyed
 *
 * Verifies that after destroy_marked_entities(), the entity is no longer
 * alive and all its components have been removed.
 *
 * Validates: Requirement 13.1
 */
TEST_CASE("Single entity marked with DestroyRequest is fully destroyed", "[destruction_pipeline]") {
    EntityManager em;
    ComponentStorage storage;

    Entity entity = em.create_entity();
    storage.add_component(entity, Position{10.0f, 20.0f});
    storage.add_component(entity, Size{30.0f, 40.0f});
    storage.add_component(entity, DestroyRequest{});

    destroy_marked_entities(em, storage);

    CHECK_FALSE(em.is_alive(entity));
    CHECK_FALSE(storage.has_component<Position>(entity));
    CHECK_FALSE(storage.has_component<Size>(entity));
    CHECK_FALSE(storage.has_component<DestroyRequest>(entity));
}

/**
 * Test: Only marked entities are destroyed; unmarked entities are untouched
 *
 * Creates 3 entities, marks only e2 with DestroyRequest. After destruction,
 * e1 and e3 remain alive with their original component values, while e2 is gone.
 *
 * Validates: Requirement 13.2
 */
TEST_CASE("Only marked entities are destroyed; unmarked entities are untouched", "[destruction_pipeline]") {
    EntityManager em;
    ComponentStorage storage;

    Entity e1 = em.create_entity();
    Entity e2 = em.create_entity();
    Entity e3 = em.create_entity();

    storage.add_component(e1, Position{1.0f, 1.0f});
    storage.add_component(e1, Size{10.0f, 10.0f});

    storage.add_component(e2, Position{2.0f, 2.0f});
    storage.add_component(e2, Size{20.0f, 20.0f});
    storage.add_component(e2, DestroyRequest{});

    storage.add_component(e3, Position{3.0f, 3.0f});
    storage.add_component(e3, Size{30.0f, 30.0f});

    destroy_marked_entities(em, storage);

    // e1 alive and unchanged
    CHECK(em.is_alive(e1));
    auto p1 = storage.get_component<Position>(e1);
    REQUIRE(p1.has_value());
    CHECK(p1->get().x == Approx(1.0f));
    CHECK(p1->get().y == Approx(1.0f));
    auto s1 = storage.get_component<Size>(e1);
    REQUIRE(s1.has_value());
    CHECK(s1->get().width == Approx(10.0f));
    CHECK(s1->get().height == Approx(10.0f));

    // e2 destroyed
    CHECK_FALSE(em.is_alive(e2));
    CHECK_FALSE(storage.has_component<Position>(e2));
    CHECK_FALSE(storage.has_component<Size>(e2));
    CHECK_FALSE(storage.has_component<DestroyRequest>(e2));

    // e3 alive and unchanged
    CHECK(em.is_alive(e3));
    auto p3 = storage.get_component<Position>(e3);
    REQUIRE(p3.has_value());
    CHECK(p3->get().x == Approx(3.0f));
    CHECK(p3->get().y == Approx(3.0f));
    auto s3 = storage.get_component<Size>(e3);
    REQUIRE(s3.has_value());
    CHECK(s3->get().width == Approx(30.0f));
    CHECK(s3->get().height == Approx(30.0f));
}

/**
 * Test: Zero marked entities leaves everything unchanged
 *
 * Creates 3 entities with Position+Size, none marked with DestroyRequest.
 * After destruction, all 3 remain alive with unchanged components.
 *
 * Validates: Requirement 13.3
 */
TEST_CASE("Zero marked entities leaves everything unchanged", "[destruction_pipeline]") {
    EntityManager em;
    ComponentStorage storage;

    Entity e1 = em.create_entity();
    Entity e2 = em.create_entity();
    Entity e3 = em.create_entity();

    storage.add_component(e1, Position{10.0f, 10.0f});
    storage.add_component(e1, Size{100.0f, 100.0f});

    storage.add_component(e2, Position{20.0f, 20.0f});
    storage.add_component(e2, Size{200.0f, 200.0f});

    storage.add_component(e3, Position{30.0f, 30.0f});
    storage.add_component(e3, Size{300.0f, 300.0f});

    destroy_marked_entities(em, storage);

    // All three alive
    CHECK(em.is_alive(e1));
    CHECK(em.is_alive(e2));
    CHECK(em.is_alive(e3));

    // All components unchanged
    auto p1 = storage.get_component<Position>(e1);
    REQUIRE(p1.has_value());
    CHECK(p1->get().x == Approx(10.0f));
    CHECK(p1->get().y == Approx(10.0f));

    auto p2 = storage.get_component<Position>(e2);
    REQUIRE(p2.has_value());
    CHECK(p2->get().x == Approx(20.0f));
    CHECK(p2->get().y == Approx(20.0f));

    auto p3 = storage.get_component<Position>(e3);
    REQUIRE(p3.has_value());
    CHECK(p3->get().x == Approx(30.0f));
    CHECK(p3->get().y == Approx(30.0f));
}

/**
 * Test: Entity with all 15 component types has every one removed
 *
 * Creates one entity with all 15 component types plus DestroyRequest.
 * After destruction, the entity is dead and has_component returns false
 * for every type.
 *
 * Validates: Requirement 13.4
 */
TEST_CASE("Entity with all 15 component types has every one removed", "[destruction_pipeline]") {
    EntityManager em;
    ComponentStorage storage;

    Entity entity = em.create_entity();

    storage.add_component(entity, Position{1.0f, 2.0f});
    storage.add_component(entity, Size{3.0f, 4.0f});
    storage.add_component(entity, Color{255, 128, 0, 255});
    storage.add_component(entity, Input{});
    storage.add_component(entity, Velocity{5.0f, 6.0f});
    storage.add_component(entity, Images{{"test.png"}, 0});
    storage.add_component(entity, Text{"hello", "default.ttf", 24.0f, {255, 255, 255, 255}});
    storage.add_component(entity, ScreenPosition{7.0f, 8.0f});
    storage.add_component(entity, Rotation{0.5f, 1.0f});
    storage.add_component(entity, Collider{10.0f, 20.0f, 1, 2});
    storage.add_component(entity, CircleCollider{5.0f, 0.0f, 0.0f});
    storage.add_component(entity, OBBCollider{10.0f, 5.0f});
    storage.add_component(entity, Lifetime{5.0f});
    storage.add_component(entity, WrapAround{});
    storage.add_component(entity, DestroyRequest{});

    destroy_marked_entities(em, storage);

    CHECK_FALSE(em.is_alive(entity));
    CHECK_FALSE(storage.has_component<Position>(entity));
    CHECK_FALSE(storage.has_component<Size>(entity));
    CHECK_FALSE(storage.has_component<Color>(entity));
    CHECK_FALSE(storage.has_component<Input>(entity));
    CHECK_FALSE(storage.has_component<Velocity>(entity));
    CHECK_FALSE(storage.has_component<Images>(entity));
    CHECK_FALSE(storage.has_component<Text>(entity));
    CHECK_FALSE(storage.has_component<ScreenPosition>(entity));
    CHECK_FALSE(storage.has_component<Rotation>(entity));
    CHECK_FALSE(storage.has_component<Collider>(entity));
    CHECK_FALSE(storage.has_component<CircleCollider>(entity));
    CHECK_FALSE(storage.has_component<OBBCollider>(entity));
    CHECK_FALSE(storage.has_component<Lifetime>(entity));
    CHECK_FALSE(storage.has_component<WrapAround>(entity));
    CHECK_FALSE(storage.has_component<DestroyRequest>(entity));
}

/**
 * Test: Default-constructed Rotation has zeroed fields
 *
 * Validates: Requirement 1.3
 */
TEST_CASE("Default-constructed Rotation has zeroed fields", "[destruction_pipeline]") {
    Rotation r{};
    CHECK(r.angle == 0.0f);
    CHECK(r.angular_velocity == 0.0f);
}

/**
 * Test: Default-constructed Collider has layer=0 and mask=0
 *
 * Validates: Requirement 2.4
 */
TEST_CASE("Default-constructed Collider has layer=0 and mask=0", "[destruction_pipeline]") {
    Collider c{};
    CHECK(c.layer == 0);
    CHECK(c.mask == 0);
}

/**
 * Test: Default-constructed Lifetime has remaining=0.0f
 *
 * Validates: Requirement 3.2
 */
TEST_CASE("Default-constructed Lifetime has remaining=0.0f", "[destruction_pipeline]") {
    Lifetime l{};
    CHECK(l.remaining == 0.0f);
}
