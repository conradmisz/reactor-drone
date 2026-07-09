/**
 * Unit + property tests for the Tint component and modulate_color helper (v2).
 *
 * modulate_color(base, tint) multiplies per channel (base*tint/255) including
 * alpha. Verified: identity tint is a no-op, zero tint yields black/transparent,
 * and every output channel is bounded by its inputs (monotone, in range).
 * Pure math — no SDL required.
 */

#include <catch2/catch_test_macros.hpp>
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"

TEST_CASE("Tint default is identity", "[tint]") {
    Tint t;
    CHECK(t.r == 255);
    CHECK(t.g == 255);
    CHECK(t.b == 255);
    CHECK(t.a == 255);
    CHECK(t.additive == false);
}

TEST_CASE("modulate_color with identity tint returns base unchanged", "[tint]") {
    Tint identity;  // {255,255,255,255}
    for (int v : {0, 1, 63, 128, 200, 255}) {
        Color c{static_cast<uint8_t>(v), static_cast<uint8_t>(255 - v),
                static_cast<uint8_t>(v / 2), static_cast<uint8_t>(v)};
        Color out = modulate_color(c, identity);
        CHECK(out.r == c.r);
        CHECK(out.g == c.g);
        CHECK(out.b == c.b);
        CHECK(out.a == c.a);
    }
}

TEST_CASE("modulate_color with zero tint yields zero", "[tint]") {
    Tint zero{0, 0, 0, 0, false};
    Color c{200, 150, 100, 255};
    Color out = modulate_color(c, zero);
    CHECK(out.r == 0);
    CHECK(out.g == 0);
    CHECK(out.b == 0);
    CHECK(out.a == 0);
}

TEST_CASE("modulate_color endpoints: full tint channel preserves base channel", "[tint]") {
    // A channel tinted at 255 is unchanged; at 0 it is zeroed.
    Color c{123, 45, 210, 99};
    Tint t{255, 0, 255, 0, false};
    Color out = modulate_color(c, t);
    CHECK(out.r == c.r);   // r tint 255 -> unchanged
    CHECK(out.g == 0);     // g tint 0   -> zero
    CHECK(out.b == c.b);   // b tint 255 -> unchanged
    CHECK(out.a == 0);     // a tint 0   -> zero
}

TEST_CASE("modulate_color property: output bounded by min(base,tint), in [0,255]", "[tint]") {
    // Exhaustive over a stride so it runs fast but covers the channel space.
    for (int b = 0; b <= 255; b += 17) {
        for (int m = 0; m <= 255; m += 17) {
            Color base{static_cast<uint8_t>(b), static_cast<uint8_t>(b),
                       static_cast<uint8_t>(b), static_cast<uint8_t>(b)};
            Tint tint{static_cast<uint8_t>(m), static_cast<uint8_t>(m),
                      static_cast<uint8_t>(m), static_cast<uint8_t>(m), false};
            Color out = modulate_color(base, tint);
            int expected = (b * m) / 255;
            CHECK(out.r == expected);
            // never exceeds either input, always representable
            CHECK(out.r <= b);
            CHECK(out.r <= m);
            CHECK(out.r >= 0);
            CHECK(out.r <= 255);
        }
    }
}

TEST_CASE("modulate_color is monotone in the tint", "[tint]") {
    Color base{200, 200, 200, 200};
    Color lo = modulate_color(base, Tint{50, 50, 50, 50, false});
    Color hi = modulate_color(base, Tint{200, 200, 200, 200, false});
    CHECK(lo.r <= hi.r);
    CHECK(lo.a <= hi.a);
}

TEST_CASE("Tint round-trips through ComponentStorage", "[tint]") {
    ComponentStorage storage;
    EntityManager em;
    Entity e = em.create_entity();
    storage.add_component<Tint>(e, Tint{10, 20, 30, 40, true});
    REQUIRE(storage.has_component<Tint>(e));
    auto got = storage.get_component<Tint>(e);
    REQUIRE(got.has_value());
    CHECK(got->get().r == 10);
    CHECK(got->get().additive == true);
    storage.remove_component<Tint>(e);
    CHECK_FALSE(storage.has_component<Tint>(e));
}
