include_guard(GLOBAL)
include(FetchContent)

# CMake 4.x warns about FetchContent_Populate for source-only dependencies
# such as ImGui/ImGuizmo. We intentionally use it there because those
# repositories do not provide stable top-level CMake targets for Whip.
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

set(WHP_DEP_IMGUI_TAG "v1.92.8-docking" CACHE STRING "Pinned Dear ImGui docking tag used by Whip.")
set(WHP_DEP_IMGUIZMO_TAG "1.10" CACHE STRING "Pinned ImGuizmo tag used by Whip.")

function(whip_create_alias alias_name target_name)
    if(TARGET ${alias_name})
        return()
    endif()

    if(NOT TARGET ${target_name})
        message(FATAL_ERROR "Cannot create ${alias_name}; target ${target_name} does not exist.")
    endif()

    # CMake does not allow creating an ALIAS to another ALIAS. Some packages
    # expose namespace targets like spdlog::spdlog_header_only as aliases, so
    # unwrap them before creating Whip's stable alias.
    get_target_property(_whip_aliased_target ${target_name} ALIASED_TARGET)
    if(_whip_aliased_target)
        set(target_name ${_whip_aliased_target})
    endif()

    add_library(${alias_name} ALIAS ${target_name})
endfunction()

function(whip_alias_first_existing alias_name)
    foreach(candidate ${ARGN})
        if(TARGET ${candidate})
            whip_create_alias(${alias_name} ${candidate})
            return()
        endif()
    endforeach()
    message(FATAL_ERROR "Could not create alias ${alias_name}; none of these targets exist: ${ARGN}")
endfunction()

function(whip_header_only_target target include_dir)
    if(NOT TARGET ${target})
        add_library(${target} INTERFACE)
        target_include_directories(${target} INTERFACE "${include_dir}")
    endif()
endfunction()

