/**
 * test_kit_visuals.cpp — the upgrade kit (D133) and the shield field (D134).
 *
 * What fails silently without these:
 *   - a kit part mapped to the wrong shop row, so buying Overclock bolts on the
 *     armour plating (the rows are indices into ShipState.upg_counts, and index
 *     1 is a HOLE because the Shield Capacitor has no static part);
 *   - a frame index off the end of the shield strip, which is a garbage tile
 *     out of the atlas rather than a crash — invisible in a unit test that only
 *     checks "some int came back";
 *   - the frame constants drifting from shield_frames() in make_sprites.py:
 *     the strip is 21 frames and NOTHING at runtime re-reads that;
 *   - the hit window keying off the wrong end of shield_delay, so the bloom
 *     plays for the whole regen wait instead of a third of a second.
 */
#include <catch2/catch_test_macros.hpp>

#include "game/player_components.hpp"
#include "game/upgrade_visuals.hpp"

using namespace upgrade_visuals;

namespace {
constexpr float DELAY_TOTAL = 5.0f;   // GameData shop.shield_regen_delay

ShipState with_shield(float shield, float max, float delay) {
    ShipState s{};
    s.shield = shield;
    s.shield_max = max;
    s.shield_delay = delay;
    return s;
}
}  // namespace

TEST_CASE("kit parts map onto the shop rows they belong to", "[kit][visuals]") {
    // The catalogue order in GameData.json: 0 Hull Plating, 1 Shield Capacitor,
    // 2 Aux Thruster, 3 Overclock, 4 Heavy Rounds, 5 Twin Barrel, 6 Long Barrel,
    // 7 Ricochet Coils.
    REQUIRE(KIT_COUNT == 7);

    SECTION("every part points at a distinct, valid row, and never at row 1") {
        bool seen[8] = {false};
        for (int i = 0; i < KIT_COUNT; ++i) {
            const int row = KIT[i].row;
            REQUIRE(row >= 0);
            REQUIRE(row < 8);
            REQUIRE(row != 1);          // Shield Capacitor is the field, not a part
            REQUIRE_FALSE(seen[row]);   // no two parts share a row
            seen[row] = true;
            REQUIRE(KIT[i].image != nullptr);
        }
    }

    SECTION("a part shows only once its own row is bought") {
        for (int i = 0; i < KIT_COUNT; ++i) {
            ShipState s{};
            REQUIRE_FALSE(part_worn(s, i));
            s.upg_counts[KIT[i].row] = 1;
            REQUIRE(part_worn(s, i));
            // ...and buying it did not light up anything else
            for (int j = 0; j < KIT_COUNT; ++j)
                if (j != i) REQUIRE_FALSE(part_worn(s, j));
        }
    }

    SECTION("buying the Shield Capacitor bolts nothing on") {
        ShipState s{};
        s.upg_counts[1] = 3;
        for (int i = 0; i < KIT_COUNT; ++i) REQUIRE_FALSE(part_worn(s, i));
    }

    SECTION("out-of-range part indices are false, not undefined") {
        ShipState s{};
        for (int& c : s.upg_counts) c = 5;
        REQUIRE_FALSE(part_worn(s, -1));
        REQUIRE_FALSE(part_worn(s, KIT_COUNT));
        REQUIRE_FALSE(part_worn(s, 9999));
    }
}

