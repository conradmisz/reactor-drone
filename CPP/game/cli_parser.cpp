/**
 * cli_parser.cpp - Command-line argument parser for debug sessions
 *
 * Implements parse_command_line, options_to_argv, VALID_KEY_NAMES,
 * and create_log_directory. Pure functions with no SDL/engine dependencies.
 *
 * Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 2.1, 2.4, 2.5,
 *               3.1, 3.6, 4.1, 4.7, 5.1, 5.3, 6.1, 7.1, 8.1, 8.4,
 *               9.1, 9.3, 9.4, 9.5, 9.6, 9.7, 11.1
 */

#include "cli_parser.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// VALID_KEY_NAMES
// ---------------------------------------------------------------------------

const std::vector<std::string> VALID_KEY_NAMES = {
    "LEFT", "RIGHT", "UP", "DOWN", "SPACE",
    "F1", "F2", "F9", "F10",
    "H", "J", "T", "C", "ESC",
    "PLUS", "MINUS",
    "W", "A", "S", "D", "R", "P",
    // Gameplay Phase 3: shop keys, so a headless script can open the shop and buy.
    "B", "1", "2", "3", "4", "5", "6", "7", "8",
    // Gameplay Phase 4: TAB flips to the gear page, Q spends the consumable.
    "TAB", "Q",
    // Iteration 3 (D57): the thruster dash, so a headless script can fire one.
    "LSHIFT"
};

// ---------------------------------------------------------------------------
// Helpers (internal linkage)
// ---------------------------------------------------------------------------

namespace {

/**
 * Print a usage / help message to the given stream.
 */
void print_usage(std::ostream& out) {
    out << "Usage: game [OPTIONS]\n"
        << "\n"
        << "Options:\n"
        << "  --keys FRAME:KEY ...   Inject key presses at specific frames\n"
        << "  --clicks FRAME:X,Y ... Inject mouse clicks at specific screen coordinates\n"
        << "  --hover FRAME:X,Y ...  Inject cursor world position at specific frames\n"
        << "  --dump N ...           Dump game state at specified frames\n"
        << "  --trace N ...          Trace system execution at specified frames\n"
        << "  --stopframe N          Stop after frame N\n"
        << "  --paused               Start in paused (debug) mode\n"
        << "  --seed N               Fixed timestep for frame-for-frame debug runs\n"
        << "  --verbose, -v          Enable per-frame verbose logging\n"
        << "  --fps N                Override target FPS (positive integer)\n"
        << "  --clear-logs           Clear previous log directories (default)\n"
        << "  --no-clear-logs        Preserve previous log directories\n"
        << "  --screenshot N ...     Save a BMP screenshot at specified frames\n"
        << "  --debug-keys           Enable interactive J/T dump/trace keys (default)\n"
        << "  --no-debug-keys        Disable interactive J/T dump/trace keys\n"
        << "  --level N              Start on level N (1-based; from assets/levels.json)\n"
        << "  --script FILE          Load session from a JSON script file (exclusive)\n"
        << "  --help                 Show this help message\n";
}

/**
 * Return true if `s` looks like a flag (starts with "--" or is "-v").
 */
bool is_flag(const std::string& s) {
    if (s.size() >= 2 && s[0] == '-' && s[1] == '-') return true;
    if (s == "-v") return true;
    return false;
}

/**
 * Return true if `name` is in VALID_KEY_NAMES.
 */
bool is_valid_key_name(const std::string& name) {
    return std::find(VALID_KEY_NAMES.begin(), VALID_KEY_NAMES.end(), name)
           != VALID_KEY_NAMES.end();
}

/**
 * Try to parse a non-negative uint64_t from `s`.
 * Returns true on success, writing the value to `out`.
 */
bool parse_uint64(const std::string& s, uint64_t& out) {
    if (s.empty()) return false;
    // Reject negative numbers and non-digit characters
    for (char c : s) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    }
    try {
        unsigned long long val = std::stoull(s);
        out = static_cast<uint64_t>(val);
        return true;
    } catch (...) {
        return false;
    }
}

/**
 * Try to parse an integer from `s`.
 * Returns true on success, writing the value to `out`.
 */
