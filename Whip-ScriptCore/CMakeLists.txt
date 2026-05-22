file(GLOB_RECURSE WHIP_SCRIPT_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cs"
    "${CMAKE_CURRENT_SOURCE_DIR}/Properties/*.cs")

add_library(Whip-ScriptCore SHARED ${WHIP_SCRIPT_CORE_SOURCES})

set_target_properties(Whip-ScriptCore PROPERTIES
    DOTNET_TARGET_FRAMEWORK_VERSION "v4.7.2"
    VS_DOTNET_REFERENCES "System"
    VS_GLOBAL_AllowUnsafeBlocks "true"
    VS_GLOBAL_LangVersion "latest")

foreach(cfg Debug Release Dist)
    string(TOUPPER "${cfg}" cfg_upper)
    set_target_properties(Whip-ScriptCore PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${cfg_upper} "${WHP_OUTPUT_ROOT}/${cfg}-${WHP_SYSTEM}-${WHP_ARCH}/Whip-Editor/Resources/Scripts"
        LIBRARY_OUTPUT_DIRECTORY_${cfg_upper} "${WHP_OUTPUT_ROOT}/${cfg}-${WHP_SYSTEM}-${WHP_ARCH}/Whip-Editor/Resources/Scripts")
endforeach()
