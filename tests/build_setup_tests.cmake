cmake_minimum_required(VERSION 3.28)

if(NOT DEFINED GAME_SOURCE_DIR OR NOT DEFINED TEST_ROOT OR NOT DEFINED TEST_GENERATOR)
    message(FATAL_ERROR "Pass GAME_SOURCE_DIR, TEST_ROOT and TEST_GENERATOR to run setup checks")
endif()

# Configure an isolated copy without network downloads. The configuration-only
# engine fixture tests dependency selection; a real engine build is separate.
string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef run_id)
set(workspace "${TEST_ROOT}/setup-${run_id}")
set(game_dir "${workspace}/game")
file(MAKE_DIRECTORY "${game_dir}")
file(COPY "${GAME_SOURCE_DIR}/CMakeLists.txt" "${GAME_SOURCE_DIR}/src"
    DESTINATION "${game_dir}")

function(expect_setup_failure name expected_path expected_guidance)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -S "${game_dir}" -B "${workspace}/${name}"
            -G "${TEST_GENERATOR}" ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
        TIMEOUT 30
    )
    set(log "${output}\n${error}")
    file(WRITE "${workspace}/${name}.log" "${log}")
    if(NOT result MATCHES "^[1-9][0-9]*$")
        message(FATAL_ERROR "${name}: expected configure failure, got ${result}\n${log}")
    endif()
    # CMake wraps diagnostics at spaces, including spaces inside a path.
    string(REGEX REPLACE "[ \t\r\n]+" "" compact_log "${log}")
    string(REGEX REPLACE "[ \t\r\n]+" "" compact_path "${expected_path}")
    string(REGEX REPLACE "[ \t\r\n]+" "" compact_guidance "${expected_guidance}")
    string(FIND "${compact_log}" "${compact_path}" path_index)
    string(FIND "${compact_log}" "${compact_guidance}" guidance_index)
    if(path_index EQUAL -1 OR guidance_index EQUAL -1)
        message(FATAL_ERROR "${name}: missing prerequisite path or recovery guidance\n${log}")
    endif()
    if(log MATCHES "ENGINE_SETUP_SHOULD_NOT_RUN")
        message(FATAL_ERROR "${name}: engine configuration ran before prerequisite checks")
    endif()
    message(STATUS "${name}: actionable setup failure verified")
endfunction()

function(expect_setup_success name)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -S "${game_dir}" -B "${workspace}/${name}"
            -G "${TEST_GENERATOR}" -DGAME_BUILD_TESTS=OFF ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
        TIMEOUT 30
    )
    file(WRITE "${workspace}/${name}.log" "${output}\n${error}")
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${name}: configure failed\n${output}\n${error}")
    endif()
    message(STATUS "${name}: configure succeeded")
endfunction()

set(fetched_engine "${workspace}/fetched engine")
file(MAKE_DIRECTORY "${fetched_engine}/examples/hello_entity/assets")
file(WRITE "${fetched_engine}/CMakeLists.txt"
    "cmake_minimum_required(VERSION 3.28)\nadd_library(engine INTERFACE)\n")
# A placeholder suffices for configure; no executable is built with this fixture.
file(WRITE "${fetched_engine}/examples/hello_entity/assets/SuperBouncer.ttf" "")

# An unrelated sibling engine must not override the pinned download by default.
file(MAKE_DIRECTORY "${workspace}/GEF_engine")
file(WRITE "${workspace}/GEF_engine/CMakeLists.txt"
    "message(FATAL_ERROR \"WRONG_SIBLING_ENGINE_SELECTED\")\n")
expect_setup_success(automatic-engine
    "-DFETCHCONTENT_SOURCE_DIR_GEF_ENGINE=${fetched_engine}")
expect_setup_success(local-engine
    "-DGEF_ENGINE_DIR=${fetched_engine}" -DFETCHCONTENT_FULLY_DISCONNECTED=ON)

set(missing_engine "${workspace}/missing engine")
expect_setup_failure(missing-override "${missing_engine}" "-DGEF_ENGINE_DIR="
    "-DGEF_ENGINE_DIR=${missing_engine}")

# This deliberately incomplete engine must be rejected before add_subdirectory.
set(incomplete_engine "${workspace}/incomplete engine")
file(MAKE_DIRECTORY "${incomplete_engine}")
file(WRITE "${incomplete_engine}/CMakeLists.txt"
    "message(FATAL_ERROR \"ENGINE_SETUP_SHOULD_NOT_RUN\")\n")
expect_setup_failure(missing-font "SuperBouncer.ttf" "complete engine checkout"
    "-DGEF_ENGINE_DIR=${incomplete_engine}")
