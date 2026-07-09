/**
 * Property-based tests for new component types and destruction pipeline
 *
 * These tests verify universal properties for the five new component types
 * (Rotation, Collider, Lifetime, WrapAround, DestroyRequest)
 * and the destroy_marked_entities() destruction pipeline.
 *
 * Testing Framework: Catch2 v3
 * Constants: NUM_OUTER_TESTS = 10, NUM_INNER_TESTS = 5
 *
 * Requirements tested: 1.1, 1.2, 2.1, 2.2, 2.3, 3.1, 4.1, 4.2, 5.1,
 *                      6.1, 6.2, 7.1, 8.2, 8.3, 8.4, 9.1, 9.2, 9.3,
 *                      12.5, 12.6, 12.7
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/destruction.hpp"
#include <random>
#include <vector>
#include <algorithm>

static constexpr int NUM_OUTER_TESTS = 10;
static constexpr int NUM_INNER_TESTS = 5;

// ============================================================================
// Property 1: Rotation component round-trip
// Feature: 050-01-components-and-destruction, Property 1: Rotation component round-trip
//
// **Validates: Requirements 1.1, 1.2, 12.5**
//
// For any entity and for any float values for angle and angular_velocity,
// adding a Rotation component and then retrieving it must return identical values.
// ============================================================================
TEST_CASE("Property: Rotation component round-trip", "[new_component][rotation]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 1000000)));
    std::mt19937 gen(seed);
    std::uniform_real_distribution<float> float_dist(-1000.0f, 1000.0f);

    for (int inner = 0; inner < NUM_INNER_TESTS; ++inner) {
        EntityManager em;
        ComponentStorage storage;

        float angle = float_dist(gen);
        float angular_velocity = float_dist(gen);

        Entity e = em.create_entity();
        storage.add_component(e, Rotation{angle, angular_velocity});

        auto retrieved = storage.get_component<Rotation>(e);
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->get().angle == angle);
        REQUIRE(retrieved->get().angular_velocity == angular_velocity);
    }
}

// ============================================================================
// Property 2: Collider component round-trip
// Feature: 050-01-components-and-destruction, Property 2: Collider component round-trip
//
// **Validates: Requirements 2.1, 2.2, 2.3, 12.6**
//
// For any entity and for any float width/height and uint8_t layer/mask,
// adding a Collider component and then retrieving it must return identical values.
// ============================================================================
TEST_CASE("Property: Collider component round-trip", "[new_component][collider]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 1000000)));
    std::mt19937 gen(seed);
    std::uniform_real_distribution<float> float_dist(-1000.0f, 1000.0f);
    std::uniform_int_distribution<int> byte_dist(0, 255);

    for (int inner = 0; inner < NUM_INNER_TESTS; ++inner) {
        EntityManager em;
        ComponentStorage storage;

        float width = float_dist(gen);
        float height = float_dist(gen);
        uint8_t layer = static_cast<uint8_t>(byte_dist(gen));
        uint8_t mask = static_cast<uint8_t>(byte_dist(gen));

        Entity e = em.create_entity();
        storage.add_component(e, Collider{width, height, layer, mask});

        auto retrieved = storage.get_component<Collider>(e);
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->get().width == width);
        REQUIRE(retrieved->get().height == height);
        REQUIRE(retrieved->get().layer == layer);
        REQUIRE(retrieved->get().mask == mask);
    }
}

// ============================================================================
// Property 3: Lifetime component round-trip
// Feature: 050-01-components-and-destruction, Property 3: Lifetime component round-trip
//
// **Validates: Requirements 3.1, 12.7**
//
// For any entity and for any float remaining value, adding a Lifetime component
// and then retrieving it must return an identical remaining value.
// ============================================================================
TEST_CASE("Property: Lifetime component round-trip", "[new_component][lifetime]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 1000000)));
    std::mt19937 gen(seed);
    std::uniform_real_distribution<float> float_dist(-1000.0f, 1000.0f);

    for (int inner = 0; inner < NUM_INNER_TESTS; ++inner) {
        EntityManager em;
        ComponentStorage storage;

        float remaining = float_dist(gen);

        Entity e = em.create_entity();
        storage.add_component(e, Lifetime{remaining});

        auto retrieved = storage.get_component<Lifetime>(e);
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->get().remaining == remaining);
    }
}

// ============================================================================
// Property 4: Tag component add-then-has round-trip
// Feature: 050-01-components-and-destruction, Property 4: Tag component add-then-has round-trip
//
// **Validates: Requirements 4.1, 4.2, 6.1, 6.2**
//
// For any entity, adding a WrapAround (or DestroyRequest) tag component and then
// calling has_component should return true. Removing the tag and calling
// has_component should return false.
// ============================================================================
TEST_CASE("Property: Tag component add-then-has round-trip", "[new_component][tag]") {
    auto _iter = GENERATE(take(NUM_OUTER_TESTS, random(0, 1)));
    (void)_iter;

    EntityManager em;
    ComponentStorage storage;

    Entity e = em.create_entity();

    // WrapAround: add -> has == true, remove -> has == false
    REQUIRE_FALSE(storage.has_component<WrapAround>(e));
    storage.add_component(e, WrapAround{});
    REQUIRE(storage.has_component<WrapAround>(e));
    storage.remove_component<WrapAround>(e);
    REQUIRE_FALSE(storage.has_component<WrapAround>(e));

    // DestroyRequest: add -> has == true, remove -> has == false
    REQUIRE_FALSE(storage.has_component<DestroyRequest>(e));
    storage.add_component(e, DestroyRequest{});
    REQUIRE(storage.has_component<DestroyRequest>(e));
    storage.remove_component<DestroyRequest>(e);
    REQUIRE_FALSE(storage.has_component<DestroyRequest>(e));
}

// ============================================================================
// Property 6: Destruction pipeline cleans up marked entities completely
// Feature: 050-01-components-and-destruction, Property 6: Destruction pipeline cleans up marked entities completely
//
// **Validates: Requirements 8.2, 8.3, 8.4**
//
// For any set of entities where some subset is marked with DestroyRequest,
// after calling destroy_marked_entities(), every previously-marked entity should
// satisfy: is_alive() returns false, has_component<T>() returns false for all
// 13 component types, and entities_with_component<DestroyRequest>() returns empty.
// ============================================================================
TEST_CASE("Property: Destruction pipeline cleans up marked entities completely", "[new_component][destruction]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 1000000)));
    std::mt19937 gen(seed);
    std::uniform_real_distribution<float> float_dist(-1000.0f, 1000.0f);
    std::uniform_int_distribution<int> byte_dist(0, 255);
    std::uniform_int_distribution<int> coin(0, 1);

    EntityManager em;
    ComponentStorage storage;

    std::vector<Entity> all_entities;
    std::vector<Entity> marked_entities;

    // Create NUM_INNER_TESTS entities with random component combinations
    for (int i = 0; i < NUM_INNER_TESTS; ++i) {
        Entity e = em.create_entity();
        all_entities.push_back(e);

        // Randomly add a subset of components
        if (coin(gen)) storage.add_component(e, Position{float_dist(gen), float_dist(gen)});
        if (coin(gen)) storage.add_component(e, Size{float_dist(gen), float_dist(gen)});
        if (coin(gen)) storage.add_component(e, Color{
            static_cast<uint8_t>(byte_dist(gen)),
            static_cast<uint8_t>(byte_dist(gen)),
            static_cast<uint8_t>(byte_dist(gen)),
            static_cast<uint8_t>(byte_dist(gen))});
        if (coin(gen)) storage.add_component(e, Rotation{float_dist(gen), float_dist(gen)});
        if (coin(gen)) storage.add_component(e, Collider{float_dist(gen), float_dist(gen),
            static_cast<uint8_t>(byte_dist(gen)),
            static_cast<uint8_t>(byte_dist(gen))});
        if (coin(gen)) storage.add_component(e, Lifetime{float_dist(gen)});
        if (coin(gen)) storage.add_component(e, WrapAround{});
    }

    // Mark a random subset with DestroyRequest
    for (Entity e : all_entities) {
        if (coin(gen)) {
            storage.add_component(e, DestroyRequest{});
            marked_entities.push_back(e);
        }
    }

    // Run destruction pipeline
    destroy_marked_entities(em, storage);

    // Verify: all marked entities are NOT alive and have no components
    for (Entity e : marked_entities) {
        REQUIRE_FALSE(em.is_alive(e));
        REQUIRE_FALSE(storage.has_component<Position>(e));
        REQUIRE_FALSE(storage.has_component<Size>(e));
        REQUIRE_FALSE(storage.has_component<Color>(e));
        REQUIRE_FALSE(storage.has_component<Input>(e));
        REQUIRE_FALSE(storage.has_component<Velocity>(e));
        REQUIRE_FALSE(storage.has_component<Images>(e));
        REQUIRE_FALSE(storage.has_component<Text>(e));
        REQUIRE_FALSE(storage.has_component<ScreenPosition>(e));
        REQUIRE_FALSE(storage.has_component<Rotation>(e));
        REQUIRE_FALSE(storage.has_component<Collider>(e));
        REQUIRE_FALSE(storage.has_component<Lifetime>(e));
        REQUIRE_FALSE(storage.has_component<WrapAround>(e));
        REQUIRE_FALSE(storage.has_component<DestroyRequest>(e));
    }

    // Verify: entities_with_component<DestroyRequest>() returns empty
    auto remaining = storage.entities_with_component<DestroyRequest>();
    REQUIRE(remaining.empty());
}

// ============================================================================
// Property 7: Destruction pipeline preserves unmarked entities
// Feature: 050-01-components-and-destruction, Property 7: Destruction pipeline preserves unmarked entities
//
// **Validates: Requirements 9.1, 9.2, 9.3**
//
// For any set of entities where some subset is marked with DestroyRequest and
// the rest are not, after calling destroy_marked_entities(), every unmarked
// entity should retain all its original components with unchanged values, and
// is_alive() should still return true.
// ============================================================================
TEST_CASE("Property: Destruction pipeline preserves unmarked entities", "[new_component][destruction]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 1000000)));
    std::mt19937 gen(seed);
    std::uniform_real_distribution<float> float_dist(-1000.0f, 1000.0f);
    std::uniform_int_distribution<int> byte_dist(0, 255);
    std::uniform_int_distribution<int> coin(0, 1);

    EntityManager em;
    ComponentStorage storage;

    // Saved component values for unmarked entities
    struct SavedComponents {
        Entity entity;
        bool has_position; Position position;
        bool has_size; Size size;
        bool has_color; Color color;
        bool has_rotation; Rotation rotation;
        bool has_collider; Collider collider;
        bool has_lifetime; Lifetime lifetime;
        bool has_wrap;
    };

    std::vector<SavedComponents> unmarked_saved;
    std::vector<Entity> all_entities;

    // Create NUM_INNER_TESTS entities with random component combinations
    for (int i = 0; i < NUM_INNER_TESTS; ++i) {
        Entity e = em.create_entity();
        all_entities.push_back(e);

        SavedComponents saved{};
        saved.entity = e;

        if (coin(gen)) {
            Position p{float_dist(gen), float_dist(gen)};
            storage.add_component(e, p);
            saved.has_position = true; saved.position = p;
        }
        if (coin(gen)) {
            Size s{float_dist(gen), float_dist(gen)};
            storage.add_component(e, s);
            saved.has_size = true; saved.size = s;
        }
        if (coin(gen)) {
            Color c{
                static_cast<uint8_t>(byte_dist(gen)),
                static_cast<uint8_t>(byte_dist(gen)),
                static_cast<uint8_t>(byte_dist(gen)),
                static_cast<uint8_t>(byte_dist(gen))};
            storage.add_component(e, c);
            saved.has_color = true; saved.color = c;
        }
        if (coin(gen)) {
            Rotation r{float_dist(gen), float_dist(gen)};
            storage.add_component(e, r);
            saved.has_rotation = true; saved.rotation = r;
        }
        if (coin(gen)) {
            Collider col{float_dist(gen), float_dist(gen),
                static_cast<uint8_t>(byte_dist(gen)),
                static_cast<uint8_t>(byte_dist(gen))};
            storage.add_component(e, col);
            saved.has_collider = true; saved.collider = col;
        }
        if (coin(gen)) {
            Lifetime lt{float_dist(gen)};
            storage.add_component(e, lt);
            saved.has_lifetime = true; saved.lifetime = lt;
        }
        if (coin(gen)) {
            storage.add_component(e, WrapAround{});
            saved.has_wrap = true;
        }

        // Decide if this entity is marked or unmarked
        if (coin(gen)) {
            storage.add_component(e, DestroyRequest{});
            // Don't save — this one will be destroyed
        } else {
            unmarked_saved.push_back(saved);
        }
    }

    // Run destruction pipeline
    destroy_marked_entities(em, storage);

    // Verify: unmarked entities are still alive and retain all original values
    for (const auto& saved : unmarked_saved) {
        Entity e = saved.entity;

        REQUIRE(em.is_alive(e));

        // Position
        if (saved.has_position) {
            auto p = storage.get_component<Position>(e);
            REQUIRE(p.has_value());
            REQUIRE(p->get().x == saved.position.x);
            REQUIRE(p->get().y == saved.position.y);
        } else {
            REQUIRE_FALSE(storage.has_component<Position>(e));
        }

        // Size
        if (saved.has_size) {
            auto s = storage.get_component<Size>(e);
            REQUIRE(s.has_value());
            REQUIRE(s->get().width == saved.size.width);
            REQUIRE(s->get().height == saved.size.height);
        } else {
            REQUIRE_FALSE(storage.has_component<Size>(e));
        }

        // Color
        if (saved.has_color) {
            auto c = storage.get_component<Color>(e);
            REQUIRE(c.has_value());
            REQUIRE(c->get().r == saved.color.r);
            REQUIRE(c->get().g == saved.color.g);
            REQUIRE(c->get().b == saved.color.b);
            REQUIRE(c->get().a == saved.color.a);
        } else {
            REQUIRE_FALSE(storage.has_component<Color>(e));
        }

        // Rotation
        if (saved.has_rotation) {
            auto r = storage.get_component<Rotation>(e);
            REQUIRE(r.has_value());
            REQUIRE(r->get().angle == saved.rotation.angle);
            REQUIRE(r->get().angular_velocity == saved.rotation.angular_velocity);
        } else {
            REQUIRE_FALSE(storage.has_component<Rotation>(e));
        }

        // Collider
        if (saved.has_collider) {
            auto col = storage.get_component<Collider>(e);
            REQUIRE(col.has_value());
            REQUIRE(col->get().width == saved.collider.width);
            REQUIRE(col->get().height == saved.collider.height);
            REQUIRE(col->get().layer == saved.collider.layer);
            REQUIRE(col->get().mask == saved.collider.mask);
        } else {
            REQUIRE_FALSE(storage.has_component<Collider>(e));
        }

        // Lifetime
        if (saved.has_lifetime) {
            auto lt = storage.get_component<Lifetime>(e);
            REQUIRE(lt.has_value());
            REQUIRE(lt->get().remaining == saved.lifetime.remaining);
        } else {
            REQUIRE_FALSE(storage.has_component<Lifetime>(e));
        }

        // WrapAround
        if (saved.has_wrap) {
            REQUIRE(storage.has_component<WrapAround>(e));
        } else {
            REQUIRE_FALSE(storage.has_component<WrapAround>(e));
        }
    }
}
