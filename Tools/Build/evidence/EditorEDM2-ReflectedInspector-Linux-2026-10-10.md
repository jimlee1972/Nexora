# Reflected opaque-property Inspector — Linux acceptance, 2026-10-10

The initial source starts from accepted main 0df2a1fa6625b46cc247902b94a985de9c76d78f;
final integration is based on accepted main 23c7f52df73a91868df5955e9468df970c5bdefb.
Editor owns bounded portable wire schemas/observations and stages exact-source opaque replacements;
EditorImGui owns drafts and actual controls. Native project activation/reload reads optional metadata
and never loads native code or rewrites schemas. Runtime/World/plugin object borrows do not escape.

## Actual acceptance

- `editor.reflected_inspector`: mixed selection, all supported wire adapters, exact source/padding
  preservation, one-step Undo/Redo, no-op Redo retention, stale selection/source/schema rejection,
  atomic invalid batch, save/reload and bounded malformed/signed/overflow/aliased metadata rejection.
- `editor.reflected_inspector_ui`: actual mouse/keyboard input at 1x/2x, enum/flag popup choices,
  full-width integer/entity/UUID, all vector/color lanes, fixed array and nested fields, draft-only
  typing, metadata replacement, disabled authoring and replay. Mixed flags set/clear only the
  clicked bit per source (Read/Write -> Read|Write/Write), retain unrelated bits/padding, and
  restore both original masks in one Undo. No draft-setter acceptance shortcut.
- `editor.linux_reflected_inspector`: real Editor/Xvfb/Vulkan Boolean and number edits, exact scene
  Undo/Redo/save bytes, padding preservation, actual selected-source read-only restart, corrupt
  metadata rejection without any project-file change, and writable reopen. All captured diagnostics
  reject Vulkan validation/synchronization errors.
- Final focused gate: 3/3 tests, 11.76 seconds, including the mixed Flags review fix.
- Full `linux-development` configure/build/CTest: **232/232**, no skipped tests,
  **575.36 seconds**, after 262 build steps. Graphical shell, Slang, Zig, Showcase and native Project Player were enabled.
- Minimal `linux-shipping` configure/build: **5 steps**, Editor stripped as intended.
  Existing pinned ImGui/Vulkan dependency sources were used without changing dependency versions.

This delivers fixed portable layouts, not arbitrary native C++ object restoration or dynamic arrays.
Physical display/input/DPI acceptance remains separate. Complete milestone progress stays **0/8**.

## Exported constant portability correction

Hosted Windows compilation at `407606866b4fa24afc9475cdc7c8d4e30010007f` rejected multi-declarator
`constexpr` members of the DLL-exported inspector class with MSVC C2487. Each unchanged constant
now has its own declaration. Linux Development rebuilt successfully; the complete graphical/native
suite passed **232/232**, zero skips, **583.29 s**. Minimal Monolithic Shipping rebuilt successfully.
Windows verification is delegated to the new exact-head hosted jobs; the Linux run is not a Windows
execution claim. No property limits, ABI functions or authoring behavior changed.
