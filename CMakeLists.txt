cmake_minimum_required(VERSION 3.25)

# Some third-party dependencies still declare very old cmake_minimum_required
# versions. CMake 4.x rejects compatibility with versions below 3.5 unless this
# cache variable is set before those projects are added.
if(CMAKE_VERSION VERSION_GREATER_EQUAL 4.0)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Minimum policy version for old third-party CMake projects" FORCE)
endif()

# Whip's old Premake project had Debug / Release / Dist.
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Dist" CACHE STRING "Available configurations" FORCE)

# Keep Visual Studio / Rider solution output readable.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake")
set(BUILD_TESTING OFF CACHE BOOL "Disable third-party tests by default" FORCE)

function(whip_set_folder_if_exists target folder)
    if(TARGET ${target})
        set_target_properties(${target} PROPERTIES FOLDER "${folder}")
    endif()
endfunction()

function(whip_apply_solution_folders)
    whip_set_folder_if_exists(Whip "Whip/Engine")
    whip_set_folder_if_exists(Whip-Assistant "Whip/Assistant")
    whip_set_folder_if_exists(Whip-Editor "Whip/Editor")
    whip_set_folder_if_exists(Whip-ScriptCore "Whip/Scripts")
    whip_set_folder_if_exists(Whip-Player "Whip/Player")

    foreach(target IN ITEMS
        glfw glfw3 update_mappings
        spdlog yaml-cpp yaml-cpp-parse yaml-cpp-read yaml-cpp-sandbox
        box2d
        ogg vorbis vorbisenc vorbisfile
        OpenAL common ex-common build_version
        msdf-atlas-gen msdfgen-core msdfgen-ext msdfgen msdfgen-standalone artery-font-format
        freetype zlib libpng bzip2 brotlicommon brotlidec brotlienc
        whip_glm whip_entt whip_filewatch whip_coco whip_nps whip_frenum whip_mono whip_shader_tools whip_vorbis)
        whip_set_folder_if_exists(${target} "Additionals")
    endforeach()

    foreach(target IN ITEMS test_bitwise test_framing test_resample test_seeking test_streams)
        whip_set_folder_if_exists(${target} "Additionals/Tests")
    endforeach()
endfunction()

# Bootstrap must run before project() so dependencies that require a toolchain
# file, such as vcpkg-backed msdf-atlas-gen/Freetype, can configure correctly.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(WhipOptions)
include(WhipBootstrap)
whip_run_bootstrap()

project(Whip VERSION 0.1.0 LANGUAGES C CXX)

include(WhipOutput)
include(WhipCompiler)
include(WhipDependencies)

if(WHP_SUPPRESS_EXTERNAL_CMAKE_WARNINGS)
    set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "Suppress CMake deprecation warnings from third-party dependency projects." FORCE)
endif()

whip_configure_output_directories()
whip_resolve_dependencies()

add_subdirectory(Whip)
add_subdirectory(Whip-Assistant)

if(WHP_BUILD_SCRIPT_CORE)
    if(CMAKE_GENERATOR MATCHES "Visual Studio")
        enable_language(CSharp)
        add_subdirectory(Whip-ScriptCore)
    else()
        message(WARNING "WHP_BUILD_SCRIPT_CORE is ON, but CSharp targets are only enabled for Visual Studio generators. Native targets will still configure.")
    endif()
endif()

if(WHP_BUILD_EDITOR)
    add_subdirectory(Whip-Editor)
endif()

if(WHP_BUILD_PLAYER)
    add_subdirectory(F-Box)
endif()

whip_apply_solution_folders()
