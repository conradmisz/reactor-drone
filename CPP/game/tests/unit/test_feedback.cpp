/**
 * Unit tests for feedback:: — the pure hit-feedback math behind screen shake
 * and hit flashes (v2, Phase 4).
 *
 * Screen shake is driven by a "trauma" scalar in [0,1] that producers add to on
 * impact and that decays each frame. Pixel amplitude is trauma-squared, so small
 * trauma is barely felt and large trauma is punchy. A Flash converts to a Tint
 * that interpolates from the flash colour back to the identity tint over the
 * flash's lifetime.
 *
 * All of this is pure — no game loop, no SDL, no Blackboard.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "game/feedback.hpp"

// ===========================================================================
// clamp_trauma
// ===========================================================================

TEST_CASE("feedback::clamp_trauma clamps into [0,1]", "[Game][feedback][unit]") {
    CHECK(feedback::clamp_trauma(-5.0f) == 0.0f);
    CHECK(feedback::clamp_trauma(0.0f) == 0.0f);
    CHECK(feedback::clamp_trauma(0.5f) == 0.5f);
    CHECK(feedback::clamp_trauma(1.0f) == 1.0f);
    CHECK(feedback::clamp_trauma(7.5f) == 1.0f);
}

// ===========================================================================
// add_trauma — producers accumulate impacts, result stays in [0,1]
// ===========================================================================

TEST_CASE("feedback::add_trauma accumulates and saturates at 1", "[Game][feedback][unit]") {
    CHECK(feedback::add_trauma(0.0f, 0.3f) == Catch::Approx(0.3f));
    CHECK(feedback::add_trauma(0.3f, 0.2f) == Catch::Approx(0.5f));

    // Repeated impacts saturate rather than overflowing the [0,1] contract.
    CHECK(feedback::add_trauma(0.9f, 0.5f) == 1.0f);
    CHECK(feedback::add_trauma(1.0f, 1.0f) == 1.0f);

    // A negative addend cannot drive trauma below the floor.
    CHECK(feedback::add_trauma(0.1f, -0.9f) == 0.0f);
}

// ===========================================================================
// shake_amplitude — quadratic response, clamped, monotone
// ===========================================================================

TEST_CASE("feedback::shake_amplitude endpoints", "[Game][feedback][unit]") {
    constexpr float kMax = 18.0f;
    CHECK(feedback::shake_amplitude(0.0f, kMax) == 0.0f);
    CHECK(feedback::shake_amplitude(1.0f, kMax) == kMax);

    // Quadratic: half trauma gives a quarter of the amplitude.
    CHECK(feedback::shake_amplitude(0.5f, kMax) == Catch::Approx(kMax * 0.25f));
}

TEST_CASE("feedback::shake_amplitude clamps out-of-range trauma", "[Game][feedback][unit]") {
    constexpr float kMax = 18.0f;
    CHECK(feedback::shake_amplitude(-3.0f, kMax) == 0.0f);
    CHECK(feedback::shake_amplitude(4.0f, kMax) == kMax);
}

TEST_CASE("feedback::shake_amplitude scales with the configured maximum", "[Game][feedback][unit]") {
    // The maximum is a GameData.json tunable, not a compile-time constant.
    CHECK(feedback::shake_amplitude(1.0f, 0.0f) == 0.0f);
    CHECK(feedback::shake_amplitude(1.0f, 42.0f) == 42.0f);
}

// ===========================================================================
// decay_trauma — linear bleed-off, floors at zero
// ===========================================================================

TEST_CASE("feedback::decay_trauma bleeds off linearly", "[Game][feedback][unit]") {
    // 1.0 trauma, 1.6/sec decay, quarter second -> 1.0 - 0.4 = 0.6
    CHECK(feedback::decay_trauma(1.0f, 0.25f, 1.6f) == Catch::Approx(0.6f));
}

TEST_CASE("feedback::decay_trauma floors at zero", "[Game][feedback][unit]") {
    CHECK(feedback::decay_trauma(0.1f, 1.0f, 1.6f) == 0.0f);
    CHECK(feedback::decay_trauma(0.0f, 1.0f, 1.6f) == 0.0f);

    // A long frame hitch must not swing trauma negative.
    CHECK(feedback::decay_trauma(0.5f, 100.0f, 1.6f) == 0.0f);
}

TEST_CASE("feedback::decay_trauma with zero dt is the identity", "[Game][feedback][unit]") {
    CHECK(feedback::decay_trauma(0.42f, 0.0f, 1.6f) == Catch::Approx(0.42f));
}

// ===========================================================================
// flash_tint — colour lerps back to the identity tint as the flash expires
//
// The tint must reach the *identity* tint {255,255,255,255} at expiry, not a
// transparent tint: alpha 0 would make the flashed entity vanish for a frame
// (SDL alpha-mod on the texture path, modulate_color on the colour-rect path)
// and then pop back when the Flash component is removed.
// ===========================================================================

TEST_CASE("feedback::flash_tint at full life is the flash colour", "[Game][feedback][unit]") {
    Flash f{0.12f, 0.12f, 255, 70, 70};
    Tint t = feedback::flash_tint(f);
    CHECK(t.r == 255);
    CHECK(t.g == 70);
    CHECK(t.b == 70);
    CHECK(t.a == 255);
    CHECK(t.additive == false);
}

TEST_CASE("feedback::flash_tint at expiry is the identity tint", "[Game][feedback][unit]") {
    Flash f{0.0f, 0.12f, 255, 70, 70};
    Tint t = feedback::flash_tint(f);
    CHECK(t.r == 255);
    CHECK(t.g == 255);
    CHECK(t.b == 255);
    CHECK(t.a == 255);  // never transparent — the entity must stay visible
}

TEST_CASE("feedback::flash_tint at half life is halfway to identity", "[Game][feedback][unit]") {
    Flash f{0.06f, 0.12f, 255, 0, 0};
    Tint t = feedback::flash_tint(f);
    CHECK(t.r == 255);          // 255 -> 255 regardless of frac
    CHECK(t.g == 128);          // 255 + (0 - 255) * 0.5, rounded
    CHECK(t.b == 128);
    CHECK(t.a == 255);
}

TEST_CASE("feedback::flash_tint with non-positive duration is the identity tint",
          "[Game][feedback][unit]") {
    // Guards the division; a zero-duration Flash is a no-op, not a crash.
    Flash f{1.0f, 0.0f, 10, 20, 30};
    Tint t = feedback::flash_tint(f);
    CHECK(t.r == 255);
    CHECK(t.g == 255);
    CHECK(t.b == 255);
    CHECK(t.a == 255);
}

TEST_CASE("feedback::flash_tint clamps time_left beyond duration", "[Game][feedback][unit]") {
    Flash over{5.0f, 0.12f, 10, 20, 30};
    Tint t = feedback::flash_tint(over);
    CHECK(t.r == 10);
    CHECK(t.g == 20);
    CHECK(t.b == 30);

    Flash under{-1.0f, 0.12f, 10, 20, 30};
    Tint u = feedback::flash_tint(under);
    CHECK(u.r == 255);
    CHECK(u.g == 255);
    CHECK(u.b == 255);
}

// ===========================================================================
// Flash storage round-trip (mirrors the Tint round-trip in test_tint.cpp)
// ===========================================================================

TEST_CASE("Flash round-trips through ComponentStorage", "[Game][feedback][unit]") {
    ComponentStorage storage;
    EntityManager em;
    Entity e = em.create_entity();

    storage.add_component<Flash>(e, Flash{0.08f, 0.12f, 255, 70, 70});
    REQUIRE(storage.has_component<Flash>(e));

    auto got = storage.get_component<Flash>(e);
    REQUIRE(got.has_value());
    CHECK(got->get().time_left == 0.08f);
    CHECK(got->get().duration == 0.12f);
    CHECK(got->get().r == 255);

    storage.remove_component<Flash>(e);
    CHECK_FALSE(storage.has_component<Flash>(e));
}
