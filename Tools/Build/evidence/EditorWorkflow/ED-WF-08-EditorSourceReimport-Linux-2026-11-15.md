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

## Complete corrected-test validation

The test-only MSVC portability correction repeats `cmake --preset linux-development`,
`cmake --build --preset linux-development` and unfiltered
`ctest --preset linux-development --output-on-failure`: **551/551**, zero failures/skips,
**2680.03 s**. The repeated modular native reimport case passes **365.56 s**.
The corrected CPU test also compiles with its actual GNU 15 flags and passes the rebuilt
**11 Monolithic CPU cases**. No test oracle or warning policy was weakened.

The original **14 sequential local gates** at `8448806d` validate the unchanged runtime/native
sources: full Shipping configure/build and canonical SDK native Player, monolithic graphical
Editor/Player plus 11 CPU and both native Editor cases, actual TwoD-OFF configure/build/refusal.
Those baseline 551 tests passed in 2639.43 s. All other 16 implementation/build/test files remain
byte-identical. Development/Shipping Player each checks **65,536** independent source/pool pixels
after deleted-source Unicode relocation, stable handles, unchanged 3D and native drain.
GNU 15 coverage consists of **21 original-flag variants across 19 translation units**: 20 unchanged
baseline variants plus the explicitly recompiled corrected test. The machine receipt separates
the baseline 14 gates from the new full-suite/test-portability evidence.

## Retained investigation history

Four original native fixture failures remain: obscured Content commands, a provisional legacy
scrollbar viewport, an unsettled filtered row and an incorrect queued-failure expectation for
pre-enqueue missing-file rejection. Final input waits use stable actual controls; pixel/World/
identity oracles were preserved. The complete focused native case then passed 367.78 s.
Final source review additionally guarded surface recovery and strengthened native failure tests;
the first full run was explicitly interrupted, then the complete 14-gate sequence reran on the
final frozen 17 implementation/build/test files. Interrupted/earlier results do not validate the
final source. See the machine receipt for retained log hashes and scoped evidence.

## Windows test portability correction

Hosted MSVC /W4 /WX rejected the original test's int-to-float Pivot ternary (C4244) and two
size_t-to-uint32 event capacities (C4267). Four Windows job logs are retained. The correction uses
explicit float literals and an explicit uint32 cast for the four-element event array. Runtime,
native, source/asset/Clip behavior and expected values are unchanged. PR #793 was made draft before
any merge. Baseline Linux/Shipping/Mono/OFF success does not substitute for corrected-head
Windows acceptance; all fresh applicable hosted checks are required before delivery.

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
