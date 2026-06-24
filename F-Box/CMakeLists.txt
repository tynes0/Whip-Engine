set(WHIP_PLAYER_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/WhipPlayerApp.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/WhipPlayerLayer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/WhipPlayerLayer.h")

add_executable(Whip-Player ${WHIP_PLAYER_SOURCES})

whip_set_target_output(Whip-Player)
whip_configure_native_target(Whip-Player)

target_include_directories(Whip-Player PRIVATE
    "${CMAKE_SOURCE_DIR}/Whip/include"
    "${CMAKE_SOURCE_DIR}/Whip/src"
    "${CMAKE_SOURCE_DIR}/Whip/vendor"
    "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_link_libraries(Whip-Player PRIVATE whip::engine)
whip_suppress_external_link_warnings(Whip-Player)

if(TARGET Whip-ScriptCore)
    add_dependencies(Whip-Player Whip-ScriptCore)
    add_custom_command(TARGET Whip-Player POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Whip-Player>/Resources/Scripts"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:Whip-ScriptCore>"
            "$<TARGET_FILE_DIR:Whip-Player>/Resources/Scripts/$<TARGET_FILE_NAME:Whip-ScriptCore>"
        COMMENT "Copying Whip-ScriptCore for Whip-Player"
        VERBATIM)
endif()

set_target_properties(Whip-Player PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:Whip-Player>")

whip_copy_directory_after_build(Whip-Player "${CMAKE_CURRENT_SOURCE_DIR}/assets" "assets")
whip_copy_directory_after_build(Whip-Player "${CMAKE_CURRENT_SOURCE_DIR}/Resources" "Resources")
whip_copy_mono_runtime_after_build(Whip-Player)
whip_copy_openal_runtime_after_build(Whip-Player)
