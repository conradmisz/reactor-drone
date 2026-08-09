/**
 * Unit tests for next_focus_index (ui_focus_math.hpp) — Option-040 Phase 7.
 *
 * Pure cyclic index helper for Tab navigation; exercised headlessly (no SDL).
 */

#include <catch2/catch_test_macros.hpp>

#include "engine/ui_focus_math.hpp"

TEST_CASE("next_focus_index: empty list yields -1", "[Engine][ui_focus][unit]") {
    CHECK(next_focus_index(0, -1, true) == -1);
    CHECK(next_focus_index(0, 0, false) == -1);
    CHECK(next_focus_index(-3, 2, true) == -1);
}

TEST_CASE("next_focus_index: from no focus, forward picks first, backward picks last",
          "[Engine][ui_focus][unit]") {
    CHECK(next_focus_index(4, -1, true) == 0);
    CHECK(next_focus_index(4, -1, false) == 3);
}

TEST_CASE("next_focus_index: forward steps and wraps", "[Engine][ui_focus][unit]") {
    CHECK(next_focus_index(3, 0, true) == 1);
    CHECK(next_focus_index(3, 1, true) == 2);
    CHECK(next_focus_index(3, 2, true) == 0);  // wrap
}

TEST_CASE("next_focus_index: backward steps and wraps", "[Engine][ui_focus][unit]") {
    CHECK(next_focus_index(3, 2, false) == 1);
    CHECK(next_focus_index(3, 1, false) == 0);
    CHECK(next_focus_index(3, 0, false) == 2);  // wrap
}

TEST_CASE("next_focus_index: single element stays put", "[Engine][ui_focus][unit]") {
    CHECK(next_focus_index(1, 0, true) == 0);
    CHECK(next_focus_index(1, 0, false) == 0);
}
