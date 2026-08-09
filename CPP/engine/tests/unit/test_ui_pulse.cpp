/**
 * test_ui_pulse.cpp — unit tests for the pulsing-widget alpha helpers.
 *
 * pulse_alpha_scale / apply_alpha_scale are the whole of the flashing-button
 * feature's logic: UIRenderSystem does nothing with a pulse except multiply the
 * resolved bg/text alpha by the scale. Testing them here covers the behaviour
 * without needing a window.
 *
 * The property that matters most for the rest of the engine is the identity
 * case: a widget that never opted in (pulse_hz == 0, the default) must render
 * byte-identically to one from before pulsing existed.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <limits>

#include "engine/ecs/systems/ui_render_math.hpp"

using Catch::Matchers::WithinAbs;

TEST_CASE("pulse_alpha_scale is exactly identity for a non-pulsing widget",
          "[Engine][ui][pulse]") {
    // The default UIElement::pulse_hz. Must be EXACTLY 1.0f, not approximately:
    // any other value would shift the rendered alpha of every existing widget.
    for (float t : {0.0f, 0.25f, 1.0f, 7.5f, 1000.0f}) {
        REQUIRE(pulse_alpha_scale(0.0f, t) == 1.0f);
    }

    SECTION("negative and non-finite rates are treated as 'no pulse'") {
        REQUIRE(pulse_alpha_scale(-1.0f, 3.0f) == 1.0f);
        REQUIRE(pulse_alpha_scale(std::numeric_limits<float>::quiet_NaN(), 3.0f) == 1.0f);
    }

    SECTION("a non-finite clock never produces a non-finite alpha") {
        const float inf = std::numeric_limits<float>::infinity();
        REQUIRE(pulse_alpha_scale(1.0f, inf) == 1.0f);
        REQUIRE(pulse_alpha_scale(1.0f, std::numeric_limits<float>::quiet_NaN()) == 1.0f);
    }
}

TEST_CASE("pulse_alpha_scale sweeps the full floor..1 range once per period",
          "[Engine][ui][pulse]") {
    const float hz = 2.0f;          // period 0.5s
    // Peak at t = 0 so a widget appears at full opacity the frame it is shown.
    REQUIRE_THAT(pulse_alpha_scale(hz, 0.0f), WithinAbs(1.0f, 1e-5f));
    // Trough at the half-period.
    REQUIRE_THAT(pulse_alpha_scale(hz, 0.25f), WithinAbs(UI_PULSE_FLOOR, 1e-5f));
    // Back to the peak at a full period, and at every whole multiple of it.
    REQUIRE_THAT(pulse_alpha_scale(hz, 0.5f), WithinAbs(1.0f, 1e-5f));
    REQUIRE_THAT(pulse_alpha_scale(hz, 5.0f), WithinAbs(1.0f, 1e-5f));

    SECTION("stays inside [floor, 1] for arbitrary times") {
        for (int i = 0; i < 200; ++i) {
            const float t = static_cast<float>(i) * 0.017f;
            const float s = pulse_alpha_scale(hz, t);
            REQUIRE(s >= UI_PULSE_FLOOR - 1e-5f);
            REQUIRE(s <= 1.0f + 1e-5f);
        }
    }

    SECTION("never fades a widget to invisible") {
        REQUIRE(UI_PULSE_FLOOR > 0.0f);
    }
}

TEST_CASE("apply_alpha_scale touches alpha only, and stays in 0..255",
          "[Engine][ui][pulse]") {
    const Color c{10, 200, 30, 200};

    SECTION("identity scale leaves the colour untouched") {
        const Color out = apply_alpha_scale(c, 1.0f);
        REQUIRE(out.r == c.r);
        REQUIRE(out.g == c.g);
        REQUIRE(out.b == c.b);
        REQUIRE(out.a == c.a);
    }

    SECTION("RGB is never modulated, only A") {
        const Color out = apply_alpha_scale(c, 0.5f);
        REQUIRE(out.r == c.r);
        REQUIRE(out.g == c.g);
        REQUIRE(out.b == c.b);
        REQUIRE(out.a == 100);          // 200 * 0.5, rounded
    }

    SECTION("a fully transparent colour cannot be pulsed into visibility") {
        const Color clear{1, 2, 3, 0};
        REQUIRE(apply_alpha_scale(clear, 1.0f).a == 0);
    }

    SECTION("out-of-range scales clamp instead of wrapping") {
        // A uint8_t cast of an unclamped 200*2 would wrap to 144 — visibly wrong.
        REQUIRE(apply_alpha_scale(c, 2.0f).a == 255);
        REQUIRE(apply_alpha_scale(c, -1.0f).a == 0);
    }
}
