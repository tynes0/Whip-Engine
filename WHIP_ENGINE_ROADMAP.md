# Whip Engine Roadmap

This note captures the engine/editor/runtime work after the first stability pass and the first authoring/runtime foundation passes.

## Current Baseline

- Primary supported development target is Windows x64.
- Primary renderer backend is OpenGL 4.5.
- Editor is the main executable for authoring projects.
- A first Windows player/export path exists, but packaging polish and platform expansion are still active roadmap work.
- Android is a desired product target, especially for 2D games.
- `Whip-Player` is the first runtime player target.
- The editor now separates authoring and gameplay presentation through Scene View and Game View.
- Runtime UI exists as a first-pass component system and now needs deeper UX, layout, and event polish.

## Current Roadmap Position

Whip is currently between the first Windows authoring/runtime foundation pass and the next UI/input/platform pass.

- Done enough to build on:
  - Windows editor stability pass.
  - Windows player/export first pass.
  - Asset pipeline first pass.
  - Project Health / validation first pass.
  - Runtime UI component first pass.
  - Scene View / Game View separation first pass.
- Current active focus:
  - Make UI authoring feel like a real game engine workflow.
  - Harden Game View, resolution presets, and runtime input behavior.
  - Continue polishing editor UX and panel/layout consistency.
- Next major engineering arc:
  - Platform abstraction.
  - Input/cursor/pointer system.
  - Runtime UI editor and canvas workflow.
  - Save/load.
  - OpenGL ES readiness.
  - Android player/export prototype.

## Completed Checkpoint

### 0. Stability Pass

- Hardened editor shortcut focus and shortcut registration.
- Hardened runtime stop/shutdown paths.
- Hardened script reload around Mono domain reload.
- Improved console auto-scroll and log copying behavior.

### 2. Asset Pipeline Authoring Pass

- Added safer texture sprite metadata normalization.
- Added missing asset registry cleanup from the content browser.
- Added multi-asset drag/drop payloads for content browser selections.
- Added multi-sprite viewport drop placement.
- Added selected sprite-slice batch removal.
- Improved create-animation-from-selection for parent spritesheets and sub-sprites.
- Improved texture editor sprite management with normalize, duplicate, and reorder controls.

### 3. Build / Export And Player First Pass

- Added the first `Whip-Player` runtime target.
- Added build/export panel foundations.
- Added Windows build configuration/output controls.
- Added project health gate integration for export.
- Added script build and runtime dependency copy flow for exported builds.
- Added debug/release-oriented export settings groundwork.

### 4. Validation First Pass

- Added Project Health panel.
- Added scene/project validation checks for common broken states.
- Added export gating hooks for validation errors and warnings.
- Added shortcuts for physics collider visibility and editor grid visibility.

### 5. Runtime UI First Pass

- Added runtime UI components:
  - UI Transform
  - Canvas
  - Image
  - Text
  - Button
  - Stack Layout
- Added UI runtime rendering and basic interaction flow.
- Added inspector editing for UI components.
- Added default text alignment controls for text and buttons.
- Added UI gizmo behavior based on UI Transform instead of world Transform.

### 6. Scene View / Game View First Pass

- Split editor presentation into:
  - Scene View: editor camera, grid, gizmos, picking, drag/drop, debug overlays.
  - Game View: primary camera output, runtime UI, runtime input target.
- Added separate scene and game framebuffers.
- Added Game View resolution presets:
  - Free Aspect
  - HD / Full HD
  - Steam Deck
  - iPhone presets
  - Galaxy S24 presets
  - Pixel / iPad presets
- Routed runtime cursor behavior through Game View instead of Scene View.
- Kept entity picking tied to Scene View only.

## Priority Roadmap

### 1. Build / Export Pipeline And Whip Player Runtime

Status: first pass implemented, polish and packaging hardening remain.

This is still important because it answers the core user question: "I made a game, now how do I ship it?"

