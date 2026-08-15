# Expose M_PI and friends from <cmath> on MSVC (not defined by default).
add_compile_definitions(_USE_MATH_DEFINES)

# Engine Tests CMakeLists.txt
# This file configures unit tests and property tests for the engine

# Note: Catch2 is already available from parent CMakeLists.txt via FetchContent

# Enable testing
enable_testing()

# ============================================================================
# Unit Tests
# ============================================================================

add_executable(engine_unit_tests
    unit/test_entity_manager.cpp
    unit/test_component_storage.cpp
    unit/test_blackboard.cpp
    unit/test_render_decision.cpp
    unit/test_hud_system.cpp
    unit/test_camera_system.cpp
    unit/test_camera_control_system.cpp
    unit/test_gamedata_loader.cpp
    unit/test_debug_hud_system.cpp
    unit/test_destruction_pipeline.cpp
    unit/test_collision_math.cpp
    unit/test_brute_force_strategy.cpp
    unit/test_wrap_system.cpp
    unit/test_lifetime_system.cpp
    unit/test_movement_wrap_skip.cpp
    unit/test_collision_system.cpp
    unit/test_rotation_system.cpp
    unit/test_collision_pair_count_unit.cpp
    unit/test_uniform_grid_strategy.cpp
    unit/test_circle_overlap.cpp
    unit/test_circle_aabb_overlap.cpp
    unit/test_circle_pipeline.cpp
    unit/test_quadtree_strategy.cpp
    unit/test_obb_obb_overlap.cpp
    unit/test_obb_circle_overlap.cpp
    unit/test_obb_pipeline.cpp
    unit/test_lua_manager.cpp
    unit/test_script_system.cpp
    unit/test_lua_bindings.cpp
    unit/test_input_system_mouse.cpp
    unit/test_gamedata_collider.cpp
    unit/test_sprite_sheet.cpp
    unit/test_animation_system.cpp
    unit/test_tile_map.cpp
    unit/test_pathfinding.cpp
    unit/test_sidecar_loader.cpp
    unit/test_tint.cpp
    unit/test_particle_system.cpp
    unit/test_project_paths.cpp
    unit/test_bloom_math.cpp
    unit/test_line_mesh_math.cpp
    unit/test_particle_mesh.cpp
    unit/test_trail_math.cpp

    # UI & menu layer (Option-040 port)
    unit/test_ui_components_storage.cpp
    unit/test_ui_components_destruction.cpp
    unit/test_ui_style.cpp
    unit/test_ui_render_math.cpp
    unit/test_ui_widget_render_math.cpp
    unit/test_screen_stack_system.cpp
    unit/test_screen_stack_capture.cpp
    unit/test_ui_system.cpp
    unit/test_ui_bindings.cpp
    unit/test_ui_fade_math.cpp
    unit/test_ui_value_interaction.cpp
    unit/test_ui_focus.cpp
    unit/test_ui_canvas_transform.cpp
    unit/test_ui_focus_math.cpp
    unit/test_ui_pulse.cpp

    # Engine implementation files needed by tests
    ../ecs/entity_manager.cpp
    ../ecs/component_storage.cpp
    ../ecs/systems/particle_system.cpp
    ../sidecar_loader.cpp
    ../ecs/systems/camera_system.cpp
    ../ecs/systems/camera_control_system.cpp
    ../ecs/systems/debug_hud_system.cpp
    ../ecs/destruction.cpp
    ../ecs/systems/collision_math.cpp
    ../ecs/systems/brute_force_strategy.cpp
    ../ecs/systems/uniform_grid_strategy.cpp
    ../ecs/systems/quadtree_strategy.cpp
    ../ecs/systems/collision_system.cpp
    ../ecs/systems/wrap_system.cpp
    ../ecs/systems/lifetime_system.cpp
    ../ecs/systems/movement_system.cpp
    ../ecs/systems/rotation_system.cpp
    ../resource_manager.cpp
    ../timer.cpp
    ../gamedata_loader.cpp
    ../lua_manager.cpp
    ../ecs/systems/script_system.cpp
    ../ecs/systems/animation_system.cpp
    ../lua_bindings.cpp
    ../ecs/systems/screen_stack_system.cpp
    ../ecs/systems/screen_fade_system.cpp
    ../ecs/systems/ui_system.cpp
    ../ui_style.cpp
    # register_all_components lives in game/debug_adapters.cpp; the UI component
    # property tests exercise the production registration, not a test double.
    ../../game/debug_adapters.cpp
)

