# Configured Editor native SDK reload acceptance

Task: `nexora-whr.8.3.8`. Initial base: `0c0a8953ed916ef995920308b90c6cb9e54a1635`.
Final integrated base: `ed5ec2cd520bd63cacbc6890f6703874699a1777` (PR794).
Final combined-source local acceptance is complete; immutable-head hosted acceptance is required before delivery.
This is a supporting slice; joint ED-WF-07/08 and physical/clean-host gates remain open.

## Reproduced admission defect

On the unmodified base, successful candidate callbacks could write unknown source999 into an entity
or hidden pool. Both candidates were accepted and executed two old Stop/Destroy probes. The new
CPU regression failed with both admissions, before adding production validation. The shared binding
resolver now rejects them before old teardown. Three focused CPU tests pass after the correction.

## Ownership and publication

Candidate callbacks mutate the existing isolated World/scene/Sprite clone. Final validation resolves
all visible/hidden/legacy bindings, captures the complete Scene palette/Sprite frame and, for the main
native SDK path, submits the owning GUI once. Project/Play/source scope and artifact bytes are
checked after candidate callbacks/CPU preparation immediately before native recording. The presenter
does not re-enter the locked Runtime host. Retirement and commit only move/swap preallocated owners;
old cached APIs retain quarantined World/session state through Stop/Destroy and library finalizers.
Source reimport waits until the next frame when a script result owns this frame. CPU-only hooks
perform CPU admission without claiming native submission. Existing cold/source-free/custom paths
retain their dispatch/lifecycle policy. C/Zig ABI, package codecs and module graph are unchanged;
C++ consumers rebuild for the added session validation method.

## Meaningful failure evidence

The CPU native presenter regression checks Unsupported, InvalidDescriptor, DeviceLost, a thrown
preparation and one Ready result. Rejected candidates preserve live generation, World, source owner
and pool; old Stop/Destroy only run after the successful presenter returns. The original CPU32/160
callback cases remain. Native fixture failures cover bad Zig, prepareReload, loadState, reloaded,
unresolved entity, hidden pool and cached writes during read-only Capture.

The original focused native run was deliberately closed gracefully after four accepted real SDK
replacements: observed36–41 seconds per canonical rebuild proved the initial aggregate900-second
budget could not fit33 replacements plus rollback compilations. Its raw trace and receipt remain
preserved. The new aggregate2400-second budget changes no per-generation90-second deadline or
assertion. Complete final-source acceptance must pass from the start before delivery.

## State and native oracle

The SDK fixture explicitly encodes80 bytes: original entity IDs, pool owner/index/generation, pending
request/additive scene, phase/next phase/input edge, ticks, primary scene and reload count. It never
serializes service tables, native pointers or Game memory. prepareReload reacquires tables; reloaded
validates preserved state without fresh start/pool creation. A provisional candidate log does not
count: acceptance requires Runtime publication, first dispatched Update and independently predicted
source/pool pixels plus unchanged3D and authoring pixels/files. Linux .fini_array callbacks call real
cached source/pool/World functions after instance destruction. Stop/cancel and obsolete compiler
results use the ordinary product coordinator. Detailed results follow after the full gate.

The first complete-run attempt reached33 accepted SDK replacements, all7 rejected failures and
all5 scene/animation phases, but failed its new cancellation driver: the existing ordinary Stop
ran before saving generation300, so an inactive watcher correctly started no compiler. This is a
fixture-order defect. The cancellation edit/progress check now precedes the one shared Stop; all
zero-module/texture/visible and no-late-admission assertions remain. The failed1747.66-second trace
is preserved, and the old full run was stopped with its native child drained. A reduced private
initial-generation/five-phase smoke checks this corrected Stop path first; it does not count as
33-replacement acceptance. The complete final-source full gate must pass again before delivery.

The corrected complete native case passes in1637.21 seconds:33 accepted actual saved SDK
replacements,7 rejected failures, all5 scene/animation phases, obsolete-result rejection and
compilation-in-progress Stop cancellation. All33 retired generations0–32 execute real image
finalizers; the receipt includes38 pre-Stop finalizer records because5 rejected candidates also
finalize. The native session reports54,064 acquired/presented frames and zero UI rejections.
Full552 and the remaining14-gate acceptance still require successful completion.

## Main integration

Main advanced toPR794/ed5ec2cd with existing-source metadata editing. The own20-file working
change was preserved in an explicit stash, main fast-forwarded, and the change reapplied without
conflicts. All peer code, documentation and evidence remain intact. Of the11 frozen implementation
files, only main.cpp and the UI test CMake list gained the upstream edits. Metadata editing remains
guarded against active Play. The prior base passed complete552/552 in4394.48s, Shipping native SDK,
Monolithic11 CPU cases and native reimport392.36s/initial SDK69.01s. Its unfinished Monolithic
replacement run was deliberately stopped with the native child drained. That is historical scope,
not acceptance of the combined tree. Complete final-base554 and all14 local gates must pass again.

## Final combined-source local acceptance

Validated base: `ed5ec2cd520bd63cacbc6890f6703874699a1777`. All11 implementation/build/test hashes in the
[companion receipt](ED-WF-08-EditorNativeReload-Cloud.json) match the final frozen source.
Complete Linux554/554 passes with zero failures/skips in4678.26s. All14 local gates pass:
Development configure/build/full CTest,21 actual GNU15 commands across19 translation units,
full Shipping configure/build and real SDK Player, Monolithic graphical Editor/Player build,
11 CPU cases and all3 native Editor cases, and actual TwoD-OFF configure/build/package refusal.
Both modular and Monolithic native Editor runs pass33 actual saved SDK replacements,7 rejected
failures,5 post-reload scene/animation phases, actual image finalizers, obsolete results and
compilation-in-progress Stop cancellation. Their initial SDK and separate32-round/70-publication
source-reimport cases pass too. Development/Shipping main SDK Player accepts deleted-source Unicode
relocation with exact native source/pool/3D masks and actual shutdown.

Commands are the mandatory `cmake --preset linux-development`,
`cmake --build --preset linux-development`, `ctest --preset linux-development --output-on-failure`,
full `linux-shipping` configure/build, and the documented source-reload Monolithic/TwoD-OFF
configurations. The Monolithic native selector is
`^editor[.]linux_packaged_source_(services|reimport|reload)$`; its CPU selector covers the11
scene/session/bridge/animation/reload cases. Every gate exit and receipt hash is retained.

The accepted local scope is Linux software Vulkan/Xvfb; this does not claim local Windows/macOS
execution or physical/clean-host acceptance. Final unchanged-head hosted checks, review inspection
and publication/tree/blob guards must pass before merging the own PR. Workflow5/9, graphical
Editor0/8, TwoD9/11, joint ED-WF-07/08 and the independently owned standalone reload scope remain
open/unchanged. RootREADME stays unchanged.
