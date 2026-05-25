file(GLOB_RECURSE FBOX_SCRIPT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cs")

add_library(FboxScripts SHARED ${FBOX_SCRIPT_SOURCES})

set_target_properties(FboxScripts PROPERTIES
    OUTPUT_NAME "Fbox"
    DOTNET_TARGET_FRAMEWORK_VERSION "v4.7.2"
    VS_DOTNET_REFERENCES "System"
    VS_GLOBAL_AllowUnsafeBlocks "true"
    VS_GLOBAL_LangVersion "latest")

if(TARGET Whip-ScriptCore)
    target_link_libraries(FboxScripts PRIVATE Whip-ScriptCore)
    add_dependencies(FboxScripts Whip-ScriptCore)
endif()

foreach(cfg Debug Release Dist)
    string(TOUPPER "${cfg}" cfg_upper)
    set_target_properties(FboxScripts PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${cfg_upper} "${CMAKE_CURRENT_SOURCE_DIR}/Binaries"
        LIBRARY_OUTPUT_DIRECTORY_${cfg_upper} "${CMAKE_CURRENT_SOURCE_DIR}/Binaries")
endforeach()

function(fbox_sync_script_binaries_after_build runtime_target)
    set(destination "${WHP_OUTPUT_ROOT}/$<CONFIG>-${WHP_SYSTEM}-${WHP_ARCH}/${runtime_target}/FboxProject/Assets/Scripts/Binaries")
    add_custom_command(TARGET FboxScripts POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${destination}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:FboxScripts>"
                "${destination}/$<TARGET_FILE_NAME:FboxScripts>"
        COMMENT "Syncing Fbox script binaries for ${runtime_target}"
        VERBATIM)
endfunction()

if(WHP_BUILD_EDITOR)
    fbox_sync_script_binaries_after_build(Whip-Editor)
endif()

if(WHP_BUILD_FBOX)
    fbox_sync_script_binaries_after_build(F-Box)
endif()
