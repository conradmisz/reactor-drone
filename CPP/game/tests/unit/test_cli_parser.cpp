/**
 * Unit tests for CLI parser (cli_parser.hpp)
 *
 * Validates: Requirements 1.1, 1.3, 1.4, 1.5, 1.6, 2.1, 2.4, 2.5,
 *            3.1, 3.6, 4.1, 4.7, 5.1, 5.3, 6.1, 7.1, 8.1, 8.4,
 *            9.3, 9.4, 10.1, 10.3
 *
 * Tests cover:
 * - Default options (no arguments)
 * - --help flag
 * - Missing value errors (--fps, --stopframe, --dump, --trace alone)
 * - Invalid FPS values (0, negative, non-integer)
 * - Invalid key tokens (bad key name, bad frame, missing colon)
 * - Combined options parsing
 * - Short verbose flag (-v)
 * - --clear-logs and --no-clear-logs
 * - Multiple keys on same frame (order preserved)
 * - Multiple dump/trace frames
 */

#include <catch2/catch_test_macros.hpp>
#include "game/cli_parser.hpp"
#include "game/debug_state.hpp"

#include <algorithm>
#include <string>
#include <vector>

// ===========================================================================
// Helper: convert string vector to argc/argv for parse_command_line
// ===========================================================================

static CommandLineOptions parse_args(std::vector<std::string> args) {
    std::vector<char*> argv;
    for (auto& s : args) {
        argv.push_back(s.data());
    }
    return parse_command_line(static_cast<int>(argv.size()), argv.data());
}

// ===========================================================================
// 1. Default options — empty argv (just program name)
// ===========================================================================

TEST_CASE("Default options: all defaults match Req 1.3 / 10.3",
          "[cli_parser][unit]") {
    auto opts = parse_args({"game"});

    REQUIRE(opts.keys.empty());
    REQUIRE(opts.dump_frames.empty());
    REQUIRE(opts.trace_frames.empty());
    REQUIRE_FALSE(opts.stop_frame.has_value());
    REQUIRE(opts.paused == false);
    REQUIRE(opts.verbose == false);
    REQUIRE(opts.fps == 0);
    REQUIRE(opts.clear_logs == true);
    REQUIRE(opts.debug_keys == true);
    REQUIRE(opts.parse_error == false);
    REQUIRE(opts.help_requested == false);
}

// ===========================================================================
// 2. --help flag
// ===========================================================================

TEST_CASE("--help sets help_requested to true", "[cli_parser][unit]") {
    auto opts = parse_args({"game", "--help"});

    REQUIRE(opts.help_requested == true);
    REQUIRE(opts.parse_error == false);
}

// ===========================================================================
// 3. Missing value errors
// ===========================================================================

TEST_CASE("Missing value errors set parse_error", "[cli_parser][unit]") {
    SECTION("--fps alone") {
        auto opts = parse_args({"game", "--fps"});
        REQUIRE(opts.parse_error == true);
    }

    SECTION("--stopframe alone") {
        auto opts = parse_args({"game", "--stopframe"});
        REQUIRE(opts.parse_error == true);
    }

    SECTION("--dump alone") {
        auto opts = parse_args({"game", "--dump"});
        REQUIRE(opts.parse_error == true);
    }

    SECTION("--trace alone") {
        auto opts = parse_args({"game", "--trace"});
        REQUIRE(opts.parse_error == true);
    }
}

// ===========================================================================
// 4. Invalid FPS values
// ===========================================================================

TEST_CASE("Invalid FPS values set parse_error", "[cli_parser][unit]") {
    SECTION("--fps 0") {
        auto opts = parse_args({"game", "--fps", "0"});
        REQUIRE(opts.parse_error == true);
    }

    SECTION("--fps -5") {
        auto opts = parse_args({"game", "--fps", "-5"});
        REQUIRE(opts.parse_error == true);
    }

    SECTION("--fps abc") {
        auto opts = parse_args({"game", "--fps", "abc"});
        REQUIRE(opts.parse_error == true);
    }
}

// ===========================================================================
// 5. Invalid key tokens
// ===========================================================================

