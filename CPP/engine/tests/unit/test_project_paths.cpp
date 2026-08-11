/**
 * Unit tests for engine/project_paths.hpp (Task 2b).
 *
 * Pure: no SDL init required. On Linux/dev both functions are CLASS_ROOT_DIR-
 * relative and unchanged in behavior; on Windows they resolve relative to the
 * exe / SDL_GetPrefPath instead (see project_paths.hpp for why).
 */
#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "../../project_paths.hpp"

TEST_CASE("assets_dir points at an existing assets directory", "[paths]") {
    REQUIRE(std::filesystem::exists(project_paths::assets_dir()));
}

TEST_CASE("user_data_dir is absolute and has no trailing separator", "[paths]") {
    std::string d = project_paths::user_data_dir();
    REQUIRE_FALSE(d.empty());
    REQUIRE(std::filesystem::path(d).is_absolute());
    REQUIRE(d.back() != '/');
    REQUIRE(d.back() != '\\');
}

// strip_trailing_seps is the part of assets_dir()/user_data_dir() that only
// runs in the _WIN32 branch (SDL_GetBasePath()/SDL_GetPrefPath() both trail a
// separator) but is plain string logic, so it is exercised here directly with
// Windows-shaped input instead of needing a Windows build to catch a
// regression (Task 2b review: on Linux both path functions just return
// class_root(), which can't tell a correct trim from a broken one).
TEST_CASE("strip_trailing_seps trims trailing separators", "[paths]") {
    CHECK(project_paths::strip_trailing_seps(
              "C:\\Users\\x\\AppData\\Roaming\\conradm\\ReactorDrone\\") ==
          "C:\\Users\\x\\AppData\\Roaming\\conradm\\ReactorDrone");
    CHECK(project_paths::strip_trailing_seps("/home/x/root/") == "/home/x/root");
    CHECK(project_paths::strip_trailing_seps("/home/x/root") == "/home/x/root");
}
