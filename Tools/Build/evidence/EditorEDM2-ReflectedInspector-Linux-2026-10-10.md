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

## Native pointer event ordering correction

Hosted Linux CI exposed a docked-window pointer timing failure at the first
Boolean edit. The native test now sends pointer movement and click separately,
allowing a hovered frame before pressing the button. Source-byte and Undo/Redo
assertions, read-only checks, restart behavior and Vulkan validation rejection
remain intact. Eight consecutive focused runs passed (**103.66 s** total).
The complete graphical Development suite with Cryptography enabled and accepted
Inspector/build-console integration passed **237/237**, zero skips, **591.45 s**;
minimal Shipping built successfully. Hosted checks on the new commit are required.

## Slow-frame pointer delivery correction

Hosted PR484 on `317b2abde35eae5cffde028f4e0d157d50d7f08c` still missed the first
Boolean edit under the runner software-rendering schedule. The native test now
delivers movement, mouse down and mouse up separately, with a settled Inspector
layout after source selection. No edit/source/Undo/Redo/read-only assertion or
validation rule was removed. On main `64b20ea7b2fc4250af4900f6c1ace17001170464`,
graphical Development rebuilt the native host and fixture (44 incremental steps);
eight consecutive actual Xvfb/lavapipe runs passed, **129.01 s** total. This is
a test-only correction (Beads `nexora-owg.2.4`); runtime behavior is unchanged.
Fresh hosted checks are required before acceptance.

## Presented control and shortcut readiness

Beads `nexora-owg.2.5`: hosted signed head `80afa48f` still missed its first Boolean
gesture despite an identical script passing both PR493 display matrices. A cold Xvfb
readback inspected the actual 1600x1200 default-theme controls and established distinct
normal/hover/held/released pixels. The initial source-selection click is the real Scene
Select all control; the prior Hierarchy comment was inaccurate. Readback now bounds
first presentation, selected reflected control, and each writable Boolean input phase.
No edit is retried or supplied through a test-only model path.

The first observed native run passed **11.44s**. A subsequent one-thread/cache-disabled
attempt edited correctly but missed its Undo shortcut; that failed attempt remains
excluded. Modifier and primary-key press/release now occur separately. Eight final
cold `LP_NUM_THREADS=1 MESA_SHADER_CACHE_DISABLE=true` Xvfb/Vulkan runs passed
**134.62s** total, including exact edit, Undo/Redo, Save/reopen, read-only restart,
corrupt metadata rejection and compatible restoration. Vulkan diagnostics remain fatal.

Full graphical Development **239/239 passed, zero skipped, 598.60s**; minimal
Shipping configure/build passed (five incremental steps).
Logs: `extension-trust/frame-ready-{first-tests,final-repeat-1..8,full-*,shipping-*}.log`
in the execution work area. Diagnostic screenshot probes are observations, not acceptance;
an intentionally edited unsaved probe hit the expected close prompt and was cleaned up.
The fix changes Linux test input synchronization only. Fresh accepted-main integration
and hosted CI are required; full Editor milestones remain **0/8**.
