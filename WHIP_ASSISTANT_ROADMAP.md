# Whip Assistant Roadmap

This note captures the future AI work so the engine/editor roadmap can move forward without losing the assistant direction.

## Current Baseline

- Whip Assistant is available as an editor panel.
- Provider abstraction exists for offline, OpenAI, and Gemini providers.
- Gemini can receive editor context, scene context, console snippets, project asset metadata, and selected texture images.
- The assistant can propose safe editor actions through `whip_tool` blocks.
- Scene proposals can be auto-applied in trusted modes and remain undo-friendly.
- The first sprite-level generation path exists, including multi-asset placement and resilient fallback for imperfect AI asset/sprite names.

## Roadmap

### 1. Reliable Tool Protocol

- Move from loose text blocks toward strict schema-backed tool calls.
- Add proposal validation before apply, with clear user-facing failure reasons.
- Add an automatic repair loop: when a proposal is invalid, feed the validation error back to the provider and request a corrected tool block.
- Support multi-step transactions with one undo entry.
- Keep partial apply disabled for scene generation unless the user explicitly forces it.

### 2. Better Engine Knowledge

- Generate the assistant knowledge manifest from actual C# bindings and editor component metadata where possible.
- Include component fields, valid enum values, script callbacks, asset types, and common patterns.
- Keep forbidden/unsupported API lists current so the assistant stops inventing Unity-style APIs.
- Add examples for Whip-specific workflows: movement, jumping, animation parameters, audio, scene loading, and collision callbacks.

### 3. Asset Understanding

- Add asset role metadata: Ground, Platform, Decoration, Prop, Chest, Torch, Tree, Hazard, Character, UI, Audio, Font, and similar roles.
- Support both manual role tagging in the editor and automatic role suggestions through image analysis.
- Cache visual summaries for spritesheets and sub-sprites so the assistant does not need to re-infer the same atlas every prompt.
- Add atlas preview annotations that map sprite indices to visual roles.
- Let the content browser expose and edit AI metadata per asset and per sub-sprite.

### 4. Level Designer Planner

- Do not let the provider directly choose every coordinate as the final authority.
- Split level generation into phases:
  - Intent: theme, size, difficulty, style, and required features.
  - Layout: grid/platform plan with start, route, gaps, verticality, rest points, and endpoint.
  - Asset pass: choose sprites by role.
  - Placement pass: snap to grid, align supports, place props, and avoid overlaps.
  - Validation pass: check scale, reachability, collisions, and camera framing.
- Add templates for common level types: platformer, arena, village hub, puzzle room, combat room, tutorial area.
- Add regeneration controls: make wider, easier, denser, more vertical, more decorative, fewer props, and similar refinements.

### 5. Visual Preview Before Apply

- Show generated level proposals as ghost objects before committing them to the scene.
- Let the user accept, reject, regenerate, or edit proposal parameters.
- Display counts: entities created, textures used, invalid placements, estimated bounds, and warnings.
- Highlight invalid or suspicious placements instead of silently skipping them.

### 6. Script Editing Agent

- Let the assistant edit C# scripts through complete-file proposals.
- Automatically run script build after applying script edits.
- Feed compile errors back to the provider and request a fix proposal.
- Add script templates for common Whip behaviors: platformer controller, camera follow, trigger zone, collectible, enemy patrol, animation parameter updater.
- Support selected-entity-aware script edits, including field creation for inspector-exposed tuning.

### 7. Scene And Entity Agent

- Support batch entity operations: rename, group, align, distribute, duplicate, cleanup, and component assignment.
- Add tool support for collider setup, rigidbody setup, animator setup, audio setup, and camera framing.
- Support entity templates/prefabs as assistant targets.
- Let the assistant reason over hierarchy, selected entities, and asset drag/drop context.

### 8. Validation And Playtest Helpers

- Add static validators for missing textures, invalid sprite indices, missing scripts, invalid animator controllers, broken asset handles, and suspicious transforms.
- Add platformer-specific checks: reachable jumps, excessive gaps, blocked spawn, unsupported props, and off-camera content.
- Add optional play-mode smoke tests that can run simple scripted checks.
- Surface warnings in the assistant response and console with actionable fixes.

### 9. Assistant UX

- Add permission levels: Review, Auto Safe, Auto All, and maybe Per-Session Trusted.
- Add proposal history with what was applied, rejected, repaired, or regenerated.
- Add "explain proposal" and "show diff" modes for scene, asset, and script changes.
- Add quick action chips after responses: Apply, Regenerate, Make Bigger, Make Cleaner, Fix Errors, Use Selected Assets.
- Improve error messages so the user sees why a proposal could not apply.

### 10. Provider Strategy

- Keep provider abstraction clean: `IAssistantProvider`, `OpenAIProvider`, `GeminiProvider`, and future local/offline providers.
- Use Gemini image understanding for atlas analysis where available.
- Keep OpenAI/Codex-style providers available for code-heavy tasks.
- Consider a local provider path for offline command planning and deterministic editor tools.

### 11. Whip Assistant Service

- Keep the long-term direction open for a separate `Whip-Assistant` service/library.
- Editor and engine should link to the assistant through stable interfaces, not panel-specific code.
- Shared assistant code should remain independent from ImGui UI.
- Editor panel should only handle presentation, settings, and proposal review/apply UX.

### 12. Safety And Undo

- Every editor mutation should be undo-friendly.
- Destructive actions should require review unless the user explicitly trusts them.
- Script edits should remain complete-file, build-validated proposals.
- Scene generation should be transactional: all valid or nothing, unless the user explicitly requests partial apply.
- The assistant should never claim something was applied until the editor confirms it.

## Recommended Next AI Milestone

When AI work resumes, the best next milestone is:

1. Add sprite/asset role metadata.
2. Add a deterministic Level Designer Planner that converts assistant intent into a validated grid layout.
3. Add proposal preview/repair so bad generations can be fixed before touching the scene.

After that, script editing with automatic build-and-repair is the next highest-value milestone.