TEST_CASE("the field reads its state off ShipState alone", "[kit][shield]") {
    SECTION("no capacitor bought means no field at all") {
        ShipState s = with_shield(0.0f, 0.0f, 0.0f);
        REQUIRE(field_state(s, DELAY_TOTAL) == FieldState::Hidden);
    }

    SECTION("a full, quiet shield hums") {
        ShipState s = with_shield(30.0f, 30.0f, 0.0f);
        REQUIRE(field_state(s, DELAY_TOTAL) == FieldState::Hum);
    }

    SECTION("a fresh hit blooms, and only briefly") {
        // PlayerDamageSystem sets shield_delay to the full delay on every hit,
        // and tick_shields counts it down — so "just hit" is a delay near the top.
        ShipState s = with_shield(20.0f, 30.0f, DELAY_TOTAL);
        REQUIRE(field_state(s, DELAY_TOTAL) == FieldState::Hit);

        s.shield_delay = DELAY_TOTAL - FIELD_HIT_TIME * 0.5f;   // mid-bloom
        REQUIRE(field_state(s, DELAY_TOTAL) == FieldState::Hit);

        s.shield_delay = DELAY_TOTAL - FIELD_HIT_TIME - 0.01f;  // bloom over
        REQUIRE(field_state(s, DELAY_TOTAL) != FieldState::Hit);
    }

    SECTION("a drained shield is down, even mid-bloom-window") {
        ShipState s = with_shield(0.0f, 30.0f, 0.0f);
        REQUIRE(field_state(s, DELAY_TOTAL) == FieldState::Down);
    }

    SECTION("partial and past the quiet spell is a rebuild") {
        ShipState s = with_shield(12.0f, 30.0f, 0.0f);
        REQUIRE(field_state(s, DELAY_TOTAL) == FieldState::Regen);
    }

    SECTION("partial but still waiting out the delay is not a rebuild yet") {
        ShipState s = with_shield(12.0f, 30.0f, 1.0f);   // waiting, not just hit
        REQUIRE(field_state(s, DELAY_TOTAL) == FieldState::Hum);
    }
}

TEST_CASE("every field frame lands inside the strip", "[kit][shield]") {
    // The strip make_sprites.py writes is 21 frames. An index past it is a
    // garbage tile, not a crash, so this is the check that catches drift.
    const FieldState states[] = {FieldState::Hidden, FieldState::Hum,
                                 FieldState::Hit, FieldState::Down,
                                 FieldState::Regen};
    for (FieldState st : states) {
        for (int p = -20; p <= 40; ++p) {          // phase far outside 0..1
            for (int fr = -5; fr <= 15; ++fr) {    // fraction far outside 0..1
                const int f = field_frame(st, fr / 10.0f, p / 10.0f);
                REQUIRE(f >= 0);
                REQUIRE(f < FIELD_TOTAL);
            }
        }
    }

    SECTION("the constants still describe a contiguous 21-frame strip") {
        REQUIRE(FIELD_HUM_START == 0);
        REQUIRE(FIELD_HUM_START + FIELD_HUM_COUNT == FIELD_HIT_START);
        REQUIRE(FIELD_HIT_START + FIELD_HIT_COUNT == FIELD_DOWN_FRAME);
        REQUIRE(FIELD_DOWN_FRAME + 1 == FIELD_REGEN_START);
        REQUIRE(FIELD_REGEN_START + FIELD_REGEN_COUNT == FIELD_TOTAL);
    }

    SECTION("regen is indexed by fraction, and reaches both ends") {
        REQUIRE(field_frame(FieldState::Regen, 0.0f, 0.0f) == FIELD_REGEN_START);
        REQUIRE(field_frame(FieldState::Regen, 1.0f, 0.0f) ==
                FIELD_REGEN_START + FIELD_REGEN_COUNT - 1);
        // monotone: a fuller shield never draws an earlier rebuild frame
        int prev = -1;
        for (int i = 0; i <= 10; ++i) {
            const int f = field_frame(FieldState::Regen, i / 10.0f, 0.0f);
            REQUIRE(f >= prev);
            prev = f;
        }
    }

    SECTION("the hum walks the whole loop as the phase advances") {
        bool hit_first = false, hit_last = false;
        for (int i = 0; i < 100; ++i) {
            const int f = field_frame(FieldState::Hum, 1.0f, i / 100.0f);
            if (f == FIELD_HUM_START) hit_first = true;
            if (f == FIELD_HUM_START + FIELD_HUM_COUNT - 1) hit_last = true;
        }
        REQUIRE(hit_first);
        REQUIRE(hit_last);
    }
}