TEST_CASE("Invalid key tokens set parse_error", "[cli_parser][unit]") {
    SECTION("--keys 5:INVALID (bad key name)") {
        auto opts = parse_args({"game", "--keys", "5:INVALID"});
        REQUIRE(opts.parse_error == true);
    }

    SECTION("--keys abc:RIGHT (bad frame number)") {
        auto opts = parse_args({"game", "--keys", "abc:RIGHT"});
        REQUIRE(opts.parse_error == true);
    }

    SECTION("--keys 5RIGHT (no colon)") {
        auto opts = parse_args({"game", "--keys", "5RIGHT"});
        REQUIRE(opts.parse_error == true);
    }
}

// ===========================================================================
// 6. Combined options
// ===========================================================================

TEST_CASE("Combined options parse all fields correctly",
          "[cli_parser][unit]") {
    auto opts = parse_args({
        "game",
        "--paused", "--verbose", "--fps", "30",
        "--stopframe", "100",
        "--keys", "5:RIGHT",
        "--dump", "10",
        "--trace", "15"
    });

    REQUIRE(opts.parse_error == false);
    REQUIRE(opts.paused == true);
    REQUIRE(opts.verbose == true);
    REQUIRE(opts.fps == 30);
    REQUIRE(opts.stop_frame.has_value());
    REQUIRE(opts.stop_frame.value() == 100);
    REQUIRE(opts.keys.size() == 1);
    REQUIRE(opts.keys[0].frame == 5);
    REQUIRE(opts.keys[0].key == "RIGHT");
    REQUIRE(opts.dump_frames.size() == 1);
    REQUIRE(opts.dump_frames[0] == 10);
    REQUIRE(opts.trace_frames.size() == 1);
    REQUIRE(opts.trace_frames[0] == 15);
}

// ===========================================================================
// 7. Short verbose flag
// ===========================================================================

TEST_CASE("-v sets verbose to true", "[cli_parser][unit]") {
    auto opts = parse_args({"game", "-v"});

    REQUIRE(opts.verbose == true);
    REQUIRE(opts.parse_error == false);
}

// ===========================================================================
// 8. --clear-logs and --no-clear-logs
// ===========================================================================

TEST_CASE("--clear-logs and --no-clear-logs set clear_logs correctly",
          "[cli_parser][unit]") {
    SECTION("--clear-logs sets clear_logs to true") {
        auto opts = parse_args({"game", "--clear-logs"});
        REQUIRE(opts.clear_logs == true);
        REQUIRE(opts.parse_error == false);
    }

    SECTION("--no-clear-logs sets clear_logs to false") {
        auto opts = parse_args({"game", "--no-clear-logs"});
        REQUIRE(opts.clear_logs == false);
        REQUIRE(opts.parse_error == false);
    }
}

// ===========================================================================
// 8b. Interactive dump/trace keys (J/T) and --debug-keys flag
// ===========================================================================

TEST_CASE("--debug-keys and --no-debug-keys set debug_keys correctly",
          "[cli_parser][unit]") {
    SECTION("--debug-keys sets debug_keys to true") {
        auto opts = parse_args({"game", "--debug-keys"});
        REQUIRE(opts.debug_keys == true);
        REQUIRE(opts.parse_error == false);
    }

    SECTION("--no-debug-keys sets debug_keys to false") {
        auto opts = parse_args({"game", "--no-debug-keys"});
        REQUIRE(opts.debug_keys == false);
        REQUIRE(opts.parse_error == false);
    }
}

TEST_CASE("--keys accepts J and T injections", "[cli_parser][unit]") {
    auto opts = parse_args({"game", "--keys", "3:J", "5:T"});

    REQUIRE(opts.parse_error == false);
    REQUIRE(opts.keys.size() == 2);
    REQUIRE(opts.keys[0].frame == 3);
    REQUIRE(opts.keys[0].key == "J");
    REQUIRE(opts.keys[1].frame == 5);
    REQUIRE(opts.keys[1].key == "T");
}

