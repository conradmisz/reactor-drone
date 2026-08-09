# Expose M_PI and friends from <cmath> on MSVC (not defined by default).
add_compile_definitions(_USE_MATH_DEFINES)

# Game Tests CMakeLists.txt
# This file configures unit and property tests for the game

# Enable testing
enable_testing()

# ============================================================================
# Unit Tests (Catch2)
# ============================================================================

# Glob all unit test files
file(GLOB GAME_UNIT_TESTS "${CMAKE_CURRENT_SOURCE_DIR}/unit/*.cpp")

# Only create unit test executable if there are test files
if(GAME_UNIT_TESTS)
    # Collect engine source files for unit tests
    file(GLOB ENGINE_SOURCES 
        "${CMAKE_CURRENT_SOURCE_DIR}/../../engine/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../engine/ecs/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../engine/ecs/systems/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../engine/ecs/debug/*.cpp"
    )

    # Collect game source files (exclude main.cpp to avoid duplicate main)
    file(GLOB GAME_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../../game/*.cpp")
    list(FILTER GAME_SOURCES EXCLUDE REGEX "main\\.cpp$")
    
    add_executable(game_unit_tests 
        ${GAME_UNIT_TESTS}
        ${ENGINE_SOURCES}
        ${GAME_SOURCES}
    )
    
    # Link Catch2, SDL3, SDL3_image, SDL3_ttf, nlohmann_json, and Lua
    target_link_libraries(game_unit_tests PRIVATE
        Catch2::Catch2WithMain
        SDL3::SDL3
        SDL3_image::SDL3_image
        SDL3_ttf::SDL3_ttf
        nlohmann_json::nlohmann_json
        lua_static
    )
    
    # Set include directories
    target_include_directories(game_unit_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
    )

    # Iteration 3 (D51): test_scaffolding.cpp reads main.cpp as text to prove the
    # per-lane hook blocks still exist. main.cpp is excluded from this target (one
    # main() per binary), so the path has to come in as a definition.
    target_compile_definitions(game_unit_tests PRIVATE
        GAME_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/.."
    )

    # Register with CTest
    add_test(NAME Game_Unit_Tests COMMAND game_unit_tests)
endif()

# ============================================================================
# Property Tests (Catch2)
# ============================================================================

# Glob all property test files
file(GLOB GAME_PROPERTY_TESTS "${CMAKE_CURRENT_SOURCE_DIR}/property/*.cpp")

# Only create property test executable if there are test files
if(GAME_PROPERTY_TESTS)
    # Collect engine source files for property tests
    file(GLOB ENGINE_SOURCES 
        "${CMAKE_CURRENT_SOURCE_DIR}/../../engine/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../engine/ecs/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../engine/ecs/systems/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../engine/ecs/debug/*.cpp"
    )

    # Collect game source files (exclude main.cpp to avoid duplicate main)
    file(GLOB GAME_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../../game/*.cpp")
    list(FILTER GAME_SOURCES EXCLUDE REGEX "main\\.cpp$")
    
    add_executable(game_property_tests 
        ${GAME_PROPERTY_TESTS}
        ${ENGINE_SOURCES}
        ${GAME_SOURCES}
    )
    
    # Link Catch2, SDL3, SDL3_image, SDL3_ttf, nlohmann_json, and Lua
    target_link_libraries(game_property_tests PRIVATE
        Catch2::Catch2WithMain
        SDL3::SDL3
        SDL3_image::SDL3_image
        SDL3_ttf::SDL3_ttf
        nlohmann_json::nlohmann_json
        lua_static
    )
    
    # Set include directories
    target_include_directories(game_property_tests PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
    )
    
    # Register with CTest
    add_test(NAME Game_Property_Tests COMMAND game_property_tests)
endif()
