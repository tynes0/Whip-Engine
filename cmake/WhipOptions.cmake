include_guard(GLOBAL)

option(WHP_BUILD_EDITOR "Build Whip-Editor." ON)
option(WHP_BUILD_FBOX "Build F-Box sandbox application." ON)
option(WHP_BUILD_SCRIPT_CORE "Build Whip-ScriptCore C# assembly. Requires a Visual Studio generator." ON)
option(WHP_BUILD_FBOX_SCRIPTS "Build F-Box C# gameplay assembly. Requires Whip-ScriptCore and a Visual Studio generator." ON)

option(WHP_USE_LOCAL_VENDOR "Prefer dependency sources already present under Whip/vendor." ON)
option(WHP_FETCH_DEPS "Fetch missing open-source dependencies with CMake FetchContent." ON)
option(WHP_UPDATE_DEPS "Allow FetchContent dependencies to update existing git checkouts during configure." OFF)

option(WHP_ENABLE_MONO "Enable Mono scripting support. Requires Mono headers and a compatible Mono import/static library." ON)
option(WHP_ENABLE_OPENGL "Enable OpenGL renderer backend." ON)
option(WHP_ENABLE_SHADER_COMPILER "Use shaderc/SPIRV-Cross from the Vulkan SDK for runtime shader compilation." ON)
option(WHP_SUPPRESS_EXTERNAL_LINK_WARNINGS "Suppress known MSVC linker warning noise from third-party binary/static dependencies." ON)
option(WHP_SUPPRESS_EXTERNAL_CMAKE_WARNINGS "Suppress noisy CMake deprecation warnings emitted by third-party dependency projects." ON)

option(WHP_BOOTSTRAP_DEPS "Run scripts/bootstrap.ps1 from CMake configure to check/install host prerequisites." ON)
option(WHP_BOOTSTRAP_ASSUME_YES "Allow CMake bootstrap to install missing prerequisites without prompting." OFF)
set(WHP_BOOTSTRAP_MODE "Prompt" CACHE STRING "Bootstrap mode: Off, CheckOnly, Prompt, Auto")
set_property(CACHE WHP_BOOTSTRAP_MODE PROPERTY STRINGS Off CheckOnly Prompt Auto)

set(WHP_CXX_STANDARD "20" CACHE STRING "C++ standard used by Whip native targets.")
set_property(CACHE WHP_CXX_STANDARD PROPERTY STRINGS 20 23)

set(WHP_MONO_ROOT "" CACHE PATH "Optional Mono SDK root. Official Windows Mono is usually C:/Program Files/Mono.")
set(WHP_MONO_INCLUDE_DIR "" CACHE PATH "Optional Mono include directory that contains mono/jit/jit.h.")
set(WHP_MONO_LIBRARY_DEBUG "" CACHE FILEPATH "Optional Debug Mono library.")
set(WHP_MONO_LIBRARY_RELEASE "" CACHE FILEPATH "Optional Release Mono library.")
set(WHP_MONO_DLL "" CACHE FILEPATH "Optional Mono runtime DLL to copy next to executables.")
set(WHP_MONO_RUNTIME_LIB_DIR "" CACHE PATH "Optional Mono runtime library directory, usually <MonoRoot>/lib/mono.")

set(WHP_VULKAN_SDK_ROOT "$ENV{VULKAN_SDK}" CACHE PATH "Optional Vulkan SDK root. Defaults to the VULKAN_SDK environment variable.")