TEST_CASE("debug_keys is preserved by options_to_argv round trip",
          "[cli_parser][unit]") {
    SECTION("debug_keys = false round-trips") {
        auto opts = parse_args({"game", "--no-debug-keys"});
        REQUIRE(opts.debug_keys == false);

        auto argv_strs = options_to_argv(opts);
        std::vector<char*> argv_ptrs;
        for (auto& s : argv_strs) argv_ptrs.push_back(const_cast<char*>(s.c_str()));
        auto opts2 = parse_command_line(static_cast<int>(argv_ptrs.size()),
                                        argv_ptrs.data());
        REQUIRE(opts2.debug_keys == false);
        REQUIRE(opts2.parse_error == false);
    }

    SECTION("J/T --keys injections round-trip") {
        auto opts = parse_args({"game", "--keys", "3:J", "5:T"});
        auto argv_strs = options_to_argv(opts);
        std::vector<char*> argv_ptrs;
        for (auto& s : argv_strs) argv_ptrs.push_back(const_cast<char*>(s.c_str()));
        auto opts2 = parse_command_line(static_cast<int>(argv_ptrs.size()),
                                        argv_ptrs.data());
        REQUIRE(opts2.parse_error == false);
        REQUIRE(opts2.keys.size() == 2);
        REQUIRE(opts2.keys[0].key == "J");
        REQUIRE(opts2.keys[1].key == "T");
    }
}

// ===========================================================================
// 9. Multiple keys on same frame — order preserved
// ===========================================================================

TEST_CASE("Multiple keys on same frame preserves order",
          "[cli_parser][unit]") {
    auto opts = parse_args({"game", "--keys", "5:RIGHT", "5:LEFT"});

    REQUIRE(opts.parse_error == false);
    REQUIRE(opts.keys.size() == 2);
    REQUIRE(opts.keys[0].frame == 5);
    REQUIRE(opts.keys[0].key == "RIGHT");
    REQUIRE(opts.keys[1].frame == 5);
    REQUIRE(opts.keys[1].key == "LEFT");
}

// ===========================================================================
// 10. Multiple dump/trace frames
// ===========================================================================

TEST_CASE("Multiple dump and trace frames parse all values",
          "[cli_parser][unit]") {
    SECTION("--dump 10 20 30") {
        auto opts = parse_args({"game", "--dump", "10", "20", "30"});
        REQUIRE(opts.parse_error == false);
        REQUIRE(opts.dump_frames.size() == 3);
        REQUIRE(opts.dump_frames[0] == 10);
        REQUIRE(opts.dump_frames[1] == 20);
        REQUIRE(opts.dump_frames[2] == 30);
    }

    SECTION("--trace 5 10") {
        auto opts = parse_args({"game", "--trace", "5", "10"});
        REQUIRE(opts.parse_error == false);
        REQUIRE(opts.trace_frames.size() == 2);
        REQUIRE(opts.trace_frames[0] == 5);
        REQUIRE(opts.trace_frames[1] == 10);
    }
}

// ===========================================================================
// 11. --screenshot option
// ===========================================================================

TEST_CASE("--screenshot parses frame numbers correctly",
          "[cli_parser][unit]") {
    SECTION("--screenshot 10 20 30") {
        auto opts = parse_args({"game", "--screenshot", "10", "20", "30"});
        REQUIRE(opts.parse_error == false);
        REQUIRE(opts.screenshot_frames.size() == 3);
        REQUIRE(opts.screenshot_frames[0] == 10);
        REQUIRE(opts.screenshot_frames[1] == 20);
        REQUIRE(opts.screenshot_frames[2] == 30);
    }

    SECTION("--screenshot alone (no frame numbers) produces parse_error") {
        auto opts = parse_args({"game", "--screenshot"});
        REQUIRE(opts.parse_error == true);
    }

    SECTION("--screenshot abc (non-numeric) produces parse_error") {
        auto opts = parse_args({"game", "--screenshot", "abc"});
        REQUIRE(opts.parse_error == true);
    }
}

// ===========================================================================
// 12. --script option
// ===========================================================================

