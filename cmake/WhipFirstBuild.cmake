cmake_minimum_required(VERSION 3.25)

get_filename_component(WHIP_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

if(NOT DEFINED WHIP_PRESET)
    set(WHIP_PRESET "vs2022")
endif()

if(NOT DEFINED WHIP_CONFIG)
    set(WHIP_CONFIG "Debug")
endif()

if(NOT DEFINED WHIP_BOOTSTRAP_MODE)
    set(WHIP_BOOTSTRAP_MODE "Prompt")
endif()

string(TOLOWER "${WHIP_CONFIG}" WHIP_CONFIG_LOWER)

if(NOT DEFINED WHIP_BUILD_PRESET)
    if(WHIP_PRESET STREQUAL "ninja-msvc-native")
        set(WHIP_BUILD_PRESET "native-${WHIP_CONFIG_LOWER}")
    else()
        set(WHIP_BUILD_PRESET "${WHIP_PRESET}-${WHIP_CONFIG_LOWER}")
    endif()
endif()

find_program(WHP_POWERSHELL NAMES pwsh powershell REQUIRED)

set(_bootstrap_args
    -NoProfile
    -ExecutionPolicy Bypass
    -File "${WHIP_REPO_ROOT}/scripts/bootstrap.ps1"
    -Mode "${WHIP_BOOTSTRAP_MODE}"
    -DepsRoot "${WHIP_REPO_ROOT}/.whip/deps"
    -FromCMake)

if(WHIP_ASSUME_YES OR WHIP_BOOTSTRAP_MODE STREQUAL "Auto")
    list(APPEND _bootstrap_args -Yes)
endif()

message(STATUS "Whip first build bootstrap mode: ${WHIP_BOOTSTRAP_MODE}")
execute_process(
    COMMAND "${WHP_POWERSHELL}" ${_bootstrap_args}
    WORKING_DIRECTORY "${WHIP_REPO_ROOT}"
    RESULT_VARIABLE _bootstrap_result)
if(NOT _bootstrap_result EQUAL 0)
    message(FATAL_ERROR "Whip bootstrap failed with exit code ${_bootstrap_result}.")
endif()

message(STATUS "Configuring Whip with preset: ${WHIP_PRESET}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" --preset "${WHIP_PRESET}"
    WORKING_DIRECTORY "${WHIP_REPO_ROOT}"
    RESULT_VARIABLE _configure_result)
if(NOT _configure_result EQUAL 0)
    message(FATAL_ERROR "Whip configure failed with exit code ${_configure_result}.")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --list-presets=build
    WORKING_DIRECTORY "${WHIP_REPO_ROOT}"
    OUTPUT_VARIABLE _build_presets
    ERROR_QUIET)
if(NOT _build_presets MATCHES "\"${WHIP_BUILD_PRESET}\"")
    message(FATAL_ERROR "Build preset '${WHIP_BUILD_PRESET}' was not found. Pass -DWHIP_BUILD_PRESET=<name> or use a matching WHIP_PRESET/WHIP_CONFIG pair.")
endif()

message(STATUS "Building Whip with preset: ${WHIP_BUILD_PRESET}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" --build --preset "${WHIP_BUILD_PRESET}"
    WORKING_DIRECTORY "${WHIP_REPO_ROOT}"
    RESULT_VARIABLE _build_result)
if(NOT _build_result EQUAL 0)
    message(FATAL_ERROR "Whip build failed with exit code ${_build_result}.")
endif()

message(STATUS "Whip first build completed.")
