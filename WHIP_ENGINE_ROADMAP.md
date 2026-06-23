# Whip Engine Roadmap

This note captures the engine/editor/runtime work after the first stability pass.

## Current Baseline

- Primary supported development target is Windows x64.
- Primary renderer backend is OpenGL 4.5.
- Editor is the main executable for authoring projects.
- There is no complete end-user game export pipeline yet.
- Android is a desired product target, especially for 2D games.
- The old F-Box test project/target should be repurposed into the first Whip Player runtime instead of remaining as an unrelated sample game.

## Completed Checkpoint

### 0. Stability Pass

- Hardened editor shortcut focus and shortcut registration.
- Hardened runtime stop/shutdown paths.
- Hardened script reload around Mono domain reload.
- Improved console auto-scroll and log copying behavior.

## Priority Roadmap

### 1. Build / Export Pipeline And Whip Player Runtime

This is the highest priority because it answers the core user question: "I made a game, now how do I ship it?"

- Convert the old F-Box target into `Whip-Player`.
- Keep `Whip-Player` runtime-only: no editor panels, no editor-only dependencies.
- Load a project or packaged manifest from command-line arguments.
- Load the configured start scene automatically.
- Build scripts before export and copy the script outputs.
- Copy or package runtime assets, engine resources, Mono runtime files, and script assemblies.
- Add editor build settings:
  - target platform
  - build configuration
  - output folder
  - app name
  - icon
  - start scene
  - resolution/window mode
  - debug logs
  - script debugging
- Add `Build` and `Build And Run` commands.
- First milestone output:
  - `Builds/Windows/<ProjectName>/<ProjectName>.exe`
  - copied `Assets`
  - copied `Resources`
  - copied script binaries
  - copied runtime dependencies

### 2. Platform Abstraction Foundation

This unlocks Windows/Linux/Android work without spreading platform checks through gameplay systems.

- Add stable interfaces for:
  - window creation and native handles
  - input backend
  - cursor backend
  - file dialogs
  - process launch
  - clipboard
  - external app/path opening
  - save-data path
  - user/app data path
- Keep Windows as the first implementation.
- Move Windows-only logic out of generic engine/editor code where possible.
- Make unsupported platforms fail with clear build/runtime diagnostics.

### 3. Input And Cursor Runtime System

This is needed before Android and before polished desktop games.

- Add a unified pointer API:
  - mouse
  - touch
  - multi-touch
  - pointer id
  - pointer position
  - pointer delta
  - press/release/held
- Keep keyboard and gamepad ready for the same action system later.
- Add cursor controls:
  - show/hide
  - lock/unlock
  - confine
  - set cursor image/state
  - platform fallback cursor shapes
- Add runtime input mapping for game actions.

### 4. Save / Load Runtime System

This is needed for real games and especially mobile.

- Add platform-specific persistent data paths.
- Add simple `PlayerPrefs` style key/value storage.
- Add save slots.
- Add JSON save files first.
- Add binary save support later if needed.
- Add scene/entity state save hooks.
- Add script-facing save/load API.
- Add editor settings for default save behavior.

### 5. Runtime UI System

Editor ImGui is not a game UI solution. Whip needs a runtime UI layer.

- Add Canvas.
- Add Image, Text, Button, Toggle, Slider, Panel.
- Add anchors and basic layout.
- Add screen-space UI first.
- Add world-space UI later.
- Connect UI events to mouse/touch input.
- Add script callbacks for UI interaction.
- Add UI asset/component editing in the editor.

### 6. Renderer OpenGL ES Readiness

Android needs OpenGL ES or Vulkan. OpenGL ES is the shortest path for a 2D engine.

- Separate desktop OpenGL assumptions from renderer abstractions.
- Add renderer capability queries.
- Add shader profile/dialect handling for desktop GL vs GLES.
- Audit framebuffer, texture formats, uniform buffers, blending, and line rendering for GLES compatibility.
- Avoid OpenGL 4.5-only calls in runtime paths that need Android.
- Add a backend selection path even if only OpenGL is implemented at first.
- Prepare an `OpenGLES` backend or compatibility layer.

### 7. Android Player / Export Prototype

This should come after the export pipeline, platform abstraction, input, save paths, and GLES readiness have a first pass.

- Add Android player target.
- Add Gradle project generation or checked-in Android shell project.
- Create GLES context on Android.
- Load packaged assets from APK/AAB.
- Map Android lifecycle events to Whip runtime:
  - pause
  - resume
  - surface lost/restored
  - app close
- Add touch input backend.
- Add persistent storage path.
- Add orientation, app icon, splash, and manifest settings.
- Produce first runnable APK.

### 8. Linux Desktop Port

Linux is not the main product target, but it is a useful portability check after the platform layer exists.

- Add Linux window/input/platform utils.
- Add CMake presets for Linux.
- Verify OpenGL backend on Linux.
- Replace or wrap Windows-only editor/process/file-dialog paths.

### 9. Long-Term Renderer Backends

Do this after shipping/export foundations are real.

- Vulkan as the first serious cross-platform modern backend.
- DirectX 12 only if Windows-specific performance or tooling becomes important.
- Metal or MoltenVK only when macOS/iOS becomes a real target.

## Recommended Execution Order

1. Build / Export Pipeline And Whip Player Runtime.
2. Platform Abstraction Foundation.
3. Input And Cursor Runtime System.
4. Save / Load Runtime System.
5. Runtime UI System.
6. Renderer OpenGL ES Readiness.
7. Android Player / Export Prototype.
8. Linux Desktop Port.
9. Long-term renderer backend work.

## First Implementation Milestone

Start with the Windows export path:

1. Rename or repurpose F-Box into `Whip-Player`.
2. Make `Whip-Player` accept a project/package path.
3. Make it open the configured start scene.
4. Add editor build settings.
5. Add `Build` and `Build And Run`.
6. Export a playable Windows folder.

This gives Whip a real "make game -> ship game" loop before Android work begins.
