cmake_minimum_required(VERSION 3.28)
project(robot_factory_escape LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Download the pinned Milestone 1 engine unless a local checkout is supplied.
set(GEF_ENGINE_DIR "" CACHE PATH
    "Optional local GEF_engine v0.1.0 checkout; leave empty to download it")
set(ENGINE_BUILD_TESTS OFF CACHE BOOL "Build engine tests" FORCE)
if(GEF_ENGINE_DIR)
    get_filename_component(GAME_ENGINE_SOURCE_DIR "${GEF_ENGINE_DIR}" ABSOLUTE
        BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    if(NOT EXISTS "${GAME_ENGINE_SOURCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR
            "GEF_engine not found at: ${GAME_ENGINE_SOURCE_DIR}\n"
            "Correct -DGEF_ENGINE_DIR=/absolute/path/to/GEF_engine, or use "
            "-DGEF_ENGINE_DIR= to download the pinned engine automatically.")
    endif()
else()
    include(FetchContent)
    FetchContent_Declare(gef_engine
        GIT_REPOSITORY https://github.com/ShounakDeshmukh/GEF_engine.git
        GIT_TAG 835c7804b8f3ce236330a9d16231f5e5a07500ac # v0.1.0
        SYSTEM
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(gef_engine)
    set(GAME_ENGINE_SOURCE_DIR "${gef_engine_SOURCE_DIR}")
endif()
set(GAME_HUD_FONT "${GAME_ENGINE_SOURCE_DIR}/examples/hello_entity/assets/SuperBouncer.ttf")
if(NOT EXISTS "${GAME_HUD_FONT}")
    message(FATAL_ERROR
        "HUD font not found at: ${GAME_HUD_FONT}\n"
        "Use a complete engine checkout at v0.1.0, including examples/hello_entity/assets.")
endif()
if(GEF_ENGINE_DIR)
    add_subdirectory("${GAME_ENGINE_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/engine" EXCLUDE_FROM_ALL)
endif()

add_executable(robot_factory_escape
    src/main.cpp
    src/game.cpp
    src/player_motion.cpp
    src/game_sprites.cpp
)
target_include_directories(robot_factory_escape PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(robot_factory_escape PRIVATE engine)
target_compile_definitions(robot_factory_escape PRIVATE
    GAME_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/sprites/"
    GAME_FONT_PATH="${GAME_HUD_FONT}"
)
target_compile_options(robot_factory_escape PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
)

option(GAME_BUILD_TESTS "Build game movement checks" ON)
if(GAME_BUILD_TESTS)
    enable_testing()
    add_test(NAME build_setup COMMAND "${CMAKE_COMMAND}"
        "-DGAME_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
        "-DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/setup-checks"
        "-DTEST_GENERATOR=${CMAKE_GENERATOR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/build_setup_tests.cmake")
    add_executable(player_motion_tests tests/player_motion_tests.cpp src/player_motion.cpp)
    target_include_directories(player_motion_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(player_motion_tests PRIVATE engine)
    add_test(NAME player_motion COMMAND player_motion_tests)
    add_executable(drone_patrol_tests tests/drone_patrol_tests.cpp)
    target_include_directories(drone_patrol_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(drone_patrol_tests PRIVATE engine)
    add_test(NAME drone_patrol COMMAND drone_patrol_tests)
    add_executable(game_progress_tests tests/game_progress_tests.cpp)
    target_include_directories(game_progress_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(game_progress_tests PRIVATE engine)
    add_test(NAME game_progress COMMAND game_progress_tests)
    add_executable(game_sprites_tests tests/game_sprites_tests.cpp src/game_sprites.cpp)
    target_include_directories(game_sprites_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(game_sprites_tests PRIVATE engine)
    target_compile_definitions(game_sprites_tests PRIVATE GAME_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/sprites/")
    add_test(NAME game_sprites COMMAND game_sprites_tests)
    set_tests_properties(game_sprites PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software")
endif()
