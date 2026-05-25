include_guard(GLOBAL)

function(whip_configure_output_directories)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(WHP_ARCH "x86_64" CACHE INTERNAL "Whip architecture label")
    else()
        set(WHP_ARCH "x86" CACHE INTERNAL "Whip architecture label")
    endif()

    if(WIN32)
        set(WHP_SYSTEM "windows" CACHE INTERNAL "Whip system label")
    elseif(APPLE)
        set(WHP_SYSTEM "macos" CACHE INTERNAL "Whip system label")
    elseif(UNIX)
        set(WHP_SYSTEM "linux" CACHE INTERNAL "Whip system label")
    else()
        set(WHP_SYSTEM "unknown" CACHE INTERNAL "Whip system label")
    endif()

    set(WHP_OUTPUT_ROOT "${CMAKE_SOURCE_DIR}/bin" CACHE PATH "Whip runtime output root")
    set(WHP_INTERMEDIATE_ROOT "${CMAKE_SOURCE_DIR}/bin-int" CACHE PATH "Whip intermediate output root")

    foreach(cfg Debug Release Dist)
        string(TOUPPER "${cfg}" cfg_upper)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${cfg_upper} "${WHP_OUTPUT_ROOT}/${cfg}-${WHP_SYSTEM}-${WHP_ARCH}" CACHE PATH "" FORCE)
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${cfg_upper} "${WHP_OUTPUT_ROOT}/${cfg}-${WHP_SYSTEM}-${WHP_ARCH}" CACHE PATH "" FORCE)
        set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${cfg_upper} "${WHP_OUTPUT_ROOT}/${cfg}-${WHP_SYSTEM}-${WHP_ARCH}" CACHE PATH "" FORCE)
    endforeach()
endfunction()
function(whip_set_target_output target)
    foreach(cfg Debug Release Dist)
        string(TOUPPER "${cfg}" cfg_upper)
        set_target_properties(${target} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_${cfg_upper} "${WHP_OUTPUT_ROOT}/${cfg}-${WHP_SYSTEM}-${WHP_ARCH}/${target}"
            LIBRARY_OUTPUT_DIRECTORY_${cfg_upper} "${WHP_OUTPUT_ROOT}/${cfg}-${WHP_SYSTEM}-${WHP_ARCH}/${target}"
            ARCHIVE_OUTPUT_DIRECTORY_${cfg_upper} "${WHP_OUTPUT_ROOT}/${cfg}-${WHP_SYSTEM}-${WHP_ARCH}/${target}")
    endforeach()
endfunction()

function(whip_copy_directory_after_build target source_dir dest_name)
    if(EXISTS "${source_dir}")
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND}
                    -DWHP_COPY_SOURCE=${source_dir}
                    -DWHP_COPY_DESTINATION=$<TARGET_FILE_DIR:${target}>/${dest_name}
                    -P "${CMAKE_SOURCE_DIR}/cmake/WhipCopyDirectoryIfDifferent.cmake"
            COMMENT "Copying ${dest_name} for ${target}"
            VERBATIM)
    endif()
endfunction()

function(whip_copy_mono_runtime_after_build target)
    if(NOT WHP_ENABLE_MONO)
        return()
    endif()

    if(WHP_MONO_RUNTIME_LIB_DIR AND EXISTS "${WHP_MONO_RUNTIME_LIB_DIR}")
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target}>/mono/lib"
            COMMAND ${CMAKE_COMMAND}
                    -DWHP_COPY_SOURCE=${WHP_MONO_RUNTIME_LIB_DIR}
                    -DWHP_COPY_DESTINATION=$<TARGET_FILE_DIR:${target}>/mono/lib/mono
                    -P "${CMAKE_SOURCE_DIR}/cmake/WhipCopyDirectoryIfDifferent.cmake"
            COMMENT "Copying Mono runtime assemblies for ${target}"
            VERBATIM)
    endif()

    if(WHP_MONO_DLL AND EXISTS "${WHP_MONO_DLL}")
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${WHP_MONO_DLL}"
                    "$<TARGET_FILE_DIR:${target}>"
            COMMENT "Copying Mono runtime DLL for ${target}"
            VERBATIM)
    endif()
endfunction()


function(whip_copy_runtime_target_after_build target)
    foreach(candidate IN LISTS ARGN)
        if(TARGET ${candidate})
            get_target_property(_aliased_target ${candidate} ALIASED_TARGET)
            if(_aliased_target)
                set(_runtime_target "${_aliased_target}")
            else()
                set(_runtime_target "${candidate}")
            endif()

            add_dependencies(${target} ${_runtime_target})

            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "$<TARGET_FILE:${_runtime_target}>"
                        "$<TARGET_FILE_DIR:${target}>"
                COMMENT "Copying runtime dependency $<TARGET_FILE_NAME:${_runtime_target}> for ${target}"
                VERBATIM)

            return()
        endif()
    endforeach()

    message(WARNING "Could not find any runtime dependency target for ${target}: ${ARGN}")
endfunction()

function(whip_copy_openal_runtime_after_build target)
    if(NOT WIN32)
        return()
    endif()

    whip_copy_runtime_target_after_build(${target}
        OpenAL
        OpenAL32
        OpenAL::OpenAL
        whip::openal)
endfunction()
