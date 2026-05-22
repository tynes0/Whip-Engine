file(GLOB_RECURSE WHIP_NATIVE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

set(WHIP_INTERNAL_VENDOR_SOURCES
    "${WHP_STB_IMAGE_SOURCE}"
    "${WHP_MINIAUDIO_SOURCE}"
    "${WHP_MINIMP3_SOURCE}"
    "${WHP_ALHELPERS_DIR}/alhelpers.cpp"
    "${WHP_GLAD_DIR}/src/glad.c"
    "${WHP_IMGUI_DIR}/imgui.cpp"
    "${WHP_IMGUI_DIR}/imgui_draw.cpp"
    "${WHP_IMGUI_DIR}/imgui_tables.cpp"
    "${WHP_IMGUI_DIR}/imgui_widgets.cpp"
    "${WHP_IMGUI_DIR}/imgui_demo.cpp"
    "${WHP_IMGUIZMO_DIR}/ImGuizmo.cpp")

add_library(Whip STATIC
    ${WHIP_NATIVE_SOURCES}
    ${WHIP_INTERNAL_VENDOR_SOURCES})
add_library(whip::engine ALIAS Whip)

whip_set_target_output(Whip)
whip_configure_native_target(Whip)

target_precompile_headers(Whip PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/whippch.h")

target_include_directories(Whip
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/vendor"
        "${WHP_IMGUI_DIR}"
        "${WHP_IMGUI_DIR}/backends"
        "${WHP_IMGUI_DIR}/misc/cpp"
        "${WHP_IMGUIZMO_DIR}"
        "${WHP_GLAD_DIR}/include"
        "${WHP_STB_IMAGE_DIR}"
        "${WHP_MINIAUDIO_DIR}"
        "${WHP_MINIMP3_DIR}"
        "${WHP_ALHELPERS_DIR}"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Whip")

set(WHIP_OPTIONAL_PUBLIC_INCLUDE_DIRS)
foreach(dir IN ITEMS
    "${WHP_OPENAL_SOFT_DIR}"
    "${WHP_OPENAL_SOFT_DIR}/include"
    "${WHP_OPENAL_SOFT_DIR}/src"
    "${WHP_OPENAL_SOFT_DIR}/src/common"
    "${WHP_OPENAL_SOFT_DIR}/common"
    "${WHP_MSDF_ATLAS_GEN_DIR}/msdf-atlas-gen"
    "${WHP_MSDF_ATLAS_GEN_DIR}/msdfgen"
    "${WHP_MSDF_ATLAS_GEN_DIR}/msdfgen/include")
    if(dir AND EXISTS "${dir}")
        list(APPEND WHIP_OPTIONAL_PUBLIC_INCLUDE_DIRS "${dir}")
    endif()
endforeach()

if(WHIP_OPTIONAL_PUBLIC_INCLUDE_DIRS)
    target_include_directories(Whip PUBLIC ${WHIP_OPTIONAL_PUBLIC_INCLUDE_DIRS})
endif()

target_compile_definitions(Whip
    PUBLIC
        _CRT_SECURE_NO_WARNINGS
        IMGUI_DEFINE_MATH_OPERATORS
        GLFW_INCLUDE_NONE
        AL_LIBTYPE_STATIC
        $<$<BOOL:${WHP_ENABLE_MONO}>:WHP_ENABLE_MONO>
        $<$<BOOL:${WHP_ENABLE_OPENGL}>:WHP_ENABLE_OPENGL>)

target_link_libraries(Whip
    PUBLIC
        whip::spdlog
        whip::glm
        whip::entt
        whip::filewatch
        whip::coco
        whip::nps
        whip::frenum
        whip::yaml_cpp
        whip::box2d
        whip::openal
        whip::ogg
        whip::vorbis
        whip::msdf_atlas_gen
        whip::mono
        whip::shader_tools
        whip::glfw
        OpenGL::GL)

if(WIN32)
    target_link_libraries(Whip PUBLIC Ws2_32 Winmm Version Bcrypt)
endif()

# Engine-internal third-party sources live under the Whip target to avoid
# noisy solution projects, but they should still appear in readable source
# groups inside IDEs.
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" PREFIX "Engine" FILES ${WHIP_NATIVE_SOURCES})
source_group("Internal Vendor" FILES ${WHIP_INTERNAL_VENDOR_SOURCES})

# ImGui/ImGuizmo/alhelpers/Glad are external code. Do not force Whip's PCH on
# sources that do not include whippch.h themselves.
foreach(src IN LISTS WHIP_INTERNAL_VENDOR_SOURCES)
    if(src MATCHES "[/\\](imgui|imguizmo|ImGuizmo|alhelpers|Glad|miniaudio|minimp3|stb_image)[/\\]" OR src MATCHES "[/\\]generated[/\\]" OR src MATCHES "[/\\]ImGuizmo\\.cpp$")
        set_source_files_properties("${src}" PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
    endif()
endforeach()
