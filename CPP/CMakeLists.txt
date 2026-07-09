# CMake configuration for Class-110 Final Project: "Reactor Drone" arena shooter
# Forked from Class-090 (Pathfinding & AI). Self-contained build -- run: cmake -B build -S .

cmake_minimum_required(VERSION 3.20...4.0)

project(Class110ReactorDroneV2 VERSION 2.0 LANGUAGES CXX C)

# Expose M_PI and friends from <cmath> on MSVC (not defined by default).
add_compile_definitions(_USE_MATH_DEFINES)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable compiler warnings so students get feedback on unhandled enum cases, etc.
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# CLASS_ROOT_DIR points to the class root (one level up from CPP/)
add_compile_definitions(CLASS_ROOT_DIR="${CMAKE_SOURCE_DIR}/..")

# Find SDL3 package using CONFIG mode
find_package(SDL3 REQUIRED CONFIG)
find_package(SDL3_image REQUIRED CONFIG)
find_package(SDL3_ttf REQUIRED CONFIG)

# Fetch Catch2 for unit and property tests
include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.2
)
FetchContent_MakeAvailable(Catch2)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# Fetch Lua 5.4 source and build as a static C library
FetchContent_Declare(
    lua
    GIT_REPOSITORY https://github.com/lua/lua.git
    GIT_TAG v5.4.7
)
FetchContent_MakeAvailable(lua)

# Build Lua as a static C library from source
file(GLOB LUA_SOURCES "${lua_SOURCE_DIR}/*.c")
# Remove lua.c (standalone interpreter) and luac.c (compiler) — we only want the library
list(FILTER LUA_SOURCES EXCLUDE REGEX "lua\\.c$")
list(FILTER LUA_SOURCES EXCLUDE REGEX "luac\\.c$")

add_library(lua_static STATIC ${LUA_SOURCES})
target_include_directories(lua_static PUBLIC ${lua_SOURCE_DIR})
# Compile as C, not C++
set_target_properties(lua_static PROPERTIES LINKER_LANGUAGE C)
# ponytail: silence warnings from vendored Lua sources; -Wpedantic is for our code, not upstream's
target_compile_options(lua_static PRIVATE -w)

# Enable testing for CTest
enable_testing()

# ============================================================================
# Engine and Game
# ============================================================================

add_subdirectory(engine/tests)
add_subdirectory(game)
