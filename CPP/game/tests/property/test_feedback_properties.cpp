/**
 * Property-based tests for feedback:: — screen-shake and hit-flash math (v2).
 *
 * Property F1: shake_amplitude is monotone non-decreasing in trauma and always
 *              lands in [0, max_px], for any trauma (including out-of-range).
 * Property F2: decay_trauma never returns a negative value and never increases
 *              trauma, for any non-negative dt and decay rate.
 * Property F3: flash_tint yields channels in [0,255] for arbitrary
 *              time_left/duration, is always opaque (alpha 255) and never
 *              additive, and fades monotonically toward the identity value 255.
 *
 * Each property uses a single Catch2 GENERATE and drives the rest of the space
 * with plain internal loops — nesting GENERATE (especially inside a loop)
 * multiplies section replays and makes the suite pathologically slow.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>

#include <algorithm>

#include "game/feedback.hpp"

// Configurable test iteration counts (MANDATORY — workspace policy)
constexpr int NUM_OUTER_TESTS = 10;
constexpr int NUM_INNER_TESTS = 5;

// ===========================================================================
// Property F1: shake_amplitude monotone in trauma, bounded by [0, max_px]
// ===========================================================================

TEST_CASE("feedback::shake_amplitude is monotone in trauma and bounded",
          "[Game][feedback][property]") {
    // One generator: the configured maximum. Trauma pairs are swept internally.
    auto max_px = GENERATE(take(NUM_OUTER_TESTS, random(0.0f, 64.0f)));

    // Sample trauma well outside [0,1] so the clamp is exercised too.
    for (int i = 0; i <= NUM_INNER_TESTS; ++i) {
        for (int j = 0; j <= NUM_INNER_TESTS; ++j) {
            float a = -2.0f + 5.0f * (static_cast<float>(i) / NUM_INNER_TESTS);
            float b = -2.0f + 5.0f * (static_cast<float>(j) / NUM_INNER_TESTS);
            float lo = std::min(a, b), hi = std::max(a, b);

            float amp_lo = feedback::shake_amplitude(lo, max_px);
            float amp_hi = feedback::shake_amplitude(hi, max_px);

            CHECK(amp_lo <= amp_hi);      // monotone in trauma
            CHECK(amp_lo >= 0.0f);        // never negative
            CHECK(amp_hi <= max_px);      // never exceeds the configured maximum
        }
    }
}

// ===========================================================================
// Property F2: decay_trauma floors at zero and never increases trauma
// ===========================================================================

TEST_CASE("feedback::decay_trauma floors at zero and never increases trauma",
          "[Game][feedback][property]") {
    auto trauma = GENERATE(take(NUM_OUTER_TESTS, random(0.0f, 1.0f)));

    for (int i = 0; i <= NUM_INNER_TESTS; ++i) {
        for (int j = 0; j <= NUM_INNER_TESTS; ++j) {
            float dt   = 5.0f  * (static_cast<float>(i) / NUM_INNER_TESTS);
            float rate = 10.0f * (static_cast<float>(j) / NUM_INNER_TESTS);

            float out = feedback::decay_trauma(trauma, dt, rate);

            CHECK(out >= 0.0f);
            CHECK(out <= trauma);
        }
    }
}

// ===========================================================================
// Property F3: flash_tint valid, opaque, non-additive, fades toward identity
// ===========================================================================

TEST_CASE("feedback::flash_tint produces a valid, opaque, non-additive tint",
          "[Game][feedback][property]") {
    auto channel = GENERATE(take(NUM_OUTER_TESTS, random(0, 255)));
    const uint8_t c = static_cast<uint8_t>(channel);

    for (int i = 0; i <= NUM_INNER_TESTS; ++i) {
        // Sweep time_left across (and past) the flash lifetime, incl. negatives.
        float duration = 0.12f;
        float time_left = -0.05f + duration * 1.4f * (static_cast<float>(i) / NUM_INNER_TESTS);

        Tint t = feedback::flash_tint(Flash{time_left, duration, c, c, c});

        // Channel is bracketed by the flash colour and the identity value 255.
        CHECK(t.r >= std::min<int>(c, 255));
        CHECK(t.r <= 255);
        CHECK(t.g == t.r);            // equal channels stay equal
        CHECK(t.b == t.r);

        // Never transparent (the entity must never blink out) and never additive.
        CHECK(t.a == 255);
        CHECK(t.additive == false);
    }

    // Less time left => closer to identity (255) => larger channel value.
    Tint early = feedback::flash_tint(Flash{0.12f, 0.12f, c, c, c});
    Tint late  = feedback::flash_tint(Flash{0.03f, 0.12f, c, c, c});
    Tint gone  = feedback::flash_tint(Flash{0.0f,  0.12f, c, c, c});
    CHECK(early.g <= late.g);
    CHECK(late.g <= gone.g);
    CHECK(gone.g == 255);
}
