include_guard(GLOBAL)

function(whip_configure_native_target target)
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD ${WHP_CXX_STANDARD}
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO)

    target_compile_definitions(${target} PRIVATE
        $<$<CONFIG:Debug>:WHP_DEBUG>
        $<$<CONFIG:Release>:WHP_RELEASE>
        $<$<CONFIG:Dist>:WHP_DIST>)

    if(MSVC)
        target_compile_options(${target} PRIVATE /MP /permissive- /Zc:__cplusplus)
        set_property(TARGET ${target} PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

function(whip_suppress_external_link_warnings target)
    if(NOT WHP_SUPPRESS_EXTERNAL_LINK_WARNINGS)
        return()
    endif()

    if(MSVC)
        target_link_options(${target} PRIVATE
            /IGNORE:4098
            /IGNORE:4099)
    endif()
endfunction()
