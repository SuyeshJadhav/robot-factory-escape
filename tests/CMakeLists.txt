function(add_game_test name)
    add_executable(${name}_tests ${name}_tests.cpp)
    target_link_libraries(${name}_tests PRIVATE robot_factory_core)
    add_test(NAME ${name} COMMAND ${name}_tests)
endfunction()

add_game_test(player_motion)
add_game_test(moving_platform)
add_game_test(drone_patrol)
add_game_test(game_progress)
add_game_test(game_sprites)
add_game_test(network_state)
add_game_test(network_sessions)

target_compile_definitions(game_sprites_tests PRIVATE
    GAME_ASSET_DIR="${PROJECT_SOURCE_DIR}/assets/sprites/"
)
set_tests_properties(network_sessions PROPERTIES TIMEOUT 25)
set_tests_properties(game_sprites PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software"
)
