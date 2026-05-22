include_guard(GLOBAL)

function(whip_apply_bootstrap_environment)
    if(WHP_VCPKG_ROOT)
        file(TO_CMAKE_PATH "${WHP_VCPKG_ROOT}" _whip_vcpkg_root)
        set(ENV{VCPKG_ROOT} "${_whip_vcpkg_root}")
        set(_whip_vcpkg_toolchain "${_whip_vcpkg_root}/scripts/buildsystems/vcpkg.cmake")
        if(EXISTS "${_whip_vcpkg_toolchain}" AND NOT CMAKE_TOOLCHAIN_FILE)
            set(CMAKE_TOOLCHAIN_FILE "${_whip_vcpkg_toolchain}" CACHE FILEPATH "vcpkg toolchain file" FORCE)
            message(STATUS "Using vcpkg toolchain: ${CMAKE_TOOLCHAIN_FILE}")
        endif()
    endif()
endfunction()

function(whip_run_bootstrap)
    if(NOT WIN32)
        return()
    endif()

    if(NOT WHP_BOOTSTRAP_DEPS OR WHP_BOOTSTRAP_MODE STREQUAL "Off")
        set(env_file "${CMAKE_SOURCE_DIR}/.whip/deps/deps-env.cmake")
        if(EXISTS "${env_file}")
            include("${env_file}")
            whip_apply_bootstrap_environment()
        endif()
        return()
    endif()

    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/scripts/bootstrap.ps1")
        message(FATAL_ERROR "WHP_BOOTSTRAP_DEPS is ON, but scripts/bootstrap.ps1 does not exist.")
    endif()

    find_program(WHP_POWERSHELL NAMES pwsh powershell)
    if(NOT WHP_POWERSHELL)
        message(FATAL_ERROR "PowerShell was not found. Install PowerShell or set WHP_BOOTSTRAP_DEPS=OFF.")
    endif()

    set(_bootstrap_args
        -NoProfile
        -ExecutionPolicy Bypass
        -File "${CMAKE_SOURCE_DIR}/scripts/bootstrap.ps1"
        -Mode "${WHP_BOOTSTRAP_MODE}"
        -DepsRoot "${CMAKE_SOURCE_DIR}/.whip/deps"
        -FromCMake)

    if(WHP_BOOTSTRAP_ASSUME_YES)
        list(APPEND _bootstrap_args -Yes)
    endif()

    message(STATUS "Running Whip bootstrap: ${WHP_BOOTSTRAP_MODE}")
    execute_process(
        COMMAND "${WHP_POWERSHELL}" ${_bootstrap_args}
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        RESULT_VARIABLE _bootstrap_result)

    if(NOT _bootstrap_result EQUAL 0)
        message(FATAL_ERROR "Whip bootstrap failed with exit code ${_bootstrap_result}. Run scripts/bootstrap.ps1 manually for details.")
    endif()

    set(env_file "${CMAKE_SOURCE_DIR}/.whip/deps/deps-env.cmake")
    if(EXISTS "${env_file}")
        include("${env_file}")
        whip_apply_bootstrap_environment()
    else()
        message(WARNING "Bootstrap completed but did not generate ${env_file}. Dependency discovery may still fail.")
    endif()
endfunction()
