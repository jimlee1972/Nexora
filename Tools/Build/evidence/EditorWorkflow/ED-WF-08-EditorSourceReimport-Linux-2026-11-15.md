# Configured Editor Sprite and Atlas reimport — Linux Cloud, 2026-11-15

Supporting task `nexora-whr.8.3.7`, starting from accepted main `5e284c6e1c0295e8559b20496575f3f00d002cd6` (PR792).
The main Play World now retains entity/style/animation/pool state while an owning source/Atlas
catalog and matching GUI registrations publish after one successful complete native frame.
Source inventory/types and Clip bytes remain fixed; other edits require restarting Play.
Source-free/custom paths retain their existing behavior. C/Zig layouts, codecs and module graph
are unchanged; C++ consumers rebuild. The root README and milestone totals remain unchanged.

## Executed behavior

- Actual Content Reimport in modular and monolithic Editors: **32 rounds / 70 publications**,
  **31,104** independently expected source/pool pixels across 32 masks, **36** independent 3D
  pixels per capture, stable pool identity, exact saved scenes/settings/scripts/sidecars and
  unchanged logical anchors. Five animation/Additive/unload/Replace phases check another
  **5,184** source/pool pixels. A half-frame clock survives reimport and completes after resuming.
- Actual canonical SDK Build while Play remains active publishes the reimported Pivot 3/green Atlas
  into a capability 4/two-scene game. The CPU consumer checks those changed package values;
  a saved read-only reopen compiles the SDK and verifies native source/pool pixels again.
  Real Stop/Destroy and native drain report no rejected UI submissions.
- Corrupt worker failure and missing-file pre-enqueue rejection preserve live native pixels;
  restoration publishes successfully. CPU tests cover stale source rechecks before native work,
  descriptor drift, missing closure, changed Clips, hidden unresolved pool values, foreign threads,
  preparation/reload/iteration guards, half-clock/style/World/pool storage and 32 catalog changes.
  Superseded decoded Atlas owners expire. Metadata-only changes share native registrations at
  all 64 occupied slots; changed Atlas capacity/native failures retain the previous owners.
- Native DeviceLost remains distinguishable from redrawable descriptor rejection, performs one
  transaction draw attempt and keeps published assets. The application uses existing surface
  recovery rules and never replays an ended GUI frame. This is CPU propagation coverage, not a
  physical device-loss or clean-host acceptance claim.

## Complete final validation

`cmake --preset linux-development`, `cmake --build --preset linux-development`, unfiltered
`ctest --preset linux-development --output-on-failure`: **551/551**, zero failures/skips,
**2639.43 s**. The final modular native reimport case passes **361.36 s**.
All **14 sequential local gates** pass, including full Shipping configure/build and native SDK
Player, monolithic graphical Editor/Player plus **11 CPU cases and both native Editor cases**,
and actual TwoD-OFF configure/build/package refusal. Development/Shipping Player each checks
**65,536** independent source/pool pixels after deleted-source Unicode relocation, stable handles,
unchanged 3D and native drain. Final GNU15 validation compiles **21 variants across 19 translation
units** with original compile flags. The machine receipt records every command, exit and log hash.

## Retained investigation history

Four original native fixture failures remain: obscured Content commands, a provisional legacy
scrollbar viewport, an unsettled filtered row and an incorrect queued-failure expectation for
pre-enqueue missing-file rejection. Final input waits use stable actual controls; pixel/World/
identity oracles were preserved. The complete focused native case then passed 367.78 s.
Final source review additionally guarded surface recovery and strengthened native failure tests;
the first full run was explicitly interrupted, then the complete 14-gate sequence reran on the
final frozen 17 implementation/build/test files. Interrupted/earlier results do not validate the
final source. See the machine receipt for retained log hashes and scoped evidence.

## Remaining acceptance

This is source/Atlas publication and initial SDK generation evidence. At least 32 actual main
Editor SDK Zig replacements, complete rollback/retired library finalizer acceptance and joint
ED-WF-07/08 remain open in `nexora-whr.8.3.8` and their parents. The independent 8.2 claim,
historical investigations and physical/clean-host gates are preserved. Workflow **5/9**,
graphical Editor **0/8**, TwoD **9/11** remain unchanged. All applicable hosted checks must pass
for the unchanged own PR head/base/tree, with reviewed findings resolved, before delivery.

[Machine receipt](ED-WF-08-EditorSourceReimport-Cloud.json),
[application contract](../../../../Apps/Editor/README.md#live-configured-sprite-and-atlas-reimport),
[session contract](../../../../Engine/TwoD/GameSpriteSession.md#source-and-atlas-reimport).
