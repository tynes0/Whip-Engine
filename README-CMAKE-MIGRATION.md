# Whip CMake Bootstrap

For a clean Windows machine with only CMake available, run the script-mode first build. It performs bootstrap before CMake has to select the Visual Studio generator:

```powershell
cmake -DWHIP_BOOTSTRAP_MODE=Auto -DWHIP_ASSUME_YES=ON -DWHIP_PRESET=vs2022-auto -P cmake/WhipFirstBuild.cmake
```

For day-to-day development after prerequisites are installed, use:

```powershell
cmake --preset vs2022
cmake --build --preset vs2022-debug
```

On Windows, `cmake --preset vs2022` runs `scripts/bootstrap.ps1` during configure. The bootstrap script checks the host tools and SDKs Whip needs:

- Visual Studio 2022 Build Tools with C++ and .NET Framework 4.7.2 targeting components
- Vulkan SDK
- Mono for Windows
- Git and CMake when the script is run manually

The script never silently installs dependencies in the default preset. Missing tools are installed only after confirmation. For CI or a throwaway fresh machine that should install missing prerequisites without prompts, use:

```powershell
cmake --preset vs2022-auto
cmake --build --preset vs2022-auto-debug
```

For a dry check with no installs:

```powershell
cmake --preset vs2022-check
```

For a totally clean Windows machine where even CMake might not exist yet, run the bootstrap script first:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/bootstrap.ps1 -Mode Prompt
```

Then run CMake normally.

## Dependency policy

Huge SDK folders should not be committed:

- `vendor/VulkanSDK/`
- `libs/mono/`
- `Whip/vendor/mono/`

Small local helper libraries should stay committed because they are custom or generated:

- `Whip/vendor/Glad`
- `Whip/vendor/alhelpers`
- `Whip/vendor/coco`
- `Whip/vendor/nps`
- `Whip/vendor/frenum`
- `Whip/vendor/miniaudio`
- `Whip/vendor/minimp3`
- `Whip/vendor/stb_image`
- `Whip/vendor/filewatch`

Large third-party dependencies are fetched by CMake under `build/_deps` when missing. Dependencies that come from git are pinned by tag where practical, and existing checkouts are not updated during normal configure. To intentionally refresh fetched dependency checkouts, use:

```powershell
cmake --preset vs2022-refresh-deps
cmake --build --preset vs2022-refresh-deps-debug
```

## Runtime Mono layout

The engine code currently calls:

```cpp
mono_set_assemblies_path("mono/lib");
assembly_manager::load_assembly("Resources/Scripts/Whip-ScriptCore.dll");
```

So the CMake build copies Mono runtime assemblies into each executable output directory as:

```text
<target dir>/mono/lib/mono/...
```

and copies `Resources` next to the executable.

## Useful commands

```powershell
# Full build, prompt before installing missing host dependencies
cmake --preset vs2022
cmake --build --preset vs2022-debug

# Full build, install missing host dependencies automatically
cmake --preset vs2022-auto
cmake --build --preset vs2022-auto-debug

# Native-only build
cmake --preset vs2022-native
cmake --build --preset vs2022-native-debug

# Script wrapper
scripts\build.bat -Preset vs2022 -Config Debug -Bootstrap
```

## Notes

The first build can take a while because it may install host tools, clone CMake dependencies, build vcpkg packages, and copy the Mono runtime beside the editor executable.
