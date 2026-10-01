cmake_minimum_required(VERSION 3.28)
project(robot_factory_escape LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Consume the team engine without modifying its build files. A sibling checkout
# works by default; submissions extracted elsewhere can set GEF_ENGINE_DIR.
set(GEF_ENGINE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../GEF_engine" CACHE PATH
    "Path to the team GEF_engine source checkout")
if(NOT EXISTS "${GEF_ENGINE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "GEF_engine not found at ${GEF_ENGINE_DIR}. Clone the team engine beside "
        "this game, or configure with -DGEF_ENGINE_DIR=/path/to/GEF_engine")
endif()
set(ENGINE_BUILD_TESTS OFF CACHE BOOL "Build engine tests" FORCE)
add_subdirectory("${GEF_ENGINE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/engine" EXCLUDE_FROM_ALL)

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
    GAME_FONT_PATH="${GEF_ENGINE_DIR}/examples/hello_entity/assets/SuperBouncer.ttf"
)
target_compile_options(robot_factory_escape PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
)

add_executable(robot_factory_server src/server/main.cpp)
target_link_libraries(robot_factory_server PRIVATE robot_factory_core)

option(GAME_BUILD_TESTS "Build game regression checks" ON)
if(GAME_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
