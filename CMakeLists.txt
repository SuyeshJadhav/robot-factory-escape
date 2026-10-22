cmake_minimum_required(VERSION 3.28)
project(robot_factory_escape LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Download the pinned Milestone 2 engine unless a local checkout is supplied.
set(GEF_ENGINE_DIR "" CACHE PATH
    "Optional local GEF_engine v0.2.0 checkout; leave empty to download it")
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
        GIT_TAG 155b85c4fcea83bd25fe0bb5acc1b3d661953768 # v0.2.0
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
        "Use a complete engine checkout at v0.2.0, including examples/hello_entity/assets.")
endif()
if(GEF_ENGINE_DIR)
    add_subdirectory("${GAME_ENGINE_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/engine" EXCLUDE_FROM_ALL)
endif()

# Shared game rules and sprite helpers, reused by the client and tests.
add_library(robot_factory_core STATIC
    src/gameplay/player_motion.cpp
    src/graphics/game_sprites.cpp
)
target_include_directories(robot_factory_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(robot_factory_core PUBLIC engine::engine)
target_compile_options(robot_factory_core PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
)

add_executable(robot_factory_escape
    src/app/main.cpp
    src/app/game.cpp
    src/app/game_loop.cpp
    src/gameplay/level.cpp
    src/gameplay/gameplay.cpp
    src/networking/game_network.cpp
    src/graphics/game_render.cpp
)
target_link_libraries(robot_factory_escape PRIVATE robot_factory_core)
target_compile_definitions(robot_factory_escape PRIVATE
    GAME_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/sprites/"
    GAME_FONT_PATH="${GAME_HUD_FONT}"
)
target_compile_options(robot_factory_escape PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
)

add_executable(robot_factory_server src/server/main.cpp)
target_link_libraries(robot_factory_server PRIVATE robot_factory_core)

option(GAME_BUILD_TESTS "Build game regression checks" ON)
if(GAME_BUILD_TESTS)
    enable_testing()
    add_test(NAME build_setup COMMAND "${CMAKE_COMMAND}"
        "-DGAME_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
        "-DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/setup-checks"
        "-DTEST_GENERATOR=${CMAKE_GENERATOR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/build_setup_tests.cmake")
    add_subdirectory(tests)
endif()
