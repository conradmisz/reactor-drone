# Expose M_PI and friends from <cmath> on MSVC (not defined by default).
add_compile_definitions(_USE_MATH_DEFINES)

# CMakeLists.txt for the game

# Engine source files — listed explicitly (sorted) so that adding or removing
# an engine .cpp is an intentional, reviewable change. Do NOT replace this
# with a glob: a glob silently absorbs stray files (see Defect 1.19).
set(ENGINE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/ecs/component_storage.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/ecs/debug/json_serializer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/ecs/destruction.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/ecs/entity_manager.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/ecs/systems/animation_system.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/ecs/systems/brute_force_strategy.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/ecs/systems/camera_control_system.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/ecs/systems/camera_system.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/ecs/systems/chip_synth_system.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/ecs/systems/collision_math.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/ecs/systems/collision_system.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/ecs/systems/debug_hud_system.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/ecs/systems/hud_system.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/ecs/systems/input_system.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/ecs/systems/lifetime_system.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/ecs/systems/movement_system.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/ecs/systems/player_control_system.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/ecs/systems/quadtree_strategy.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/ecs/systems/palette_system.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/ecs/systems/particle_system.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/ecs/systems/render_system.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/ecs/systems/resonance_grid_system.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/ecs/systems/rotation_system.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/ecs/systems/screenshot_system.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/ecs/systems/script_system.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/ecs/systems/screen_fade_system.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/ecs/systems/screen_stack_system.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/ecs/systems/ui_render_system.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/ecs/systems/ui_system.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/ecs/systems/uniform_grid_strategy.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/ecs/systems/wrap_system.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/gamedata_loader.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/lua_bindings.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/lua_manager.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/resource_manager.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/sidecar_loader.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/timer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../engine/ui_style.cpp"
)

# Collect game source files
file(GLOB GAME_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

# Create the executable
add_executable(game ${ENGINE_SOURCES} ${GAME_SOURCES})

# Link SDL3, SDL3_image, SDL3_ttf, nlohmann_json, and Lua
target_link_libraries(game PRIVATE SDL3::SDL3 SDL3_image::SDL3_image SDL3_ttf::SDL3_ttf nlohmann_json::nlohmann_json lua_static)

# Set include directories so games can #include "engine/..."
target_include_directories(game PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Windows-specific: Copy SDL3.dll to output directory
if(WIN32)
    add_custom_command(TARGET game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:game>
        COMMENT "Copying SDL3 DLL to output directory"
    )
endif()

# Add tests subdirectory
add_subdirectory(tests)
