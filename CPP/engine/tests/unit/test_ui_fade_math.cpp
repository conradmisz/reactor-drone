/**
 * Unit tests for fade_overlay_alpha (ui_fade_math.hpp) — Option-040 Phase 6.
 *
 * The fade overlay alpha is a pure triangle curve: 0 at the endpoints, peaking
 * at FADE_MAX_ALPHA at the midpoint. Exercised headlessly (no SDL).
 */

#include <catch2/catch_test_macros.hpp>

#include "engine/ui_fade_math.hpp"

TEST_CASE("fade_overlay_alpha: endpoints are fully transparent", "[Engine][ui_fade][unit]") {
    CHECK(fade_overlay_alpha(0.0f) == 0);
    CHECK(fade_overlay_alpha(1.0f) == 0);
}

TEST_CASE("fade_overlay_alpha: midpoint reaches the peak", "[Engine][ui_fade][unit]") {
    CHECK(fade_overlay_alpha(0.5f) == FADE_MAX_ALPHA);
}

TEST_CASE("fade_overlay_alpha: quarter points are symmetric and half-peak", "[Engine][ui_fade][unit]") {
    int a25 = fade_overlay_alpha(0.25f);
    int a75 = fade_overlay_alpha(0.75f);
    CHECK(a25 == a75);                       // symmetric about 0.5
    CHECK(a25 == FADE_MAX_ALPHA / 2);        // tri = 0.5 -> 100
}

TEST_CASE("fade_overlay_alpha: out-of-range input is clamped", "[Engine][ui_fade][unit]") {
    CHECK(fade_overlay_alpha(-5.0f) == 0);   // clamps to 0
    CHECK(fade_overlay_alpha(5.0f) == 0);    // clamps to 1
    CHECK(fade_overlay_alpha(-0.001f) == 0);
}

TEST_CASE("fade_overlay_alpha: always within [0, FADE_MAX_ALPHA]", "[Engine][ui_fade][unit]") {
    for (float p = -0.5f; p <= 1.5f; p += 0.05f) {
        int a = fade_overlay_alpha(p);
        CHECK(a >= 0);
        CHECK(a <= FADE_MAX_ALPHA);
    }
}