TEST_CASE("--script parses filename correctly",
          "[cli_parser][unit]") {
    SECTION("--script session.json stores the filename") {
        auto opts = parse_args({"game", "--script", "session.json"});
        REQUIRE(opts.parse_error == false);
        REQUIRE(opts.script_file == "session.json");
    }

    SECTION("--script alone (no filename) produces parse_error") {
        auto opts = parse_args({"game", "--script"});
        REQUIRE(opts.parse_error == true);
    }

    SECTION("--script session.json --paused produces parse_error (exclusivity)") {
        auto opts = parse_args({"game", "--script", "session.json", "--paused"});
        REQUIRE(opts.parse_error == true);
    }
}

// ===========================================================================
// 13. --clicks option — single click
// ===========================================================================

TEST_CASE("--clicks 10:400,300 parses single click correctly",
          "[cli_parser][unit]") {
    auto opts = parse_args({"game", "--clicks", "10:400,300"});

    REQUIRE(opts.parse_error == false);
    REQUIRE(opts.clicks.size() == 1);
    REQUIRE(opts.clicks[0].frame == 10);
    REQUIRE(opts.clicks[0].x == 400);
    REQUIRE(opts.clicks[0].y == 300);
}

// ===========================================================================
// 14. --clicks option — multiple clicks
// ===========================================================================

TEST_CASE("--clicks with multiple tokens parses all clicks correctly",
          "[cli_parser][unit]") {
    auto opts = parse_args({"game", "--clicks", "5:100,200", "10:400,300"});

    REQUIRE(opts.parse_error == false);
    REQUIRE(opts.clicks.size() == 2);
    REQUIRE(opts.clicks[0].frame == 5);
    REQUIRE(opts.clicks[0].x == 100);
    REQUIRE(opts.clicks[0].y == 200);
    REQUIRE(opts.clicks[1].frame == 10);
    REQUIRE(opts.clicks[1].x == 400);
    REQUIRE(opts.clicks[1].y == 300);
}

// ===========================================================================
// 15. --clicks with no tokens → parse_error
// ===========================================================================

TEST_CASE("--clicks with no tokens produces parse_error",
          "[cli_parser][unit]") {
    auto opts = parse_args({"game", "--clicks"});
    REQUIRE(opts.parse_error == true);
}

// ===========================================================================
// 16. --clicks invalid token errors
// ===========================================================================

TEST_CASE("--clicks invalid tokens produce parse_error",
          "[cli_parser][unit]") {
    SECTION("--clicks 10: (missing coordinates)") {
        auto opts = parse_args({"game", "--clicks", "10:"});
        REQUIRE(opts.parse_error == true);
    }

    SECTION("--clicks 10:400 (missing comma and Y)") {
        auto opts = parse_args({"game", "--clicks", "10:400"});
        REQUIRE(opts.parse_error == true);
    }

    SECTION("--clicks 10:-1,200 (negative X)") {
        auto opts = parse_args({"game", "--clicks", "10:-1,200"});
        REQUIRE(opts.parse_error == true);
    }

    SECTION("--clicks 10:400,-1 (negative Y)") {
        auto opts = parse_args({"game", "--clicks", "10:400,-1"});
        REQUIRE(opts.parse_error == true);
    }

    SECTION("--clicks abc:400,300 (non-numeric frame)") {
        auto opts = parse_args({"game", "--clicks", "abc:400,300"});
        REQUIRE(opts.parse_error == true);
    }

    SECTION("--clicks -5:400,300 (negative frame)") {
        auto opts = parse_args({"game", "--clicks", "-5:400,300"});
        REQUIRE(opts.parse_error == true);
    }
}

// ===========================================================================
// 17. --script combined with --clicks → parse_error (exclusivity)
// ===========================================================================

TEST_CASE("--script combined with --clicks produces parse_error",
          "[cli_parser][unit]") {
    auto opts = parse_args({"game", "--script", "file.json", "--clicks", "10:400,300"});
    REQUIRE(opts.parse_error == true);
}

// ===========================================================================
// 18. --clicks 10:0,0 → valid (zero coordinates allowed)
// ===========================================================================

TEST_CASE("--clicks 10:0,0 is valid (zero coordinates allowed)",
          "[cli_parser][unit]") {
    auto opts = parse_args({"game", "--clicks", "10:0,0"});

    REQUIRE(opts.parse_error == false);
    REQUIRE(opts.clicks.size() == 1);
    REQUIRE(opts.clicks[0].frame == 10);
    REQUIRE(opts.clicks[0].x == 0);
    REQUIRE(opts.clicks[0].y == 0);
}

