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

### 2. Asset Pipeline

This is one of the most-used editor areas and should stay polished while export/runtime work starts.

- Polish the texture and sprite editors.
- Finalize sprite slicing metadata.
- Improve sub-sprite management:
  - expand/collapse under parent texture
  - batch delete
  - rename
  - reorder where useful
  - clear all slices
- Improve multi-selection in the content browser.
- Support multi drag/drop into scenes and animation tools.
- Add "Create Animation From Selected Sprites" workflows.
- Harden missing asset and deleted file behavior.
- Continue fixing texture bleeding, padding, atlas preview, and import settings.
- Keep import settings visible and predictable for texture/sprite assets.

### 3. Editor UX And Panel System

This keeps the editor feeling professional instead of like a pile of useful tools.

- Polish panel layout, docking, floating, minimize, fullscreen, and restore behavior.
- Do one final pass over shortcut focus, scope, conflicts, and settings UX.
- Improve hierarchy multi-select.
- Improve hierarchy duplicate, delete, rename, grouping, and reordering.
- Fix inspector text overflow and narrow-panel layout issues.
- Continue console/filter/log UX polish.
- Improve command palette and global actions.
- Keep panel base class and panel manager patterns consistent.

### 4. Animation Runtime Control

The animation editor is in a strong place, but runtime behavior must be just as clear.

- Finalize animator parameter API.
- Validate transition runtime behavior.
- Add warnings for missing states, clips, controllers, and invalid sprite indices.
- Keep animation preview behavior consistent with runtime behavior.
- Add controller debug view:
  - current state
  - active transition
  - parameter values
  - transition condition results
- Add script-facing animation helper APIs where needed.

### 5. Physics And Character Feel

This is critical for actually making games that feel good.

- Write and enforce a Rigidbody2D usage pattern.
- Add a character controller helper.
- Standardize jump and ground detection.
- Add collider setup tools.
- Add platformer movement examples.
- Add recommended settings for dynamic, kinematic, gravity scale, friction, and jump feel.
- Add scene tools that make collider authoring less manual.

### 6. Validation System

Validation gives the editor a big quality jump and prevents broken exports.

- Add a Project Health / Scene Validator panel.
- Detect missing scripts.
- Detect missing assets and textures.
- Detect invalid sprite indices.
- Detect broken animator controllers.
- Detect duplicate or invalid UUIDs.
- Detect suspicious transform scale.
- Detect build, script reload, and hot reload diagnostics.
- Show actionable fixes where possible.
- Gate export with warnings/errors that the user can inspect.

### 7. Project And Templates

This improves the first-run and new-project experience.

- Polish starter templates.
- Ensure default scenes and starter entities are correct.
- Move sample/test projects out of the engine/editor repo path or keep them isolated from source control.
- Finalize project creation.
- Finalize project open/recent project behavior.
- Add template metadata:
  - platformer
  - empty 2D
  - UI sample
  - animation sample
  - physics sample

### 8. Platform Abstraction Foundation

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

### 9. Input And Cursor Runtime System

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

### 10. Save / Load Runtime System

This is needed for real games and especially mobile.

- Add platform-specific persistent data paths.
- Add simple `PlayerPrefs` style key/value storage.
- Add save slots.
- Add JSON save files first.
- Add binary save support later if needed.
- Add scene/entity state save hooks.
- Add script-facing save/load API.
- Add editor settings for default save behavior.

### 11. Runtime UI System

Editor ImGui is not a game UI solution. Whip needs a runtime UI layer.

- Add Canvas.
- Add Image, Text, Button, Toggle, Slider, Panel.
- Add anchors and basic layout.
- Add screen-space UI first.
- Add world-space UI later.
- Connect UI events to mouse/touch input.
- Add script callbacks for UI interaction.
- Add UI asset/component editing in the editor.

### 12. Renderer OpenGL ES Readiness

Android needs OpenGL ES or Vulkan. OpenGL ES is the shortest path for a 2D engine.

- Separate desktop OpenGL assumptions from renderer abstractions.
- Add renderer capability queries.
- Add shader profile/dialect handling for desktop GL vs GLES.
- Audit framebuffer, texture formats, uniform buffers, blending, and line rendering for GLES compatibility.
- Avoid OpenGL 4.5-only calls in runtime paths that need Android.
- Add a backend selection path even if only OpenGL is implemented at first.
- Prepare an `OpenGLES` backend or compatibility layer.

### 13. Android Player / Export Prototype

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

### 14. Linux Desktop Port

Linux is not the main product target, but it is a useful portability check after the platform layer exists.

- Add Linux window/input/platform utils.
- Add CMake presets for Linux.
- Verify OpenGL backend on Linux.
- Replace or wrap Windows-only editor/process/file-dialog paths.

### 15. Long-Term Renderer Backends

Do this after shipping/export foundations are real.

- Vulkan as the first serious cross-platform modern backend.
- DirectX 12 only if Windows-specific performance or tooling becomes important.
- Metal or MoltenVK only when macOS/iOS becomes a real target.

## Recommended Execution Order

1. Build / Export Pipeline And Whip Player Runtime.
2. Asset Pipeline.
3. Editor UX And Panel System.
4. Animation Runtime Control.
5. Physics And Character Feel.
6. Validation System.
7. Project And Templates.
8. Platform Abstraction Foundation.
9. Input And Cursor Runtime System.
10. Save / Load Runtime System.
11. Runtime UI System.
12. Renderer OpenGL ES Readiness.
13. Android Player / Export Prototype.
14. Linux Desktop Port.
15. Long-term renderer backend work.

## First Implementation Milestone

Start with the Windows export path:

1. Rename or repurpose F-Box into `Whip-Player`.
2. Make `Whip-Player` accept a project/package path.
3. Make it open the configured start scene.
4. Add editor build settings.
5. Add `Build` and `Build And Run`.
6. Export a playable Windows folder.

This gives Whip a real "make game -> ship game" loop before Android work begins.