- Keep hardening `Whip-Player` as runtime-only: no editor panels, no editor-only dependencies.
- Harden project/package loading from command-line arguments.
- Harden automatic configured start scene loading.
- Harden script build before export and script output copy.
- Harden copy/package flow for runtime assets, engine resources, Mono runtime files, and script assemblies.
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

Status: first pass implemented, ongoing polish.

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

Status: partially implemented, still active.

This keeps the editor feeling professional instead of like a pile of useful tools.

- Polish panel layout, docking, floating, minimize, fullscreen, and restore behavior.
- Keep Scene View and Game View distinct:
  - Scene View for authoring.
  - Game View for primary camera output, runtime input, and device presets.
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
- Add an adjustable CapsuleCollider2D component for characters and rounded platformer bodies:
  - configurable size, radius, offset, direction, density, friction, restitution, sensor, and tag
  - runtime Box2D fixture creation/update
  - scene serialization, inspector editing, multi-select editing, script API, debug draw, and validation
- Add collider setup tools.
- Add platformer movement examples.
- Add recommended settings for dynamic, kinematic, gravity scale, friction, and jump feel.
- Add scene tools that make collider authoring less manual.

### 6. Validation System

Status: first pass implemented, ongoing expansion.

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

Status: viewport cursor split first pass implemented, unified pointer/action system still pending.

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
- Keep Game View as the canonical runtime input surface in editor play mode.
- Add device-aware input testing for mobile presets.

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

Status: first pass implemented, now entering real authoring/editor workflow phase.

Editor ImGui is not a game UI solution. Whip needs a runtime UI layer.

- Harden Canvas, Image, Text, Button, and layout behavior.
- Add Toggle, Slider, Panel, Input Field, and layout containers.
- Improve anchors, pivot, stretch, layout, scaling, safe area, and resolution behavior.
- Continue screen-space UI first.
- Add world-space UI later.
- Connect UI events to mouse/touch input.
- Add script callbacks for UI interaction.
- Add UI asset/component editing in the editor.
- Add a proper UI authoring mode:
  - canvas outline
  - selected UI bounds
  - UI-specific gizmos
  - responsive preview
  - Game View preset preview
  - safe area overlays for phones

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

1. Finish Runtime UI authoring polish around Canvas, Game View presets, safe areas, and UI event behavior.
2. Finalize Scene View / Game View UX, including toolbar polish and device preview workflows.
3. Finish shortcut/focus/layout polish in editor panels.
4. Expand input and cursor runtime system into a real pointer/action abstraction.
5. Add Save / Load runtime system.
6. Start Platform Abstraction Foundation.
7. Prepare Renderer OpenGL ES readiness.
8. Build Android Player / Export prototype.
9. Continue Build / Export hardening for Windows packaging.
10. Continue Asset Pipeline polish as needed.
11. Animation Runtime Control debug/validation pass.
12. Physics And Character Feel, including CapsuleCollider2D.
13. Project/Templates polish.
14. Linux Desktop Port.
15. Long-term renderer backend work.

## First Implementation Milestone

Status: first pass completed.

The first milestone was the Windows export path:

1. Rename or repurpose F-Box into `Whip-Player`.
2. Make `Whip-Player` accept a project/package path.
3. Make it open the configured start scene.
4. Add editor build settings.
5. Add `Build` and `Build And Run`.
6. Export a playable Windows folder.

This gives Whip a real "make game -> ship game" loop before Android work begins.

## Next Implementation Milestone

Move from "runtime UI exists" to "runtime UI can be authored comfortably":

1. Create a clearer UI authoring mode on top of Scene View and Game View.
2. Add Canvas-specific editor overlays.
3. Add safe area and device preview overlays to Game View presets.
4. Harden UI scaling behavior across Free Aspect, desktop, and phone presets.
5. Add missing UI controls: Toggle, Slider, Panel, Input Field.
6. Add script-facing UI callbacks and helper APIs.
7. Add touch/pointer groundwork so the same UI stack can move toward Android.
