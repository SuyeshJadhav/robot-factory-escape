cmake_minimum_required(VERSION 3.28)
project(robot_factory_escape LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Consume the team's engine without modifying its build files.
set(ENGINE_BUILD_TESTS OFF CACHE BOOL "Build engine tests" FORCE)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../.." "${CMAKE_CURRENT_BINARY_DIR}/engine" EXCLUDE_FROM_ALL)

add_executable(robot_factory_escape main.cpp game.cpp)
target_link_libraries(robot_factory_escape PRIVATE engine)
target_compile_options(robot_factory_escape PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
)