function(whip_fetch name repo tag)
    if(NOT WHP_FETCH_DEPS)
        message(FATAL_ERROR "Missing dependency ${name}. Enable WHP_FETCH_DEPS or place it under Whip/vendor.")
    endif()
    FetchContent_Declare(${name}
        GIT_REPOSITORY ${repo}
        GIT_TAG        ${tag}
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(${name})
endfunction()

function(whip_fetch_source name repo tag out_var)
    string(TOLOWER "${name}" lc_name)
    if(NOT WHP_FETCH_DEPS)
        message(FATAL_ERROR "Missing dependency ${name}. Enable WHP_FETCH_DEPS or restore its local vendor source.")
    endif()
    FetchContent_Declare(${lc_name}
        GIT_REPOSITORY ${repo}
        GIT_TAG        ${tag}
        GIT_SHALLOW    TRUE)
    FetchContent_GetProperties(${lc_name})
    if(NOT ${lc_name}_POPULATED)
        FetchContent_Populate(${lc_name})
    endif()
    set(${out_var} "${${lc_name}_SOURCE_DIR}" PARENT_SCOPE)
endfunction()

function(whip_fetch_header name repo tag local_subdir required_relative_header out_var)
    string(TOLOWER "${name}" lc_name)
    set(local_dir "${CMAKE_SOURCE_DIR}/Whip/vendor/${local_subdir}")
    if(WHP_USE_LOCAL_VENDOR AND EXISTS "${local_dir}/${required_relative_header}")
        set(${out_var} "${local_dir}" PARENT_SCOPE)
        return()
    endif()

    whip_fetch_source(${lc_name} ${repo} ${tag} fetched_dir)
    set(${out_var} "${fetched_dir}" PARENT_SCOPE)
endfunction()

function(whip_add_spdlog)
    if(TARGET whip::spdlog)
        return()
    endif()

    if(WHP_USE_LOCAL_VENDOR AND EXISTS "${CMAKE_SOURCE_DIR}/Whip/vendor/spdlog/include/spdlog/spdlog.h")
        whip_header_only_target(whip_spdlog "${CMAKE_SOURCE_DIR}/Whip/vendor/spdlog/include")
        whip_create_alias(whip::spdlog whip_spdlog)
        return()
    endif()

    set(SPDLOG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)
    whip_fetch(spdlog https://github.com/gabime/spdlog.git v1.15.1)
    if(TARGET spdlog::spdlog_header_only)
        whip_create_alias(whip::spdlog spdlog::spdlog_header_only)
    elseif(TARGET spdlog::spdlog)
        whip_create_alias(whip::spdlog spdlog::spdlog)
    else()
        whip_alias_first_existing(whip::spdlog spdlog)
    endif()
endfunction()

function(whip_resolve_source_dir name repo tag local_subdir required_relative_file cache_var)
    set(local_dir "${CMAKE_SOURCE_DIR}/Whip/vendor/${local_subdir}")
    if(WHP_USE_LOCAL_VENDOR AND EXISTS "${local_dir}/${required_relative_file}")
        set(resolved_dir "${local_dir}")
    else()
        whip_fetch_source(${name} ${repo} ${tag} resolved_dir)
    endif()

    set(${cache_var} "${resolved_dir}" CACHE PATH "Resolved source directory for ${name}" FORCE)
endfunction()

function(whip_resolve_imguizmo)
    set(local_dir "${CMAKE_SOURCE_DIR}/Whip/vendor/ImGuizmo")
    set(resolved_root "")

    if(WHP_USE_LOCAL_VENDOR AND (EXISTS "${local_dir}/ImGuizmo.cpp" OR EXISTS "${local_dir}/src/ImGuizmo.cpp"))
        set(resolved_root "${local_dir}")
    else()
        whip_fetch_source(imguizmo https://github.com/CedricGuillemet/ImGuizmo.git ${WHP_DEP_IMGUIZMO_TAG} resolved_root)
    endif()

    if(EXISTS "${resolved_root}/src/ImGuizmo.cpp")
        set(resolved_source_dir "${resolved_root}/src")
    elseif(EXISTS "${resolved_root}/ImGuizmo.cpp")
        set(resolved_source_dir "${resolved_root}")
    else()
        message(FATAL_ERROR "ImGuizmo was resolved to ${resolved_root}, but ImGuizmo.cpp was not found at the repository root or under src/.")
    endif()

    set(WHP_IMGUIZMO_ROOT "${resolved_root}" CACHE PATH "Resolved ImGuizmo root directory" FORCE)
    set(WHP_IMGUIZMO_DIR "${resolved_source_dir}" CACHE PATH "Resolved ImGuizmo source/include directory" FORCE)
endfunction()

function(whip_prepare_internal_vendor_sources)
    # These are Whip-internal vendor/wrapper sources. They should be compiled
    # into the Whip engine target, not emitted as separate Visual Studio/Rider
    # projects. Keeping them internal fixes PCH/include propagation and keeps
    # the solution tree readable.

    set(glad_dir "${CMAKE_SOURCE_DIR}/Whip/vendor/Glad")
    if(NOT EXISTS "${glad_dir}/src/glad.c")
        message(FATAL_ERROR "Glad is generated API-loader source. Keep Whip/vendor/Glad in the repository. It is tiny and should not be deleted.")
    endif()
    set(WHP_GLAD_DIR "${glad_dir}" CACHE PATH "Resolved Glad source directory" FORCE)

    set(alhelpers_dir "${CMAKE_SOURCE_DIR}/Whip/vendor/alhelpers")
    if(NOT EXISTS "${alhelpers_dir}/alhelpers.cpp")
        message(FATAL_ERROR "alhelpers is a local Whip/OpenAL helper. Keep Whip/vendor/alhelpers in the repository.")
    endif()
    set(WHP_ALHELPERS_DIR "${alhelpers_dir}" CACHE PATH "Resolved alhelpers source directory" FORCE)

    whip_resolve_source_dir(imgui https://github.com/ocornut/imgui.git ${WHP_DEP_IMGUI_TAG} imgui imgui.cpp WHP_IMGUI_DIR)
    whip_resolve_imguizmo()

    # stb/miniaudio/minimp3 are single-header libraries. If the old local
    # implementation .cpp exists, use it. Otherwise generate the tiny build
    # translation unit under the build directory.
    set(stb_dir "${CMAKE_SOURCE_DIR}/Whip/vendor/stb_image")
    if(WHP_USE_LOCAL_VENDOR AND EXISTS "${stb_dir}/stb_image.cpp")
        set(WHP_STB_IMAGE_DIR "${stb_dir}" CACHE PATH "Resolved stb_image include directory" FORCE)
        set(WHP_STB_IMAGE_SOURCE "${stb_dir}/stb_image.cpp" CACHE FILEPATH "Resolved stb_image implementation source" FORCE)
    else()
        whip_fetch_source(stb https://github.com/nothings/stb.git master fetched_stb_dir)
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")
        set(generated_stb_source "${CMAKE_BINARY_DIR}/generated/stb_image.cpp")
        file(WRITE "${generated_stb_source}" "#define STB_IMAGE_IMPLEMENTATION\n#include <stb_image.h>\n")
        set(WHP_STB_IMAGE_DIR "${fetched_stb_dir}" CACHE PATH "Resolved stb_image include directory" FORCE)
        set(WHP_STB_IMAGE_SOURCE "${generated_stb_source}" CACHE FILEPATH "Resolved stb_image implementation source" FORCE)
    endif()

    set(miniaudio_dir "${CMAKE_SOURCE_DIR}/Whip/vendor/miniaudio")
    if(WHP_USE_LOCAL_VENDOR AND EXISTS "${miniaudio_dir}/miniaudio_build.cpp")
        set(WHP_MINIAUDIO_DIR "${miniaudio_dir}" CACHE PATH "Resolved miniaudio include directory" FORCE)
        set(WHP_MINIAUDIO_SOURCE "${miniaudio_dir}/miniaudio_build.cpp" CACHE FILEPATH "Resolved miniaudio implementation source" FORCE)
    else()
        whip_fetch_source(miniaudio https://github.com/mackron/miniaudio.git master fetched_miniaudio_dir)
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")
        set(generated_miniaudio_source "${CMAKE_BINARY_DIR}/generated/miniaudio_build.cpp")
        file(WRITE "${generated_miniaudio_source}" "#define MINIAUDIO_IMPLEMENTATION\n#include <miniaudio.h>\n")
        set(WHP_MINIAUDIO_DIR "${fetched_miniaudio_dir}" CACHE PATH "Resolved miniaudio include directory" FORCE)
        set(WHP_MINIAUDIO_SOURCE "${generated_miniaudio_source}" CACHE FILEPATH "Resolved miniaudio implementation source" FORCE)
    endif()

    set(minimp3_dir "${CMAKE_SOURCE_DIR}/Whip/vendor/minimp3")
    if(WHP_USE_LOCAL_VENDOR AND EXISTS "${minimp3_dir}/minimp3_build.cpp")
        set(WHP_MINIMP3_DIR "${minimp3_dir}" CACHE PATH "Resolved minimp3 include directory" FORCE)
        set(WHP_MINIMP3_SOURCE "${minimp3_dir}/minimp3_build.cpp" CACHE FILEPATH "Resolved minimp3 implementation source" FORCE)
    else()
        whip_fetch_source(minimp3 https://github.com/lieff/minimp3.git master fetched_minimp3_dir)
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")
        set(generated_minimp3_source "${CMAKE_BINARY_DIR}/generated/minimp3_build.cpp")
        file(WRITE "${generated_minimp3_source}" "#define MINIMP3_IMPLEMENTATION\n#define MINIMP3_ONLY_MP3\n#include <minimp3.h>\n#include <minimp3_ex.h>\n")
        set(WHP_MINIMP3_DIR "${fetched_minimp3_dir}" CACHE PATH "Resolved minimp3 include directory" FORCE)
        set(WHP_MINIMP3_SOURCE "${generated_minimp3_source}" CACHE FILEPATH "Resolved minimp3 implementation source" FORCE)
    endif()
endfunction()

function(whip_add_shader_tools)
    if(TARGET whip_shader_tools)
        return()
    endif()

    if(NOT WHP_ENABLE_SHADER_COMPILER)
        add_library(whip_shader_tools INTERFACE)
        whip_create_alias(whip::shader_tools whip_shader_tools)
        return()
    endif()

    if(NOT WHP_VULKAN_SDK_ROOT)
        message(FATAL_ERROR "WHP_ENABLE_SHADER_COMPILER is ON, but Vulkan SDK was not found. Run scripts/bootstrap.ps1 or install KhronosGroup.VulkanSDK.")
    endif()

    file(TO_CMAKE_PATH "${WHP_VULKAN_SDK_ROOT}" _vulkan_root)
    set(vulkan_include "${_vulkan_root}/Include")
    set(vulkan_lib "${_vulkan_root}/Lib")

    find_path(WHP_SHADERC_INCLUDE_DIR shaderc/shaderc.hpp PATHS "${vulkan_include}" NO_DEFAULT_PATH)
    find_path(WHP_SPIRV_CROSS_INCLUDE_DIR spirv_cross/spirv_cross.hpp PATHS "${vulkan_include}" NO_DEFAULT_PATH)

    find_library(WHP_SHADERC_DEBUG_LIBRARY NAMES shaderc_sharedd shaderc_combinedd shadercd PATHS "${vulkan_lib}" NO_DEFAULT_PATH)
    find_library(WHP_SHADERC_RELEASE_LIBRARY NAMES shaderc_shared shaderc_combined shaderc PATHS "${vulkan_lib}" NO_DEFAULT_PATH)
    find_library(WHP_SPIRV_CROSS_CORE_DEBUG_LIBRARY NAMES spirv-cross-cored spirv-cross-core PATHS "${vulkan_lib}" NO_DEFAULT_PATH)
    find_library(WHP_SPIRV_CROSS_CORE_RELEASE_LIBRARY NAMES spirv-cross-core PATHS "${vulkan_lib}" NO_DEFAULT_PATH)
    find_library(WHP_SPIRV_CROSS_GLSL_DEBUG_LIBRARY NAMES spirv-cross-glsld spirv-cross-glsl PATHS "${vulkan_lib}" NO_DEFAULT_PATH)
    find_library(WHP_SPIRV_CROSS_GLSL_RELEASE_LIBRARY NAMES spirv-cross-glsl PATHS "${vulkan_lib}" NO_DEFAULT_PATH)

    foreach(required_var
        WHP_SHADERC_INCLUDE_DIR
        WHP_SPIRV_CROSS_INCLUDE_DIR
        WHP_SHADERC_RELEASE_LIBRARY
        WHP_SPIRV_CROSS_CORE_RELEASE_LIBRARY
        WHP_SPIRV_CROSS_GLSL_RELEASE_LIBRARY)
        if(NOT ${required_var})
            message(FATAL_ERROR "Could not find ${required_var} in Vulkan SDK: ${_vulkan_root}")
        endif()
    endforeach()

    if(NOT WHP_SHADERC_DEBUG_LIBRARY)
        set(WHP_SHADERC_DEBUG_LIBRARY "${WHP_SHADERC_RELEASE_LIBRARY}")
        message(WARNING "Vulkan debug shaderc library not found; using release shaderc library in Debug.")
    endif()
    if(NOT WHP_SPIRV_CROSS_CORE_DEBUG_LIBRARY)
        set(WHP_SPIRV_CROSS_CORE_DEBUG_LIBRARY "${WHP_SPIRV_CROSS_CORE_RELEASE_LIBRARY}")
    endif()
    if(NOT WHP_SPIRV_CROSS_GLSL_DEBUG_LIBRARY)
        set(WHP_SPIRV_CROSS_GLSL_DEBUG_LIBRARY "${WHP_SPIRV_CROSS_GLSL_RELEASE_LIBRARY}")
    endif()

    add_library(whip_shader_tools INTERFACE)
    target_include_directories(whip_shader_tools INTERFACE "${vulkan_include}")
    target_link_libraries(whip_shader_tools INTERFACE
        $<$<CONFIG:Debug>:${WHP_SHADERC_DEBUG_LIBRARY};${WHP_SPIRV_CROSS_CORE_DEBUG_LIBRARY};${WHP_SPIRV_CROSS_GLSL_DEBUG_LIBRARY}>
        $<$<NOT:$<CONFIG:Debug>>:${WHP_SHADERC_RELEASE_LIBRARY};${WHP_SPIRV_CROSS_CORE_RELEASE_LIBRARY};${WHP_SPIRV_CROSS_GLSL_RELEASE_LIBRARY}>)
    whip_create_alias(whip::shader_tools whip_shader_tools)
endfunction()

function(whip_add_mono)
    if(TARGET whip_mono)
        return()
    endif()

    if(NOT WHP_ENABLE_MONO)
        add_library(whip_mono INTERFACE)
        whip_create_alias(whip::mono whip_mono)
        return()
    endif()

    if(WHP_MONO_INCLUDE_DIR AND EXISTS "${WHP_MONO_INCLUDE_DIR}/mono/jit/jit.h")
        set(mono_include "${WHP_MONO_INCLUDE_DIR}")
    else()
        if(WHP_MONO_ROOT)
            set(mono_root "${WHP_MONO_ROOT}")
        else()
            set(mono_root "${CMAKE_SOURCE_DIR}/Whip/vendor/mono")
        endif()

        if(EXISTS "${mono_root}/include/mono/jit/jit.h")
            set(mono_include "${mono_root}/include")
        elseif(EXISTS "${mono_root}/include/mono-2.0/mono/jit/jit.h")
            set(mono_include "${mono_root}/include/mono-2.0")
        else()
            set(mono_include "")
        endif()
    endif()

    if(WHP_MONO_LIBRARY_RELEASE)
        set(mono_release_lib "${WHP_MONO_LIBRARY_RELEASE}")
    else()
        find_library(mono_release_lib
            NAMES libmono-static-sgen mono-2.0-sgen libmono-2.0-sgen mono-2.0 libmono-2.0 monosgen-2.0
            PATHS
                "${WHP_MONO_ROOT}/lib"
                "${WHP_MONO_ROOT}/lib/Release"
                "${CMAKE_SOURCE_DIR}/Whip/vendor/mono/lib/Release"
                "${CMAKE_SOURCE_DIR}/Whip/vendor/mono/lib"
            NO_DEFAULT_PATH)
    endif()

    if(WHP_MONO_LIBRARY_DEBUG)
        set(mono_debug_lib "${WHP_MONO_LIBRARY_DEBUG}")
    else()
        find_library(mono_debug_lib
            NAMES libmono-static-sgen mono-2.0-sgend libmono-2.0-sgend mono-2.0-sgen libmono-2.0-sgen mono-2.0 libmono-2.0 monosgen-2.0
            PATHS
                "${WHP_MONO_ROOT}/lib/Debug"
                "${WHP_MONO_ROOT}/lib"
                "${CMAKE_SOURCE_DIR}/Whip/vendor/mono/lib/Debug"
                "${CMAKE_SOURCE_DIR}/Whip/vendor/mono/lib/Release"
                "${CMAKE_SOURCE_DIR}/Whip/vendor/mono/lib"
            NO_DEFAULT_PATH)
    endif()

    if(NOT mono_debug_lib)
        set(mono_debug_lib "${mono_release_lib}")
    endif()

    if(NOT mono_include OR NOT mono_release_lib)
        message(FATAL_ERROR "Mono SDK was not found. Run scripts/bootstrap.ps1, install Mono.Mono, or set WHP_MONO_ROOT/WHP_MONO_INCLUDE_DIR/WHP_MONO_LIBRARY_RELEASE.")
    endif()

    add_library(whip_mono INTERFACE)
    target_include_directories(whip_mono INTERFACE "${mono_include}")
    target_link_libraries(whip_mono INTERFACE
        $<$<CONFIG:Debug>:${mono_debug_lib}>
        $<$<NOT:$<CONFIG:Debug>>:${mono_release_lib}>)
    whip_create_alias(whip::mono whip_mono)
endfunction()

function(whip_add_gemini_cpp)
    if(TARGET whip::gemini_cpp)
        return()
    endif()

    if(NOT WHP_ENABLE_GEMINI_CPP)
        add_library(whip_gemini_cpp INTERFACE)
        whip_create_alias(whip::gemini_cpp whip_gemini_cpp)
        return()
    endif()

    set(GEMINI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(CURL_ZLIB OFF CACHE STRING "" FORCE)

    if(WHP_USE_LOCAL_VENDOR AND EXISTS "${CMAKE_SOURCE_DIR}/Whip/vendor/Gemini-cpp/CMakeLists.txt")
        add_subdirectory("${CMAKE_SOURCE_DIR}/Whip/vendor/Gemini-cpp" "${CMAKE_BINARY_DIR}/vendor/Gemini-cpp")
    elseif(WHP_FETCH_DEPS)
        FetchContent_Declare(gemini_cpp
            GIT_REPOSITORY https://github.com/tynes0/Gemini-cpp.git
            GIT_TAG        87a79e10bd47013838f5263f079c5f225775cde6
            GIT_SHALLOW    FALSE)
        FetchContent_MakeAvailable(gemini_cpp)
    else()
        message(FATAL_ERROR "Gemini-cpp SDK is required for Whip Assistant Gemini support. Enable WHP_FETCH_DEPS, disable WHP_ENABLE_GEMINI_CPP, or place it under Whip/vendor/Gemini-cpp.")
    endif()

    whip_alias_first_existing(whip::gemini_cpp gemini-cpp gemini-core)
endfunction()

function(whip_resolve_dependencies)
    # Keep all CMake-fetched dependencies under build/_deps. Source checkout stays clean.
    # Third-party targets are grouped under Additionals by default.
    set(FETCHCONTENT_QUIET OFF CACHE BOOL "" FORCE)
    if(WHP_UPDATE_DEPS)
        set(FETCHCONTENT_UPDATES_DISCONNECTED OFF CACHE BOOL "" FORCE)
    else()
        set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "" FORCE)
    endif()
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(CMAKE_FOLDER "Additionals")

    whip_prepare_internal_vendor_sources()

    if(WHP_USE_LOCAL_VENDOR AND EXISTS "${CMAKE_SOURCE_DIR}/Whip/vendor/GLFW/CMakeLists.txt")
        set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
        add_subdirectory("${CMAKE_SOURCE_DIR}/Whip/vendor/GLFW" "${CMAKE_BINARY_DIR}/vendor/GLFW")
    elseif(WHP_FETCH_DEPS)
        set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
        whip_fetch(glfw https://github.com/glfw/glfw.git 3.4)
    endif()
    whip_alias_first_existing(whip::glfw glfw glfw3)

    whip_add_spdlog()
    if(WHP_USE_LOCAL_VENDOR AND EXISTS "${CMAKE_SOURCE_DIR}/Whip/vendor/yaml-cpp/CMakeLists.txt")
        set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
        set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
        add_subdirectory("${CMAKE_SOURCE_DIR}/Whip/vendor/yaml-cpp" "${CMAKE_BINARY_DIR}/vendor/yaml-cpp")
    elseif(WHP_FETCH_DEPS)
        set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
        set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
        whip_fetch(yaml-cpp https://github.com/jbeder/yaml-cpp.git 0.8.0)
    endif()
    if(TARGET yaml-cpp::yaml-cpp)
        whip_create_alias(whip::yaml_cpp yaml-cpp::yaml-cpp)
    else()
        whip_alias_first_existing(whip::yaml_cpp yaml-cpp)
    endif()

    if(WHP_USE_LOCAL_VENDOR AND EXISTS "${CMAKE_SOURCE_DIR}/Whip/vendor/box2d/CMakeLists.txt")
        set(BOX2D_BUILD_TESTBED OFF CACHE BOOL "" FORCE)
        set(BOX2D_BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
        set(BOX2D_BUILD_DOCS OFF CACHE BOOL "" FORCE)
        add_subdirectory("${CMAKE_SOURCE_DIR}/Whip/vendor/box2d" "${CMAKE_BINARY_DIR}/vendor/box2d")
    elseif(WHP_FETCH_DEPS)
        set(BOX2D_BUILD_TESTBED OFF CACHE BOOL "" FORCE)
        set(BOX2D_BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
        set(BOX2D_BUILD_DOCS OFF CACHE BOOL "" FORCE)
        whip_fetch(box2d https://github.com/erincatto/box2d.git v2.4.1)
    endif()
    whip_alias_first_existing(whip::box2d box2d)

    set(_whip_openal_soft_dir "")
    if(WIN32)
        set(ALSOFT_BACKEND_PIPEWIRE OFF CACHE BOOL "" FORCE)
        set(ALSOFT_BACKEND_PULSEAUDIO OFF CACHE BOOL "" FORCE)
        set(ALSOFT_BACKEND_JACK OFF CACHE BOOL "" FORCE)
        set(ALSOFT_BACKEND_OPENSL OFF CACHE BOOL "" FORCE)
        set(ALSOFT_BACKEND_PORTAUDIO OFF CACHE BOOL "" FORCE)
    endif()

    if(WHP_USE_LOCAL_VENDOR AND EXISTS "${CMAKE_SOURCE_DIR}/Whip/vendor/OpenAL-Soft/CMakeLists.txt")
        set(ALSOFT_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(ALSOFT_TESTS OFF CACHE BOOL "" FORCE)
        set(ALSOFT_UTILS OFF CACHE BOOL "" FORCE)
        set(ALSOFT_INSTALL OFF CACHE BOOL "" FORCE)
        set(_whip_openal_soft_dir "${CMAKE_SOURCE_DIR}/Whip/vendor/OpenAL-Soft")
        add_subdirectory("${_whip_openal_soft_dir}" "${CMAKE_BINARY_DIR}/vendor/OpenAL-Soft")
    elseif(WHP_FETCH_DEPS)
        set(ALSOFT_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(ALSOFT_TESTS OFF CACHE BOOL "" FORCE)
        set(ALSOFT_UTILS OFF CACHE BOOL "" FORCE)
        set(ALSOFT_INSTALL OFF CACHE BOOL "" FORCE)
        whip_fetch(openal-soft https://github.com/kcat/openal-soft.git 1.23.1)
        FetchContent_GetProperties(openal-soft SOURCE_DIR _whip_openal_soft_dir)
    endif()
    if(_whip_openal_soft_dir)
        set(WHP_OPENAL_SOFT_DIR "${_whip_openal_soft_dir}" CACHE PATH "Resolved OpenAL-Soft source directory" FORCE)
    endif()
    whip_alias_first_existing(whip::openal OpenAL OpenAL::OpenAL OpenAL32)

    set(INSTALL_DOCS OFF CACHE BOOL "" FORCE)
    set(INSTALL_PKG_CONFIG_MODULE OFF CACHE BOOL "" FORCE)

    if(WHP_USE_LOCAL_VENDOR AND EXISTS "${CMAKE_SOURCE_DIR}/Whip/vendor/libogg/CMakeLists.txt")
        add_subdirectory("${CMAKE_SOURCE_DIR}/Whip/vendor/libogg" "${CMAKE_BINARY_DIR}/vendor/libogg")
    elseif(WHP_FETCH_DEPS)
        whip_fetch(ogg https://github.com/xiph/ogg.git v1.3.5)
    endif()
    if(TARGET Ogg::ogg)
        whip_create_alias(whip::ogg Ogg::ogg)
    else()
        whip_alias_first_existing(whip::ogg ogg libogg)
    endif()

    # xiph/vorbis uses its own FindOgg.cmake and expects the classic
    # OGG_INCLUDE_DIR / OGG_LIBRARY variables. When Ogg is brought in through
    # FetchContent/add_subdirectory, it exists as a CMake target but is not
    # installed into a prefix yet, so Vorbis cannot discover it automatically.
    # Seed those variables before configuring Vorbis.
    FetchContent_GetProperties(ogg)
    if(ogg_SOURCE_DIR AND EXISTS "${ogg_SOURCE_DIR}/include/ogg/ogg.h")
        set(OGG_INCLUDE_DIR "${ogg_SOURCE_DIR}/include" CACHE PATH "Ogg include directory for fetched Vorbis" FORCE)
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/Whip/vendor/libogg/include/ogg/ogg.h")
        set(OGG_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/Whip/vendor/libogg/include" CACHE PATH "Ogg include directory for local Vorbis" FORCE)
    endif()

    if(TARGET Ogg::ogg)
        set(OGG_LIBRARY Ogg::ogg CACHE STRING "Ogg target for fetched Vorbis" FORCE)
    elseif(TARGET ogg)
        set(OGG_LIBRARY ogg CACHE STRING "Ogg target for fetched Vorbis" FORCE)
    elseif(TARGET libogg)
        set(OGG_LIBRARY libogg CACHE STRING "Ogg target for fetched Vorbis" FORCE)
    endif()

    if(WHP_USE_LOCAL_VENDOR AND EXISTS "${CMAKE_SOURCE_DIR}/Whip/vendor/Vorbis/CMakeLists.txt")
        add_subdirectory("${CMAKE_SOURCE_DIR}/Whip/vendor/Vorbis" "${CMAKE_BINARY_DIR}/vendor/Vorbis")
    elseif(WHP_FETCH_DEPS)
        whip_fetch(vorbis https://github.com/xiph/vorbis.git v1.3.7)
    endif()
    add_library(whip_vorbis INTERFACE)
    foreach(candidate vorbis Vorbis::vorbis)
        if(TARGET ${candidate})
            target_link_libraries(whip_vorbis INTERFACE ${candidate})
        endif()
    endforeach()
    foreach(candidate vorbisfile Vorbis::vorbisfile)
        if(TARGET ${candidate})
            target_link_libraries(whip_vorbis INTERFACE ${candidate})
        endif()
    endforeach()
    target_link_libraries(whip_vorbis INTERFACE whip::ogg)
    whip_create_alias(whip::vorbis whip_vorbis)

    if(WHP_VCPKG_ROOT)
        file(TO_CMAKE_PATH "${WHP_VCPKG_ROOT}" _whip_vcpkg_root)
        set(ENV{VCPKG_ROOT} "${_whip_vcpkg_root}")
    endif()

    set(MSDF_ATLAS_BUILD_STANDALONE OFF CACHE BOOL "" FORCE)
    set(MSDF_ATLAS_USE_SKIA OFF CACHE BOOL "" FORCE)
    set(MSDF_ATLAS_NO_ARTERY_FONT ON CACHE BOOL "" FORCE)
    set(MSDF_ATLAS_DYNAMIC_RUNTIME ON CACHE BOOL "" FORCE)
    set(MSDFGEN_DISABLE_PNG ON CACHE BOOL "" FORCE)
    set(MSDFGEN_DISABLE_SVG ON CACHE BOOL "" FORCE)

    set(_whip_msdf_atlas_gen_dir "")
    if(WHP_USE_LOCAL_VENDOR AND EXISTS "${CMAKE_SOURCE_DIR}/Whip/vendor/msdf-atlas-gen/CMakeLists.txt")
        set(_whip_msdf_atlas_gen_dir "${CMAKE_SOURCE_DIR}/Whip/vendor/msdf-atlas-gen")
        add_subdirectory("${_whip_msdf_atlas_gen_dir}" "${CMAKE_BINARY_DIR}/vendor/msdf-atlas-gen")
    elseif(WHP_FETCH_DEPS)
        if(NOT DEFINED ENV{VCPKG_ROOT})
            message(FATAL_ERROR "msdf-atlas-gen requires vcpkg/Freetype. Run scripts/bootstrap.ps1 or set WHP_VCPKG_ROOT/VCPKG_ROOT.")
        endif()
        set(MSDF_ATLAS_USE_VCPKG ON CACHE BOOL "" FORCE)
        whip_fetch(msdf-atlas-gen https://github.com/Chlumsky/msdf-atlas-gen.git v1.3)
        FetchContent_GetProperties(msdf-atlas-gen SOURCE_DIR _whip_msdf_atlas_gen_dir)
    endif()
    if(_whip_msdf_atlas_gen_dir)
        set(WHP_MSDF_ATLAS_GEN_DIR "${_whip_msdf_atlas_gen_dir}" CACHE PATH "Resolved msdf-atlas-gen source directory" FORCE)
    endif()
    if(TARGET msdf-atlas-gen)
        whip_create_alias(whip::msdf_atlas_gen msdf-atlas-gen)
    else()
        message(FATAL_ERROR "msdf-atlas-gen target was not found. Check the fetched msdf-atlas-gen CMake target name.")
    endif()

    whip_fetch_header(glm https://github.com/g-truc/glm.git 1.0.1 glm glm/glm.hpp WHP_GLM_DIR)
    whip_header_only_target(whip_glm "${WHP_GLM_DIR}")
    whip_create_alias(whip::glm whip_glm)

    whip_fetch_header(entt https://github.com/skypjack/entt.git v3.13.2 entt include/entt.hpp WHP_ENTT_DIR)
    if(NOT TARGET whip_entt)
        add_library(whip_entt INTERFACE)
    endif()

    # Whip's code includes <entt.hpp>. Older local vendor drops usually place
    # that file directly under include/, while upstream EnTT places it under
    # src/entt/entt.hpp or include/entt/entt.hpp. Add both the canonical include
    # root and the compatibility directory so existing Whip includes keep working.
    if(EXISTS "${WHP_ENTT_DIR}/src/entt/entt.hpp")
        target_include_directories(whip_entt INTERFACE
            "${WHP_ENTT_DIR}/src"
            "${WHP_ENTT_DIR}/src/entt")
    elseif(EXISTS "${WHP_ENTT_DIR}/include/entt.hpp")
        target_include_directories(whip_entt INTERFACE
            "${WHP_ENTT_DIR}/include")
    elseif(EXISTS "${WHP_ENTT_DIR}/include/entt/entt.hpp")
        target_include_directories(whip_entt INTERFACE
            "${WHP_ENTT_DIR}/include"
            "${WHP_ENTT_DIR}/include/entt")
    else()
        target_include_directories(whip_entt INTERFACE "${WHP_ENTT_DIR}")
    endif()
    whip_create_alias(whip::entt whip_entt)

    # These are tiny/header-only in this repo. Keep them local if possible.
    whip_fetch_header(filewatch https://github.com/ThomasMonkman/filewatch.git master filewatch FileWatch.h WHP_FILEWATCH_DIR)
    whip_header_only_target(whip_filewatch "${WHP_FILEWATCH_DIR}")
    whip_create_alias(whip::filewatch whip_filewatch)

    whip_fetch_header(coco https://github.com/tynes0/coco.git main coco coco.h WHP_COCO_DIR)
    whip_header_only_target(whip_coco "${WHP_COCO_DIR}")
    whip_create_alias(whip::coco whip_coco)

    whip_fetch_header(nps https://github.com/tynes0/nps.git main nps nps_formatter.h WHP_NPS_DIR)
    whip_header_only_target(whip_nps "${WHP_NPS_DIR}")
    whip_create_alias(whip::nps whip_nps)

    whip_fetch_header(frenum https://github.com/tynes0/frenum.git main frenum frenum.h WHP_FRENUM_DIR)
    whip_header_only_target(whip_frenum "${WHP_FRENUM_DIR}")
    whip_create_alias(whip::frenum whip_frenum)

    whip_add_shader_tools()
    whip_add_mono()
    whip_add_gemini_cpp()

    find_package(OpenGL REQUIRED)

    if(COMMAND whip_apply_solution_folders)
        whip_apply_solution_folders()
    endif()
endfunction()
