/**
 * script_loader.hpp - Load and save debug sessions as JSON script files
 *
 * A script file is a JSON object whose keys mirror the CLI flags:
 *
 *   {
 *     "fps": 30,
 *     "stop_frame": 100,
 *     "paused": false,
 *     "verbose": false,
 *     "clear_logs": true,
 *     "keys": [{"frame": 10, "key": "RIGHT"}, {"frame": 20, "key": "UP"}],
 *     "dump": [0, 25, 50],
 *     "trace": [25],
 *     "screenshot": [10, 30]
 *   }
 *
 * All fields are optional. Omitted fields take their default values.
 *
 * Use --script <file.json> on the command line to run a saved session.
 * Use run.py → "Save Session as Script" to produce a file from an
 * interactively-built session.
 */

#ifndef SCRIPT_LOADER_HPP
#define SCRIPT_LOADER_HPP

#include "cli_parser.hpp"
#include <string>

/**
 * Load a CommandLineOptions from a JSON script file.
 * Throws std::runtime_error on file-not-found or JSON parse error.
 */
CommandLineOptions load_script(const std::string& path);

/**
 * Save a CommandLineOptions to a JSON script file.
 * Throws std::runtime_error on write error.
 * Only non-default values are written so the file stays readable.
 */
void save_script(const CommandLineOptions& opts, const std::string& path);

#endif // SCRIPT_LOADER_HPP