bool parse_int(const std::string& s, int& out) {
    if (s.empty()) return false;
    try {
        size_t pos = 0;
        int val = std::stoi(s, &pos);
        if (pos != s.size()) return false;  // trailing chars
        out = val;
        return true;
    } catch (...) {
        return false;
    }
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// parse_command_line
// ---------------------------------------------------------------------------

CommandLineOptions parse_command_line(int argc, char* argv[]) {
    CommandLineOptions opts;

    int i = 1;  // skip argv[0] (program name)

    while (i < argc) {
        std::string arg = argv[i];

        if (arg == "--help") {
            opts.help_requested = true;
            print_usage(std::cout);
            ++i;

        } else if (arg == "--paused") {
            opts.paused = true;
            ++i;

        } else if (arg == "--fixed-seed") {
            std::cerr << "Error: --fixed-seed is removed; use --seed 42 (or another non-negative integer)\n";
            print_usage(std::cerr);
            opts.parse_error = true;
            return opts;

        } else if (arg == "--seed") {
            ++i;
            if (i >= argc) {
                std::cerr << "Error: --seed requires a non-negative integer value\n";
                print_usage(std::cerr);
                opts.parse_error = true;
                return opts;
            }
            uint64_t seed_val = 0;
            if (!parse_uint64(argv[i], seed_val)) {
                std::cerr << "Error: --seed value '" << argv[i]
                          << "' is not a valid non-negative integer\n";
                print_usage(std::cerr);
                opts.parse_error = true;
                return opts;
            }
            opts.seed = seed_val;
            ++i;

        } else if (arg == "--verbose" || arg == "-v") {
            opts.verbose = true;
            ++i;

        } else if (arg == "--clear-logs") {
            opts.clear_logs = true;
            ++i;

        } else if (arg == "--no-clear-logs") {
            opts.clear_logs = false;
            ++i;

        } else if (arg == "--debug-keys") {
            opts.debug_keys = true;
            ++i;

        } else if (arg == "--no-debug-keys") {
            opts.debug_keys = false;
            ++i;

        } else if (arg == "--fps") {
            ++i;
            if (i >= argc) {
                std::cerr << "Error: --fps requires a positive integer value\n";
                print_usage(std::cerr);
                opts.parse_error = true;
                return opts;
            }
            int fps_val = 0;
            if (!parse_int(argv[i], fps_val)) {
                std::cerr << "Error: --fps value '" << argv[i]
                          << "' is not a valid integer\n";
                print_usage(std::cerr);
                opts.parse_error = true;
                return opts;
            }
            if (fps_val <= 0) {
                std::cerr << "Error: --fps value must be a positive integer, got "
                          << fps_val << "\n";
                print_usage(std::cerr);
                opts.parse_error = true;
                return opts;
            }
            opts.fps = fps_val;
            ++i;

        } else if (arg == "--stopframe") {
            ++i;
            if (i >= argc) {
                std::cerr << "Error: --stopframe requires a non-negative integer value\n";
                print_usage(std::cerr);
                opts.parse_error = true;
                return opts;
            }
            uint64_t frame_val = 0;
            if (!parse_uint64(argv[i], frame_val)) {
                std::cerr << "Error: --stopframe value '" << argv[i]
                          << "' is not a valid non-negative integer\n";
                print_usage(std::cerr);
                opts.parse_error = true;
                return opts;
            }
            opts.stop_frame = frame_val;
            ++i;

        } else if (arg == "--level") {
            ++i;
            if (i >= argc) {
                std::cerr << "Error: --level requires a positive integer value\n";
                print_usage(std::cerr);
                opts.parse_error = true;
                return opts;
            }
            uint64_t level_val = 0;
            if (!parse_uint64(argv[i], level_val) || level_val < 1) {
                std::cerr << "Error: --level value '" << argv[i]
                          << "' is not a positive integer (levels are 1-based)\n";
                print_usage(std::cerr);
                opts.parse_error = true;
                return opts;
            }
            opts.level = static_cast<int>(level_val);
            ++i;

        } else if (arg == "--keys") {
            ++i;
            bool consumed_any = false;
            while (i < argc && !is_flag(argv[i])) {
                std::string token = argv[i];
                // Expect FRAME:KEY format
                auto colon_pos = token.find(':');
                if (colon_pos == std::string::npos) {
                    std::cerr << "Error: invalid key action token '" << token
                              << "' (expected FRAME:KEY format)\n";
                    print_usage(std::cerr);
                    opts.parse_error = true;
                    return opts;
                }
                std::string frame_str = token.substr(0, colon_pos);
                std::string key_str = token.substr(colon_pos + 1);

                uint64_t frame_val = 0;
                if (!parse_uint64(frame_str, frame_val)) {
                    std::cerr << "Error: invalid frame number '" << frame_str
                              << "' in key action token '" << token << "'\n";
                    print_usage(std::cerr);
                    opts.parse_error = true;
                    return opts;
                }
                if (!is_valid_key_name(key_str)) {
                    std::cerr << "Error: invalid key name '" << key_str
                              << "' in key action token '" << token << "'\n";
                    print_usage(std::cerr);
                    opts.parse_error = true;
                    return opts;
                }
                opts.keys.push_back({frame_val, key_str});
                consumed_any = true;
                ++i;
            }
            if (!consumed_any) {
                std::cerr << "Error: --keys requires at least one FRAME:KEY token\n";
                print_usage(std::cerr);
                opts.parse_error = true;
                return opts;
            }

        } else if (arg == "--clicks") {
            ++i;
            bool consumed_any = false;
            while (i < argc && !is_flag(argv[i])) {
                std::string token = argv[i];
                // Expect FRAME:X,Y format
                auto colon_pos = token.find(':');
                if (colon_pos == std::string::npos) {
                    std::cerr << "Error: invalid click action token '" << token
                              << "' (expected FRAME:X,Y format)\n";
                    print_usage(std::cerr);
                    opts.parse_error = true;
                    return opts;
                }
                std::string frame_str = token.substr(0, colon_pos);
                std::string coord_str = token.substr(colon_pos + 1);

                uint64_t frame_val = 0;
                if (!parse_uint64(frame_str, frame_val)) {
                    std::cerr << "Error: invalid frame number '" << frame_str
                              << "' in click action token '" << token << "'\n";
                    print_usage(std::cerr);
                    opts.parse_error = true;
                    return opts;
                }

                auto comma_pos = coord_str.find(',');
                if (comma_pos == std::string::npos) {
                    std::cerr << "Error: invalid coordinates '" << coord_str
                              << "' in click action token '" << token
                              << "' (expected X,Y format)\n";
                    print_usage(std::cerr);
                    opts.parse_error = true;
                    return opts;
                }
                std::string x_str = coord_str.substr(0, comma_pos);
                std::string y_str = coord_str.substr(comma_pos + 1);

                int x_val = 0, y_val = 0;
                if (!parse_int(x_str, x_val) || !parse_int(y_str, y_val)) {
                    std::cerr << "Error: invalid coordinate value in click action token '"
                              << token << "'\n";
                    print_usage(std::cerr);
                    opts.parse_error = true;
                    return opts;
                }
                if (x_val < 0 || y_val < 0) {
                    std::cerr << "Error: negative coordinate value in click action token '"
                              << token << "'\n";
                    print_usage(std::cerr);
                    opts.parse_error = true;
                    return opts;
                }

                opts.clicks.push_back({frame_val, x_val, y_val});
                consumed_any = true;
                ++i;
            }
            if (!consumed_any) {
                std::cerr << "Error: --clicks requires at least one FRAME:X,Y token\n";
                print_usage(std::cerr);
                opts.parse_error = true;
                return opts;
            }

        } else if (arg == "--hover") {
            ++i;
            bool consumed_any = false;
            while (i < argc && !is_flag(argv[i])) {
                std::string token = argv[i];
                // Expect FRAME:X,Y format
                auto colon_pos = token.find(':');
                if (colon_pos == std::string::npos) {
                    std::cerr << "Error: invalid hover action token '" << token
                              << "' (expected FRAME:X,Y format)\n";
                    print_usage(std::cerr);
                    opts.parse_error = true;
                    return opts;
                }
                std::string frame_str = token.substr(0, colon_pos);
                std::string coord_str = token.substr(colon_pos + 1);

                uint64_t frame_val = 0;
                if (!parse_uint64(frame_str, frame_val)) {
                    std::cerr << "Error: invalid frame number '" << frame_str
                              << "' in hover action token '" << token << "'\n";
                    print_usage(std::cerr);
                    opts.parse_error = true;
                    return opts;
                }

                auto comma_pos = coord_str.find(',');
                if (comma_pos == std::string::npos) {
                    std::cerr << "Error: invalid coordinates '" << coord_str
                              << "' in hover action token '" << token
                              << "' (expected X,Y format)\n";
                    print_usage(std::cerr);
                    opts.parse_error = true;
                    return opts;
                }
                std::string x_str = coord_str.substr(0, comma_pos);
                std::string y_str = coord_str.substr(comma_pos + 1);

                // World coordinates may be negative (bottom-left origin),
                // so parse as signed ints (std::stoi handles negatives).
                int x_val = 0, y_val = 0;
                if (!parse_int(x_str, x_val) || !parse_int(y_str, y_val)) {
                    std::cerr << "Error: invalid coordinate value in hover action token '"
                              << token << "'\n";
                    print_usage(std::cerr);
                    opts.parse_error = true;
                    return opts;
                }

                opts.hovers.push_back({frame_val, x_val, y_val});
                consumed_any = true;
                ++i;
            }
            if (!consumed_any) {
                std::cerr << "Error: --hover requires at least one FRAME:X,Y token\n";
                print_usage(std::cerr);
                opts.parse_error = true;
                return opts;
            }

        } else if (arg == "--dump") {
            ++i;
            bool consumed_any = false;
            while (i < argc && !is_flag(argv[i])) {
                uint64_t frame_val = 0;
                if (!parse_uint64(argv[i], frame_val)) {
                    std::cerr << "Error: invalid dump frame number '" << argv[i] << "'\n";
                    print_usage(std::cerr);
                    opts.parse_error = true;
                    return opts;
                }
                opts.dump_frames.push_back(frame_val);
                consumed_any = true;
                ++i;
            }
            if (!consumed_any) {
                std::cerr << "Error: --dump requires at least one frame number\n";
                print_usage(std::cerr);
                opts.parse_error = true;
                return opts;
            }

        } else if (arg == "--trace") {
            ++i;
            bool consumed_any = false;
            while (i < argc && !is_flag(argv[i])) {
                uint64_t frame_val = 0;
                if (!parse_uint64(argv[i], frame_val)) {
                    std::cerr << "Error: invalid trace frame number '" << argv[i] << "'\n";
                    print_usage(std::cerr);
                    opts.parse_error = true;
                    return opts;
                }
                opts.trace_frames.push_back(frame_val);
                consumed_any = true;
                ++i;
            }
            if (!consumed_any) {
                std::cerr << "Error: --trace requires at least one frame number\n";
                print_usage(std::cerr);
                opts.parse_error = true;
                return opts;
            }

        } else if (arg == "--screenshot") {
            ++i;
            bool consumed_any = false;
            while (i < argc && !is_flag(argv[i])) {
                uint64_t frame_val = 0;
                if (!parse_uint64(argv[i], frame_val)) {
                    std::cerr << "Error: invalid screenshot frame number '" << argv[i] << "'\n";
                    print_usage(std::cerr);
                    opts.parse_error = true;
                    return opts;
                }
                opts.screenshot_frames.push_back(frame_val);
                consumed_any = true;
                ++i;
            }
            if (!consumed_any) {
                std::cerr << "Error: --screenshot requires at least one frame number\n";
                print_usage(std::cerr);
                opts.parse_error = true;
                return opts;
            }

        } else if (arg == "--script") {
            ++i;
            if (i >= argc) {
                std::cerr << "Error: --script requires a filename\n";
                print_usage(std::cerr);
                opts.parse_error = true;
                return opts;
            }
            opts.script_file = argv[i];
            ++i;

        } else {
            // Unrecognized argument
            std::cerr << "Error: unrecognized argument '" << arg << "'\n";
            print_usage(std::cerr);
            opts.parse_error = true;
            return opts;
        }
    }

    // --script must be the only flag (exclusive with all session-building args)
    if (!opts.script_file.empty()) {
        bool has_other = !opts.keys.empty() || !opts.clicks.empty()
                      || !opts.hovers.empty()
                      || !opts.dump_frames.empty()
                      || !opts.trace_frames.empty() || !opts.screenshot_frames.empty()
                      || opts.stop_frame.has_value() || opts.seed.has_value()
                      || opts.fps > 0
                      || opts.paused || opts.verbose || !opts.clear_logs;
        if (has_other) {
            std::cerr << "Error: --script cannot be combined with other session flags\n";
            print_usage(std::cerr);
            opts.parse_error = true;
            return opts;
        }
    }

    return opts;
}

// ---------------------------------------------------------------------------
// options_to_argv
// ---------------------------------------------------------------------------

std::vector<std::string> options_to_argv(const CommandLineOptions& opts) {
    std::vector<std::string> argv;
    argv.push_back("game");  // dummy program name as argv[0]

    // --keys
    if (!opts.keys.empty()) {
        argv.push_back("--keys");
        for (const auto& ka : opts.keys) {
            argv.push_back(std::to_string(ka.frame) + ":" + ka.key);
        }
    }

    // --clicks
    if (!opts.clicks.empty()) {
        argv.push_back("--clicks");
        for (const auto& ca : opts.clicks) {
            argv.push_back(std::to_string(ca.frame) + ":" +
                           std::to_string(ca.x) + "," +
                           std::to_string(ca.y));
        }
    }

    // --hover
    if (!opts.hovers.empty()) {
        argv.push_back("--hover");
        for (const auto& hv : opts.hovers) {
            argv.push_back(std::to_string(hv.frame) + ":" +
                           std::to_string(hv.x) + "," +
                           std::to_string(hv.y));
        }
    }

    // --dump
    if (!opts.dump_frames.empty()) {
        argv.push_back("--dump");
        for (uint64_t f : opts.dump_frames) {
            argv.push_back(std::to_string(f));
        }
    }

    // --trace
    if (!opts.trace_frames.empty()) {
        argv.push_back("--trace");
        for (uint64_t f : opts.trace_frames) {
            argv.push_back(std::to_string(f));
        }
    }

    // --screenshot
    if (!opts.screenshot_frames.empty()) {
        argv.push_back("--screenshot");
        for (uint64_t f : opts.screenshot_frames) {
            argv.push_back(std::to_string(f));
        }
    }

    // --stopframe
    if (opts.stop_frame.has_value()) {
        argv.push_back("--stopframe");
        argv.push_back(std::to_string(opts.stop_frame.value()));
    }

    // --paused
    if (opts.paused) {
        argv.push_back("--paused");
    }

    // --seed
    if (opts.seed.has_value()) {
        argv.push_back("--seed");
        argv.push_back(std::to_string(opts.seed.value()));
    }

    // --verbose
    if (opts.verbose) {
        argv.push_back("--verbose");
    }

    // --fps
    if (opts.fps > 0) {
        argv.push_back("--fps");
        argv.push_back(std::to_string(opts.fps));
    }

    // --clear-logs / --no-clear-logs
    if (opts.clear_logs) {
        argv.push_back("--clear-logs");
    } else {
        argv.push_back("--no-clear-logs");
    }

    // --debug-keys / --no-debug-keys
    if (opts.debug_keys) {
        argv.push_back("--debug-keys");
    } else {
        argv.push_back("--no-debug-keys");
    }

    return argv;
}

// ---------------------------------------------------------------------------
// create_log_directory
// ---------------------------------------------------------------------------

std::string create_log_directory(bool clear_previous) {
    // Get current time
    auto now = std::chrono::system_clock::now();
    std::time_t now_t = std::chrono::system_clock::to_time_t(now);
    std::tm now_tm{};
#if defined(_WIN32)
    localtime_s(&now_tm, &now_t);
#else
    localtime_r(&now_t, &now_tm);
#endif

    // Format as YYYY-MMM-DD-HH-MM-SS (e.g. 2026-Mar-20-14-50-47)
    static const char* month_names[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(4) << (now_tm.tm_year + 1900)
        << "-" << month_names[now_tm.tm_mon]
        << "-" << std::setw(2) << now_tm.tm_mday
        << "-" << std::setw(2) << now_tm.tm_hour
        << "-" << std::setw(2) << now_tm.tm_min
        << "-" << std::setw(2) << now_tm.tm_sec;

    std::string timestamp = oss.str();
    fs::path logs_dir = "logs";
    fs::path new_dir = logs_dir / timestamp;

    // Optionally clear previous timestamped directories
    if (clear_previous && fs::exists(logs_dir)) {
        // Match directories that look like YYYY-MMM-DD-HH-MM-SS (20 chars)
        // Pattern: 4 digits, dash, 3 alpha, dash, 2 digits, dash, 2 digits, dash, 2 digits, dash, 2 digits
        std::error_code ec;
        for (auto& entry : fs::directory_iterator(logs_dir, ec)) {
            if (entry.is_directory()) {
                std::string name = entry.path().filename().string();
                if (name.size() == 20 &&
                    name[4] == '-' && name[8] == '-' && name[11] == '-' &&
                    name[14] == '-' && name[17] == '-' &&
                    std::isalpha(static_cast<unsigned char>(name[5])) &&
                    std::isalpha(static_cast<unsigned char>(name[6])) &&
                    std::isalpha(static_cast<unsigned char>(name[7]))) {
                    fs::remove_all(entry.path(), ec);
                }
            }
        }
    }

    // Create the new timestamped directory
    std::error_code ec;
    fs::create_directories(new_dir, ec);
    if (ec) {
        std::cerr << "Warning: failed to create log directory '"
                  << new_dir.string() << "': " << ec.message() << "\n";
    }

    return new_dir.string();
}
