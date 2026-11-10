function(add_game_test name)
    add_executable(${name}_tests ${name}_tests.cpp)
    target_link_libraries(${name}_tests PRIVATE robot_factory_core)
    # Legacy regression setup uses assert; keep those checks active in Release.
    if(MSVC)
        target_compile_options(${name}_tests PRIVATE /UNDEBUG)
    else()
        target_compile_options(${name}_tests PRIVATE -UNDEBUG)
    endif()
    add_test(NAME ${name} COMMAND ${name}_tests)
endfunction()

add_game_test(player_motion)
add_game_test(build_mode)
add_game_test(moving_platform)
add_game_test(game_progress)
add_game_test(game_sprites)
add_game_test(network_state)
add_game_test(network_refactor)
add_game_test(network_sessions)
add_game_test(factory_world)
add_game_test(factory_motion)
add_game_test(respawn)
add_game_test(factory_gameplay)
add_game_test(camera)
add_game_test(factory_replication)
add_game_test(factory_network_sessions)
add_game_test(event_wire)
add_game_test(event_notice)
add_game_test(event_input)
add_game_test(event_lifecycle)
add_game_test(event_remote)
add_game_test(event_relay)
add_game_test(event_sessions)
add_game_test(event_presentation)
add_game_test(event_replay)
set_tests_properties(event_sessions PROPERTIES TIMEOUT 40)
set_tests_properties(factory_network_sessions PROPERTIES TIMEOUT 90)

target_compile_definitions(game_sprites_tests PRIVATE
    GAME_ASSET_DIR="${PROJECT_SOURCE_DIR}/assets/sprites/"
    GAME_FONT_PATH="${GAME_HUD_FONT}"
)
set_tests_properties(network_sessions PROPERTIES TIMEOUT 25)
set_tests_properties(game_sprites PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software"
)

# ABI-specific wrappers stay out of MSVC, macOS and non-GNU builds.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
        AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    find_program(GAME_UNSHARE unshare)
    find_program(GAME_IP ip)
    if(Python3_FOUND AND GAME_UNSHARE AND GAME_IP)
        add_executable(app_smoke_probe app_smoke.cpp
            "${PROJECT_SOURCE_DIR}/src/app/main.cpp" ${GAME_APP_SOURCES})
        target_link_libraries(app_smoke_probe PRIVATE robot_factory_core SDL3::SDL3)
        target_compile_definitions(app_smoke_probe PRIVATE main=game_main ${GAME_APP_DEFINITIONS})
        target_link_options(app_smoke_probe PRIVATE
            "-Wl,--wrap=SDL_GetKeyboardState"
            "-Wl,--wrap=_ZN7factory17renderReplayFrameERN6engine8RendererERKNS0_5SceneEj"
            "-Wl,--wrap=_ZN6engine6events12eventManager7receiveESt5dequeINS_10networking15receivedMessageESaIS4_EEl")
        foreach(mode IN ITEMS drone client-server peer-to-peer)
            add_test(NAME app_smoke_${mode} COMMAND "${GAME_UNSHARE}" --user --map-root-user --net
                "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/app_smoke.py"
                --probe "$<TARGET_FILE:app_smoke_probe>" --server "$<TARGET_FILE:robot_factory_server>"
                --mode "${mode}")
            set_tests_properties(app_smoke_${mode} PROPERTIES TIMEOUT 240 RUN_SERIAL TRUE LABELS app)
        endforeach()
    else()
        message(STATUS "App smoke skipped: Python3, unshare and ip are required")
    endif()
endif()
