file(GLOB_RECURSE WHIP_EDITOR_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_executable(Whip-Editor ${WHIP_EDITOR_SOURCES})

whip_set_target_output(Whip-Editor)
whip_configure_native_target(Whip-Editor)

target_compile_definitions(Whip-Editor PRIVATE _CRT_SECURE_NO_WARNINGS)

target_include_directories(Whip-Editor PRIVATE
    "${CMAKE_SOURCE_DIR}/Whip/src"
    "${CMAKE_SOURCE_DIR}/Whip/vendor"
    "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_link_libraries(Whip-Editor PRIVATE whip::engine)
whip_suppress_external_link_warnings(Whip-Editor)

if(TARGET Whip-ScriptCore)
    add_dependencies(Whip-Editor Whip-ScriptCore)
endif()
if(TARGET FboxScripts)
    add_dependencies(Whip-Editor FboxScripts)
endif()

set_target_properties(Whip-Editor PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:Whip-Editor>")

whip_copy_directory_after_build(Whip-Editor "${CMAKE_CURRENT_SOURCE_DIR}/assets" "assets")
whip_copy_directory_after_build(Whip-Editor "${CMAKE_CURRENT_SOURCE_DIR}/Resources" "Resources")
whip_copy_directory_after_build(Whip-Editor "${CMAKE_SOURCE_DIR}/Whip-ScriptCore/Source" "Resources/Scripts/Whip-ScriptCore/Source")
whip_copy_directory_after_build(Whip-Editor "${CMAKE_SOURCE_DIR}/F-Box/FboxProject" "FboxProject")
whip_copy_mono_runtime_after_build(Whip-Editor)
whip_copy_openal_runtime_after_build(Whip-Editor)
