#ifndef PROJECT_PATHS_HPP
#define PROJECT_PATHS_HPP

#include <string>

/**
 * Compile-time project path resolution.
 *
 * CLASS_ROOT_DIR is injected by CMake as the absolute path to the
 * class root directory (e.g. /Users/you/5850/2026/Class-030).
 * This means asset paths resolve correctly no matter where the
 * executable is launched from.
 *
 * Usage:
 *   #include "engine/project_paths.hpp"
 *   std::string assets = project_paths::assets_dir();  // -> "/abs/path/to/Class-030/assets"
 */
namespace project_paths {

/// Absolute path to the class root directory (where CMakeLists.txt lives)
inline std::string class_root() {
    return CLASS_ROOT_DIR;
}

/// Absolute path to the shared assets directory
inline std::string assets_dir() {
    return std::string(CLASS_ROOT_DIR) + "/assets";
}

}  // namespace project_paths

#endif // PROJECT_PATHS_HPP
