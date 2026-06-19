file(GLOB_RECURSE FBOX_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_executable(F-Box ${FBOX_SOURCES})

whip_set_target_output(F-Box)
whip_configure_native_target(F-Box)

target_include_directories(F-Box PRIVATE
    "${CMAKE_SOURCE_DIR}/Whip/include"
    "${CMAKE_SOURCE_DIR}/Whip/src"
    "${CMAKE_SOURCE_DIR}/Whip/vendor"
    "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_link_libraries(F-Box PRIVATE whip::engine)
whip_suppress_external_link_warnings(F-Box)

if(TARGET Whip-ScriptCore)
    add_dependencies(F-Box Whip-ScriptCore)
endif()

set_target_properties(F-Box PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:F-Box>")

whip_copy_directory_after_build(F-Box "${CMAKE_CURRENT_SOURCE_DIR}/assets" "assets")
whip_copy_directory_after_build(F-Box "${CMAKE_CURRENT_SOURCE_DIR}/Resources" "Resources")
whip_copy_mono_runtime_after_build(F-Box)
whip_copy_openal_runtime_after_build(F-Box)