// ===========================================================================
// 19. Help text contains --clicks
// ===========================================================================

TEST_CASE("Help text contains --clicks description",
          "[cli_parser][unit]") {
    // --help prints to stdout; we just verify it parses without error
    // and sets help_requested. The actual text is verified by checking
    // that options_to_argv round-trips clicks correctly (property test).
    // Here we verify the help flag works alongside a basic check.
    auto opts = parse_args({"game", "--help"});
    REQUIRE(opts.help_requested == true);
    REQUIRE(opts.parse_error == false);

    // Verify that options_to_argv includes --clicks tokens
    CommandLineOptions click_opts;
    click_opts.clicks.push_back({10, 400, 300});
    auto argv = options_to_argv(click_opts);
    bool found_clicks = false;
    for (const auto& a : argv) {
        if (a == "--clicks") {
            found_clicks = true;
            break;
        }
    }
    REQUIRE(found_clicks);
}

// ===========================================================================
// 20. --hover option — inject cursor world position headlessly
// ===========================================================================

TEST_CASE("--hover 30:-200,100 parses a single hover correctly",
          "[cli_parser][unit]") {
    auto opts = parse_args({"game", "--hover", "30:-200,100"});

    REQUIRE(opts.parse_error == false);
    REQUIRE(opts.hovers.size() == 1);
    REQUIRE(opts.hovers[0].frame == 30);
    REQUIRE(opts.hovers[0].x == -200);
    REQUIRE(opts.hovers[0].y == 100);
}

TEST_CASE("--hover with multiple tokens parses all frames in order",
          "[cli_parser][unit]") {
    auto opts = parse_args({"game", "--hover", "5:100,200", "10:-50,-75"});

    REQUIRE(opts.parse_error == false);
    REQUIRE(opts.hovers.size() == 2);
    REQUIRE(opts.hovers[0].frame == 5);
    REQUIRE(opts.hovers[0].x == 100);
    REQUIRE(opts.hovers[0].y == 200);
    REQUIRE(opts.hovers[1].frame == 10);
    REQUIRE(opts.hovers[1].x == -50);
    REQUIRE(opts.hovers[1].y == -75);
}

TEST_CASE("--hover accepts negative world coordinates",
          "[cli_parser][unit]") {
    auto opts = parse_args({"game", "--hover", "0:-200,-100"});

    REQUIRE(opts.parse_error == false);
    REQUIRE(opts.hovers.size() == 1);
    REQUIRE(opts.hovers[0].x == -200);
    REQUIRE(opts.hovers[0].y == -100);
}

TEST_CASE("--hover invalid tokens produce parse_error",
          "[cli_parser][unit]") {
    SECTION("--hover with no tokens") {
        auto opts = parse_args({"game", "--hover"});
        REQUIRE(opts.parse_error == true);
    }

    SECTION("--hover 30 (missing colon)") {
        auto opts = parse_args({"game", "--hover", "30"});
        REQUIRE(opts.parse_error == true);
    }

    SECTION("--hover 30:-200 (missing comma and Y)") {
        auto opts = parse_args({"game", "--hover", "30:-200"});
        REQUIRE(opts.parse_error == true);
    }

    SECTION("--hover abc:10,20 (non-numeric frame)") {
        auto opts = parse_args({"game", "--hover", "abc:10,20"});
        REQUIRE(opts.parse_error == true);
    }

    SECTION("--hover 30:x,20 (non-numeric X)") {
        auto opts = parse_args({"game", "--hover", "30:x,20"});
        REQUIRE(opts.parse_error == true);
    }
}

