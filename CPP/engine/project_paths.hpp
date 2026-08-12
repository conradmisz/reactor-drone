#ifndef PROJECT_PATHS_HPP
#define PROJECT_PATHS_HPP

#include <string>

#if defined(_WIN32) || defined(RD_PORTABLE)
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>
#endif

/**
 * Compile-time project path resolution.
 *
 * CLASS_ROOT_DIR is injected by CMake as the absolute path to the
 * class root directory (e.g. /Users/you/5850/2026/Class-030).
 * This means asset paths resolve correctly no matter where the
 * executable is launched from — but only on the machine that built it: the
 * path is baked into the binary at compile time. Fine for the class Linux
 * dev workflow, fatal for a Windows binary shipped to someone else's PC
 * (Task 2b), so `assets_dir()`/`user_data_dir()` branch on `_WIN32` to
 * resolve relative to the running exe instead.
 *
 * Usage:
 *   #include "engine/project_paths.hpp"
 *   std::string assets = project_paths::assets_dir();  // -> "/abs/path/to/Class-030/assets"
 */
namespace project_paths {

/// Absolute path to the class root directory (where CMakeLists.txt lives).
/// Source-tree relative; leave engine test harnesses on this directly.
inline std::string class_root() {
    return CLASS_ROOT_DIR;
}

/// Strips trailing '/' and '\' from a directory path. Pure string logic, no
/// filesystem or SDL call — split out so the Windows-shaped path handling in
/// assets_dir()/user_data_dir() below (SDL_GetBasePath()/SDL_GetPrefPath()
/// results, which trail a separator) is unit-testable on Linux, where the
/// two functions themselves both just return class_root() and can't
/// distinguish a correct implementation from a broken one (Task 2b review).
inline std::string strip_trailing_seps(std::string dir) {
    while (!dir.empty() && (dir.back() == '/' || dir.back() == '\\')) dir.pop_back();
    return dir;
}

/// Absolute path to the read-only shared assets directory.
///
/// RD_PORTABLE (release packaging, any OS) selects the same exe-relative
/// resolution Windows always uses — an installed copy must never look for the
/// build machine's source tree. Dev builds keep CLASS_ROOT_DIR so run.py, the
/// replay canary and the saves/ workflow are untouched.
inline std::string assets_dir() {
#if defined(_WIN32) || defined(RD_PORTABLE)
    // Installed layout is flat: assets/ sits beside the exe (installer/package-*.sh).
    const char* base = SDL_GetBasePath();  // may be null if SDL is not initialized
    std::string dir = base ? strip_trailing_seps(base) : std::string(".");
    return dir + "/assets";
#else
    return std::string(CLASS_ROOT_DIR) + "/assets";
#endif
}

/// Absolute path to writable per-user data (saves, settings). Never inside
/// {app} on Windows — Program Files is not user-writable. No trailing
/// separator on either platform, so `+ "/saves/x.json"` call sites keep
/// working unchanged.
inline std::string user_data_dir() {
#if defined(_WIN32) || defined(RD_PORTABLE)
    // Windows: %APPDATA%\conradm\ReactorDrone. macOS: ~/Library/Application
    // Support/conradm/ReactorDrone. Linux: $XDG_DATA_HOME/conradm/ReactorDrone.
    char* pref = SDL_GetPrefPath("conradm", "ReactorDrone");
    std::string dir = pref ? strip_trailing_seps(pref) : std::string(".");
    if (pref) SDL_free(pref);
    return dir;
#else
    return class_root();
#endif
}

}  // namespace project_paths

#endif // PROJECT_PATHS_HPP
