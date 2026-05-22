# Whip Engine

Whip is a Windows-first C++ game engine/editor with a Mono-based C# scripting layer.

## First Build On A Fresh Windows Machine

Install CMake, clone the repository, then run one CMake script-mode command from the repository root:

```powershell
cmake -DWHIP_BOOTSTRAP_MODE=Auto -DWHIP_ASSUME_YES=ON -DWHIP_PRESET=vs2022-auto -P cmake/WhipFirstBuild.cmake
```

That command runs the bootstrap step before CMake selects the Visual Studio generator. It can install or prepare:

- Visual Studio 2022 Build Tools with C++ and .NET Framework 4.7.2 support
- Vulkan SDK
- Mono for Windows
- vcpkg and the Freetype package needed by `msdf-atlas-gen`
- Git, when dependencies must be fetched

For an interactive run that asks before installing missing prerequisites:

```powershell
cmake -P cmake/WhipFirstBuild.cmake
```

## Normal Developer Build

Once prerequisites are present:

```powershell
cmake --preset vs2022
cmake --build --preset vs2022-debug
```

The main editor executable is written under:

```text
bin/Debug-windows-x86_64/Whip-Editor/Whip-Editor.exe
```

## Dependency Policy

Large machine-local SDKs and generated dependency checkouts are not committed. Bootstrap/CMake discovers or installs them instead.

Ignored local dependency/output roots include:

- `.whip/`
- `build/`
- `bin/`
- `bin-int/`
- `vendor/`
- `libs/`
- `vendor/VulkanSDK/`
- `libs/mono/`
- `Whip/vendor/mono/`

Small Whip-specific helper libraries under `Whip/vendor/` stay tracked.