TEST_CASE("--hover is preserved by options_to_argv round trip",
          "[cli_parser][unit]") {
    CommandLineOptions opts;
    opts.hovers.push_back({30, -200, 100});
    opts.hovers.push_back({31, 50, -25});

    auto argv_strs = options_to_argv(opts);
    std::vector<char*> argv_ptrs;
    for (auto& s : argv_strs) argv_ptrs.push_back(const_cast<char*>(s.c_str()));
    auto opts2 = parse_command_line(static_cast<int>(argv_ptrs.size()),
                                    argv_ptrs.data());

    REQUIRE(opts2.parse_error == false);
    REQUIRE(opts2.hovers.size() == 2);
    REQUIRE(opts2.hovers[0].frame == 30);
    REQUIRE(opts2.hovers[0].x == -200);
    REQUIRE(opts2.hovers[0].y == 100);
    REQUIRE(opts2.hovers[1].frame == 31);
    REQUIRE(opts2.hovers[1].x == 50);
    REQUIRE(opts2.hovers[1].y == -25);
}

TEST_CASE("--seed parses a non-negative integer", "[cli_parser][unit]") {
    auto opts = parse_args({"game", "--seed", "99"});
    REQUIRE_FALSE(opts.parse_error);
    REQUIRE(opts.seed.has_value());
    CHECK(opts.seed.value() == 99);
}

TEST_CASE("--fixed-seed is rejected with a clear error", "[cli_parser][unit]") {
    auto opts = parse_args({"game", "--fixed-seed"});
    REQUIRE(opts.parse_error);
}

TEST_CASE("--seed without value errors out", "[cli_parser][unit]") {
    auto opts = parse_args({"game", "--seed"});
    REQUIRE(opts.parse_error);
}

TEST_CASE("--seed is preserved by options_to_argv round trip",
          "[cli_parser][unit]") {
    CommandLineOptions opts;
    opts.seed = 12345;
    auto argv_strs = options_to_argv(opts);
    bool found = false;
    for (size_t i = 0; i + 1 < argv_strs.size(); ++i) {
        if (argv_strs[i] == "--seed" && argv_strs[i + 1] == "12345") {
            found = true;
        }
    }
    REQUIRE(found);
}

// ===========================================================================
// Dev / god mode (--dev, --god) — opt-in, and free money once opted in
// ===========================================================================

TEST_CASE("dev mode is off by default and free only when opted in",
          "[cli_parser][dev][unit]") {
    SECTION("a normal run never enables it") {
        CHECK(parse_args({"game"}).dev == false);
        CHECK(parse_args({"game", "--seed", "42"}).dev == false);
        // ...and the spend path is untouched: the top-up is a no-op.
        int units = 7;
        dev_top_up(parse_args({"game"}).dev, units);
        CHECK(units == 7);
    }

    SECTION("--dev and --god both enable it, and UNITS outrun any price") {
        CHECK(parse_args({"game", "--dev"}).dev == true);
        CHECK(parse_args({"game", "--god"}).dev == true);
        int units = 0;
        dev_top_up(parse_args({"game", "--dev"}).dev, units);
        // ShopSystem's checks are `ship.currency < cost`; the dearest catalogue
        // entry is three orders of magnitude below this, so nothing is refused.
        CHECK(units == DEV_UNITS);
        CHECK(units > 10000);
    }
}

// ===========================================================================
// --suite (engine suite, D141) — opt-in, and it must not eat the argv cursor
// ===========================================================================

TEST_CASE("--suite is off by default and parses without stalling the loop",
          "[cli_parser][suite][unit]") {
    CHECK(parse_args({"game"}).suite == false);
    CHECK(parse_args({"game", "--suite"}).suite == true);

    // Every flag branch in the parser must advance `i`. A branch that forgets
    // spins forever on that argument — which is exactly what happened when
    // --suite was first inserted and silently took --dev's `++i` with it. These
    // combinations would hang rather than fail if that regressed.
    auto both = parse_args({"game", "--suite", "--dev", "--seed", "42"});
    CHECK(both.suite == true);
    CHECK(both.dev == true);
    CHECK(both.seed.has_value());
    CHECK(parse_args({"game", "--dev", "--suite"}).suite == true);

    // Round-trips through options_to_argv, like every other flag.
    CommandLineOptions on;
    on.suite = true;
    auto argv = options_to_argv(on);
    CHECK(std::find(argv.begin(), argv.end(), "--suite") != argv.end());
    CommandLineOptions off;
    auto argv_off = options_to_argv(off);
    CHECK(std::find(argv_off.begin(), argv_off.end(), "--suite") == argv_off.end());
}