target_link_libraries(engine_unit_tests PRIVATE
    Catch2::Catch2WithMain
    SDL3::SDL3
    SDL3_image::SDL3_image
    SDL3_ttf::SDL3_ttf
    nlohmann_json::nlohmann_json
    lua_static
)

# Set include directories so tests can #include "engine/ecs/..."
target_include_directories(engine_unit_tests PRIVATE
    ${CMAKE_SOURCE_DIR}
)

# Register unit tests with CTest
add_test(NAME EngineUnitTests COMMAND engine_unit_tests)

# ============================================================================
# Timer Unit Tests (opt-in, run with --plusTimer)
# ============================================================================

add_executable(engine_timer_unit_tests
    unit/test_timer.cpp
    unit/test_timer_no_advance.cpp
    
    # Engine implementation files needed by tests
    ../ecs/entity_manager.cpp
    ../ecs/component_storage.cpp
    ../timer.cpp
)

target_link_libraries(engine_timer_unit_tests PRIVATE
    Catch2::Catch2WithMain
    SDL3::SDL3
)

target_include_directories(engine_timer_unit_tests PRIVATE
    ${CMAKE_SOURCE_DIR}
)

add_test(NAME EngineTimerUnitTests COMMAND engine_timer_unit_tests)
set_tests_properties(EngineTimerUnitTests PROPERTIES LABELS "Timer")

# ============================================================================
# Property Tests
# ============================================================================

add_executable(engine_property_tests
    property/test_entity_properties.cpp
    property/test_component_properties.cpp
    property/test_blackboard_properties.cpp
    property/test_render_decision_properties.cpp
    property/test_hud_properties.cpp
    property/test_camera_properties.cpp
    property/test_camera_control_properties.cpp
    property/test_gamedata_loader_properties.cpp
    property/test_new_component_properties.cpp
    property/test_aabb_overlap_properties.cpp
    property/test_brute_force_properties.cpp
    property/test_wrap_system_properties.cpp
    property/test_lifetime_system_properties.cpp
    property/test_collision_system_properties.cpp
    property/test_rotation_system_properties.cpp
    property/test_collision_pair_count_properties.cpp
    property/test_uniform_grid_properties.cpp
    property/test_circle_collider_properties.cpp
    property/test_quadtree_properties.cpp
    property/test_obb_collision_properties.cpp
    property/test_script_properties.cpp
    property/test_lua_bindings_properties.cpp
    property/test_sprite_sheet_properties.cpp
    property/test_animation_system_properties.cpp
    property/test_tile_map_properties.cpp
    property/test_pathfinding_properties.cpp
    property/test_sidecar_loader_properties.cpp
    property/test_particle_properties.cpp

    # UI & menu layer (Option-040 port)
    property/test_ui_components_properties.cpp
    property/test_ui_render_properties.cpp
    property/test_ui_widget_render_properties.cpp
    property/test_ui_interaction_properties.cpp
    property/test_screen_stack_properties.cpp
    property/test_ui_fade_properties.cpp
    property/test_ui_value_interaction_properties.cpp
    property/test_ui_focus_properties.cpp
    property/test_ui_canvas_transform_properties.cpp

    # Engine implementation files needed by tests
    ../ecs/entity_manager.cpp
    ../ecs/component_storage.cpp
    ../ecs/systems/particle_system.cpp
    ../sidecar_loader.cpp
    ../ecs/systems/camera_system.cpp
    ../ecs/systems/camera_control_system.cpp
    ../ecs/destruction.cpp
    ../ecs/systems/collision_math.cpp
    ../ecs/systems/brute_force_strategy.cpp
    ../ecs/systems/uniform_grid_strategy.cpp
    ../ecs/systems/quadtree_strategy.cpp
    ../ecs/systems/collision_system.cpp
    ../ecs/systems/wrap_system.cpp
    ../ecs/systems/lifetime_system.cpp
    ../ecs/systems/rotation_system.cpp
    ../timer.cpp
    ../gamedata_loader.cpp
    ../lua_manager.cpp
    ../ecs/systems/script_system.cpp
    ../ecs/systems/animation_system.cpp
    ../lua_bindings.cpp
    ../ecs/systems/screen_stack_system.cpp
    ../ecs/systems/screen_fade_system.cpp
    ../ecs/systems/ui_system.cpp
    ../ui_style.cpp
    # register_all_components lives in game/debug_adapters.cpp; the UI component
    # property tests exercise the production registration, not a test double.
    ../../game/debug_adapters.cpp
)

