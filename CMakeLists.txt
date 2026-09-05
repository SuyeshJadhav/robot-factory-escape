cmake_minimum_required(VERSION 3.28)
project(robot_factory_escape LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Consume the team's engine without modifying its build files.
set(ENGINE_BUILD_TESTS OFF CACHE BOOL "Build engine tests" FORCE)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../.." "${CMAKE_CURRENT_BINARY_DIR}/engine" EXCLUDE_FROM_ALL)

add_executable(robot_factory_escape main.cpp game.cpp player_motion.cpp)
target_link_libraries(robot_factory_escape PRIVATE engine)
target_compile_options(robot_factory_escape PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
)

option(GAME_BUILD_TESTS "Build game movement checks" ON)
if(GAME_BUILD_TESTS)
    enable_testing()
    add_executable(player_motion_tests tests/player_motion_tests.cpp player_motion.cpp)
    target_include_directories(player_motion_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(player_motion_tests PRIVATE engine)
    add_test(NAME player_motion COMMAND player_motion_tests)
    add_executable(drone_patrol_tests tests/drone_patrol_tests.cpp)
    target_include_directories(drone_patrol_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(drone_patrol_tests PRIVATE engine)
    add_test(NAME drone_patrol COMMAND drone_patrol_tests)
endif()
