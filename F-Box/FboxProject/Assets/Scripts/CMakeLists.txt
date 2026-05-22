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
