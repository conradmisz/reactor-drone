/**
 * cli_parser.hpp - Command-line argument parser for debug sessions
 *
 * Provides a pure function (parse_command_line) that takes argc/argv
 * and returns a CommandLineOptions struct. Zero SDL/engine dependencies,
 * directly unit- and property-testable.
 *
 * An inverse function (options_to_argv) reconstructs an argv from a
 * CommandLineOptions struct, enabling round-trip property testing.
 *
 * Requirements: 10.1, 10.2, 10.3
 */

#ifndef CLI_PARSER_HPP
#define CLI_PARSER_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

/**
 * A single synthetic key press to inject at a specific frame.
 *
 * @field frame  Frame number at which to inject the key press
 * @field key    Key name (e.g., "RIGHT", "F1")
 */
struct KeyAction {
    uint64_t frame;
    std::string key;
};

/**
 * A single synthetic mouse click to inject at a specific frame.
 *
 * @field frame  Frame number at which to inject the click
 * @field x      Screen-pixel X coordinate (top-left origin)
 * @field y      Screen-pixel Y coordinate (top-left origin)
 */
struct MouseClickAction {
    uint64_t frame;
    int x;
    int y;
};

/**
 * A single synthetic cursor-hover position to inject at a specific frame.
 *
 * Used to drive the mouse-cursor world position headlessly (no physical
 * mouse), overriding the polled physical mouse on the given frame. Provided
 * for input-injection flag parity across the course games.
 *
 * @field frame  Frame number at which to set the cursor position
 * @field x      Cursor world X (bottom-left origin; may be negative)
 * @field y      Cursor world Y (bottom-left origin; may be negative)
 */
struct HoverAction {
    uint64_t frame;
    int x;
    int y;
};

/**
 * All parsed command-line options for a debug session.
 *
 * Plain data struct with no methods, no SDL dependencies, and no
 * game-engine dependencies. Default-constructible with the default
 * values specified in Requirement 1.3.
 *
 * Requirements: 10.1, 10.2, 10.3
 */
struct CommandLineOptions {
    std::vector<KeyAction> keys;             // --keys FRAME:KEY ...
    std::vector<MouseClickAction> clicks;   // --clicks FRAME:X,Y ...
    std::vector<HoverAction> hovers;         // --hover FRAME:X,Y ...
    std::vector<uint64_t> dump_frames;      // --dump N ...
    std::vector<uint64_t> trace_frames;     // --trace N ...
    std::vector<uint64_t> screenshot_frames; // --screenshot N ...
    std::optional<uint64_t> stop_frame;    // --stopframe N
    bool paused       = false;             // --paused
    bool verbose      = false;             // --verbose / -v
    int  fps          = 0;                 // --fps N (0 = use default 60)
    bool clear_logs   = true;             // --clear-logs (default) / --no-clear-logs
    std::optional<uint64_t> seed;          // --seed N (fixed timestep + debug)
    std::string script_file;               // --script FILE (exclusive with other flags)
    bool debug_keys   = true;              // --debug-keys (default) / --no-debug-keys
    int  level        = 1;                 // --level N (1-based; selects level from levels.json)
    bool dev          = false;             // --dev / --god (developer playtest mode; off by default)
    bool parse_error  = false;             // set on any parse failure
    bool help_requested = false;           // --help
};

/**
 * Parse command-line arguments into a CommandLineOptions struct.
 *
 * Pure function: reads argv, populates a struct, and returns.
 * On error, sets opts.parse_error = true and prints to stderr.
 * On --help, sets opts.help_requested = true and prints to stdout.
 *
 * @param argc  Argument count (from main)
 * @param argv  Argument values (from main)
 * @return Populated CommandLineOptions struct
 *
 * Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 1.6
 */
CommandLineOptions parse_command_line(int argc, char* argv[]);

/**
 * Reconstruct an argv-style vector from a CommandLineOptions struct.
 *
 * Inverse of parse_command_line. Used by the round-trip property test
 * (Requirement 11).
 *
 * @param opts  The options to serialize
 * @return Vector of command-line tokens
 *
 * Requirements: 11.1
 */
std::vector<std::string> options_to_argv(const CommandLineOptions& opts);

/**
 * Valid key names recognized by the parser.
 *
 * LEFT, RIGHT, UP, DOWN, F1, F2, F10, H, J, T, ESC, PLUS, MINUS, W, A, S, D
 */
extern const std::vector<std::string> VALID_KEY_NAMES;

/**
 * Create a timestamped log directory, optionally clearing previous ones.
 *
 * Creates logs/YYYYMMDD_HHMMSS/ and optionally deletes all existing
 * timestamped directories under logs/ when clear_previous is true.
 *
 * @param clear_previous  If true, delete previous log directories first
 * @return Path to the new timestamped log directory
 *
 * Requirements: 9.1, 9.5, 9.6
 */
std::string create_log_directory(bool clear_previous);

#endif // CLI_PARSER_HPP