target_link_libraries(engine_property_tests PRIVATE
    Catch2::Catch2WithMain
    SDL3::SDL3
    SDL3_image::SDL3_image
    SDL3_ttf::SDL3_ttf
    nlohmann_json::nlohmann_json
    lua_static
)

# Set include directories so tests can #include "engine/ecs/..."
target_include_directories(engine_property_tests PRIVATE
    ${CMAKE_SOURCE_DIR}
)

# Register property tests with CTest
add_test(NAME EnginePropertyTests COMMAND engine_property_tests)

# ============================================================================
# Timer Property Tests (opt-in, run with --plusTimer)
# ============================================================================

add_executable(engine_timer_property_tests
    property/test_timer_properties.cpp
    property/test_timer_bugfix_properties.cpp
    property/test_timer_no_advance_properties.cpp
    
    # Engine implementation files needed by tests
    ../ecs/entity_manager.cpp
    ../ecs/component_storage.cpp
    ../timer.cpp
)

target_link_libraries(engine_timer_property_tests PRIVATE
    Catch2::Catch2WithMain
    SDL3::SDL3
)

target_include_directories(engine_timer_property_tests PRIVATE
    ${CMAKE_SOURCE_DIR}
)

add_test(NAME EngineTimerPropertyTests COMMAND engine_timer_property_tests)
set_tests_properties(EngineTimerPropertyTests PROPERTIES LABELS "Timer")

# ============================================================================
# ResourceManager Unit Tests
# ============================================================================

add_executable(resource_manager_unit_tests
    unit/test_resource_manager.cpp
    ../resource_manager.cpp
)

target_link_libraries(resource_manager_unit_tests PRIVATE
    Catch2::Catch2WithMain
    SDL3::SDL3
    SDL3_image::SDL3_image
    SDL3_ttf::SDL3_ttf
)

target_include_directories(resource_manager_unit_tests PRIVATE
    ${CMAKE_SOURCE_DIR}
)

add_test(NAME EngineResourceManagerUnitTests COMMAND resource_manager_unit_tests)

# ============================================================================
# ResourceManager Property Tests
# ============================================================================

add_executable(resource_manager_property_tests
    property/test_resource_manager_properties.cpp
    ../resource_manager.cpp
)

target_link_libraries(resource_manager_property_tests PRIVATE
    Catch2::Catch2WithMain
    SDL3::SDL3
    SDL3_image::SDL3_image
    SDL3_ttf::SDL3_ttf
)

target_include_directories(resource_manager_property_tests PRIVATE
    ${CMAKE_SOURCE_DIR}
)

add_test(NAME EngineResourceManagerPropertyTests COMMAND resource_manager_property_tests)

# ============================================================================
# Test assets are referenced via absolute paths (CLASS_ROOT_DIR) so no
# file copy is needed — tests find them in the source tree directly.
# ============================================================================
