# Reflected opaque-property Inspector — Linux acceptance, 2026-10-10

The source starts from accepted main 0df2a1fa6625b46cc247902b94a985de9c76d78f.
Editor owns bounded portable wire schemas/observations and stages exact-source opaque replacements;
EditorImGui owns drafts and actual controls. Native project activation/reload reads optional metadata
and never loads native code or rewrites schemas. Runtime/World/plugin object borrows do not escape.

## Actual acceptance

- `editor.reflected_inspector`: mixed selection, all supported wire adapters, exact source/padding
  preservation, one-step Undo/Redo, no-op Redo retention, stale selection/source/schema rejection,
  atomic invalid batch, save/reload and bounded malformed/signed/overflow/aliased metadata rejection.
- `editor.reflected_inspector_ui`: actual mouse/keyboard input at 1x/2x, enum/flag popup choices,
  full-width integer/entity/UUID, all vector/color lanes, fixed array and nested fields, draft-only
  typing, metadata replacement, disabled authoring and replay. No draft-setter acceptance shortcut.
- `editor.linux_reflected_inspector`: real Editor/Xvfb/Vulkan Boolean and number edits, exact scene
  Undo/Redo/save bytes, padding preservation, actual selected-source read-only restart, corrupt
  metadata rejection without any project-file change, and writable reopen. All captured diagnostics
  reject Vulkan validation/synchronization errors.
- Final focused gate: 3/3 tests, 11.77 seconds.
- Full `linux-development` configure/build/CTest: **227/227**, no skipped tests,
  **553.39 seconds**. Graphical shell, Slang, Zig, Showcase and native Project Player were enabled.
- Minimal `linux-shipping` configure/build: **74 steps**, Editor stripped as intended.
  Existing pinned ImGui/Vulkan dependency sources were used without changing dependency versions.

This delivers fixed portable layouts, not arbitrary native C++ object restoration or dynamic arrays.
Physical display/input/DPI acceptance remains separate. Complete milestone progress stays **0/8**.
