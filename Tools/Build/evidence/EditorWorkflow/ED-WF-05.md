# ED-WF-05 Editor background compilation and stateful reload

Work order: [#746](https://github.com/jimlee1972/Nexora/issues/746), Beads `nexora-whr.5`.
Status: **in progress; integration and acceptance pending**. This document records source
observations and acceptance obligations, not completed functionality. ED-WF-02 and ED-WF-04
are accepted dependencies. Input PR #760 is a separate accepted and merged supporting change.

The first local SDK slice reproduces fresh init/start resetting a restored candidate, then adds
an opt-in candidate lifecycle capability and prepare/restore/reloaded path. The original adapter
CTest exited 8 (six passed, one failed); after the local repair it exited 0 (5.40s). This is only
a unit-level SDK check, not Editor hot reload or work-order acceptance.

Subsequent [native Windows foundation evidence](ED-WF-05-Lifecycle-Windows.json) records successful
Runtime/SDK target builds and three scoped CTests. The first cold-restart repair passed in 51.05s.
The final explicit-opt-in source passed again: exit 0, 3/3, zero skips/failures, 61.72s.
The SDK requires paired state hooks and `pub const reload_lifecycle: bool = true;`; a codec alone
keeps its legacy capability/lifecycle and rejects candidate mode before allocation.
The real Zig test retains 32 legacy reloads/224 fault rejections, and adds 32 candidate-lifecycle
reloads/160 fault rejections, counter/scene/entity continuity, generation-bound callback binding,
and unload/fresh restart. The first new run failed at the final cold restart (exit 8,22.96s): the
default host template inherited the candidate flag. Clearing that flag after commit fixes fresh
startup while retaining each live generation's original host table. The failed log remains retained.
These test-owner Worlds are not Editor resource-isolation acceptance. Real compiler coordination,
Editor World/service isolation, live animation/pool continuity and full Cloud/native/CI gates remain required.

The next local slice extracts `PrepareGameScript` from the installed GameBuild producer, including
source freezing, canonical SDK materialization, managed tools/tests, V3 admission, generation filenames
and source/artifact guards. Game Build now calls the same entry and retains its publication records.
Native Editor/Player and compiler test targets build successfully. The first five-case run exited 8:
four passed (including actual Game Build in 571.86s and standalone script compilation in 261.23s),
but the unbuilt capture test was Not Run. After building it, the capture case passed alone (exit 0,
0.45s). The complete five-case rerun then exited 0: 5/5 passed, zero skips/failures, 432.30s,
including actual Game Build (265.56s) and standalone compilation (166.16s).
[Compiler receipt](ED-WF-05-Compiler-Windows.json) retains original logs/hashes and limits.
This accepts the local scoped compiler gate, not integrated Editor hot reload. It is outside the
scoped Runtime/SDK receipt above and does not accept
Editor compile coordination or the work order.

## Verified foundation checkpoint

[PR #762](https://github.com/jimlee1972/Nexora/pull/762) is squash-merged as
`f7537be874d83b5bcc50c2140c30089623c56749`. The merged tree is identical to validated
`d093841b032376f98ccfef4766289a176418341a`: `4c8b642abd3e0209246a0f16142a0c7d9e6386fa`.
The [complete Cloud receipt](ED-WF-05-Cloud-d093841b.json) records five actual exit0 gates,
496/496 Development CTests with zero failures/skips/retries, and supported Shipping linkage
(Editor/testing OFF). The [CI inventory](ED-WF-05-CI-d093841b.json) records all41 successful
checks/three completed workflows and exact-head/merged-tree proof. Original failed sources,
compiler/XKB logs and earlier Cloud archives remain retained. This verifies the common compiler,
explicit restored Zig lifecycle and harness repairs only. It does not accept ED-WF-05, include
local uncommitted integration, accept physical devices or provide a final Editor ZIP.

## First Cloud/CI candidate

PR #762 candidate `de0204cfc8d0f2c612ae141ef8ccf6ed258c62d4` is unaccepted: multiple Linux
CI builds fail at the existing Runtime asset fixtures with GNU 15.2's dangling-pointer warning.
Those runners use Ubuntu 26, while the prior c9 Linux display job used Ubuntu 24/GNU 13.3.
[Original failure receipt](ED-WF-05-CI-Failures.json) retains full-log hashes. The repair uses named,
owning asset vectors without temporary empty-dependency aggregates, preserving every cycle,
activation, pinning and rollback assertion and all warning flags. Its Windows target build and
contract CTest pass (exit 0,1/1,0.20s), which does not substitute for repaired-head GCC/Cloud/CI gates.
The old Cloud run is allowed to finish its current live gate and retains its original evidence;
only the repaired exact head can receive final acceptance. The compiler coordinator is separate
local WIP and is not part of this candidate repair.

Repair `357be4af` clears the first Runtime fixture compilation and exposes the same GNU 15.2
diagnostic in existing V1M5 DAG/data-table initializer-list fixtures. Those now use named owning
manifests/rows with explicitly populated fields. All cycle/lookup/duplicate-row conservation,
generation/residency and the original 10k-cook deadline remain. Native Windows build exits 0 and
the two Runtime contract cases pass (exit 0,2/2,0.48s). Both candidates' original logs are retained
in the failure receipt. Full repaired-head Linux gates and CI remain mandatory; no result is waived.

Repair `4c3a53ad` clears both Runtime fixture translation units and exposes the next GNU 15.2
warning in the existing Prefab source-apply expected opaque-component vector. Its replacement
uses an owning, default-constructed record with the same type, name and payload byte.
The final native target build and normal-permission CTest pass (exit 0,1/1); earlier sandboxed
path canonicalization failed with Access denied before the changed assertion, and is retained.
The workflow contract suite passes all 16 cases after using the existing pinned Python dependencies
and task-local python3 launcher. Linux desktop CI now collects remaining Ninja diagnostics only
after its original required build fails; strict warnings and the original failed status remain.
This is diagnostic collection, not an acceptance retry. Full repaired-head gates remain required.

Repair `aba81782` clears the original Runtime/Prefab failures. Its failure-only Ninja diagnostic
step finds the remaining additive-session, scene-save-batch, scene-comparison GUI and reflected
Inspector nested-temporary fixtures in one completed compile pass. Those now have explicitly
owned opaque/component/field data with all original bytes, kinds, offsets, choices and assertions.
A matching composition fixture adopts the same ownership. Native scene targets build and pass
3/3 (exit0,121.60s); both actual ImGui cases build and pass (7.38s/6.10s). These local builds also
contain separate, uncommitted Runtime/pool prework, excluded from this fixture publication; they
do not prove an immutable full gate. Original failed build/logs remain in the receipt and every
new head still requires full Cloud and CI.

## Verified integration gaps

Observations use commit `c9f1717085078b216913cd1096427c5aa42c6946`:

- `Apps/Editor/PlayGameplayModule.h::LoadRelative` unloads before loading and constructs the
  legacy `GameplayModuleHost(Host())`; it is not an isolated hot-reload transaction.
- `Engine/Editor/src/GameBuild.cpp` already freezes script sources and stages the canonical SDK.
  `Engine/Editor/GameBuildSDK.cmake` embeds `Bindings/Zig/*.zig`, the shared project CMake entry,
  and trusted build resources. Installed Editor compilation must continue using this ownership,
  without adding engine build/ABI plumbing to authored projects.
- `Bindings/Zig/game.zig::Module.create` calls `Game.init` before state restoration. Its `start`
  calls `Game.start` unconditionally. Restored lifecycle behavior needs an explicit SDK policy
  so normal fresh-start code cannot silently reset counters or duplicate cloned entities. Preserve
  the existing fresh-create initialization-failure and allocator-cleanup contract.
- `GameplayModuleHost::ReloadLocked` captures state before `ReloadTransaction.begin`. The Editor
  embedding must prevent/reject host mutations during capture as well as isolate candidate
  create/restore/start/destroy and retired callbacks/finalizers. A failed save must preserve the
  original World, not merely retain the old DLL.
- `PlaySceneServices` owns quotas/colliders; `PlayTwoDServices` owns Sprite/animation/pool, Input,
  UI, Audio, Storage and texture state. Rebinding fresh services is not state migration. Native
  audio opens during binding and Storage writes can escape a candidate World: those effects need
  isolation, not a second ordinary `Bind` against live providers.

## Implementation obligations

Extract a shared script-module producer from the existing SDK build path. The worker owns frozen
manifest/source bytes, unique-generation output, bounded diagnostics and cancellation; no borrowed
workspace, GUI or Play World crosses threads. Compilation keeps the old game running and does not
publish a shipping game directory on every edit.

Accept results only for the latest project identity/root, source request and Play epoch. Reuse the
watcher debounce and invalidate/cancel/drain on Stop, close, project switch and newer source. Recheck
captured source identity/bytes before owner-thread admission. Failed compilation preserves the last
working generation and reports actual compiler diagnostics.

At the owner-thread safe point, stop new dispatch and wait for real registered work. Stage the
current World and service state at that point, not at compile start. Keep per-generation host
contexts/tables and allocation ownership stable through callbacks and DLL finalizers. Commit must
preserve public Play World identity, entity/scene handles and Play-owned objects; retirement writes
must reach quarantined old state. Failure discards only candidate state and resumes old dispatch.

Use negotiated SDK lifecycle/context support without changing existing V3 descriptor prefixes.
Do not store native pointers in migration payloads. Providers without safe migration must leave
the old module running with an explicit restart-required result. That fallback does not complete
the required resource/animation/pool integration or waive ED-WF-08/final acceptance.

## Required evidence

Retain real typed Zig compilation/reload through the actual Editor, at least 32 replacements,
and uninterrupted counters, stable scene/entity handles, input and resource ownership. Cover rapid
edits, failed compilation, missing migration, ABI mismatch, failed save/restore/initialization,
host-mutation attempts during save, active registered work, cancellation, Stop and project switch.
Prove no stale result or old-code pointer is used and that retirement cannot delete live objects.

SDK tests must distinguish fresh init/start from restored lifecycle, preserving the original
fresh-create failure/cleanup checks. Native GUI/source/pixel evidence is required alongside helper
tests; integrated Sprite animation/pool continuity remains required in ED-WF-08/ED-WF-09.

Run the full Codex Cloud `linux-development` configure/build/CTest and applicable supported
`linux-shipping` configure/build, followed by exact-head CI and separate native platform checks.
The foundation checkpoint above has full-gate evidence; the remaining integrated Editor workflow
has not earned those gates or native acceptance.

## Local integration prework (historical checkpoint before main transaction)

[Scoped Windows receipt](ED-WF-05-Integration-Windows.json) retains exact local source/log hashes.
Runtime capture protection/final candidate validation builds and passes the existing C++/real Zig
reload cases (2/2,exit0,18.32s), including a real old-code work lease before capture and rejection
of ignored save-phase writes. Private pool branches pass 32 commit/retirement cycles.
SpriteScene/GameplayBridge branches copy metadata, camera, owned animation clocks and queries,
then rebind moved Play World state without resetting IDs or allowing old cleanup into live state.
UI branches preserve focus/capture/navigation and immutable resource ownership, including cached
C API contexts through state exchange. Final resource target build and three CTests pass (exit0,
zero skips/failures). Original final-fixture compile and concurrently locked DLL failures are kept.

This is separate local prework, excluded from the currently validating PR source. The later
local follow-up below supplies generation proxies and resource branches. Editor watcher/epoch
admission, complete World transaction wiring and real source-edit reload acceptance remain open.
The compiler coordinator unit uses an injected producer; its pass is not an actual GUI build.
Keep ED-WF-05 unaccepted and the nine-order progress at 4/9.

Local follow-up adds input branches and independently accounted PCM mixer branches; source
usage/peak and the existing native output remain unchanged during candidate preparation. Their
32-cycle regression pairs pass (exit0,2/2,0.37s). The initial byte-type warning/build failure and
unbuilt audio executable/NotRun are retained and are not counted as passes. Generation-owned
proxies now cover the actual main Play Host and its optional 2D/Input/UI/Audio/Storage tables.
Cached callbacks cannot bypass capture or external-write guards; raw table prefixes and Storage
error codes remain distinct. Final relevant targets build and all18 scoped CTests pass (exit0,
zero skips/failures,23.04s), including actual Host/2D wiring and original Play regressions.
Strict World generation transfer, reload phase transitions, automatic watcher/compiler UI and
real native source-edit acceptance still remain; this is uncommitted prework excluded from PR762.

Coordinated production Play resource branches now preserve pool handles, animation phase, camera,
query snapshots, input edges, UI frames and existing texture identities over identical staged Worlds.
Commit transfers the existing native audio owner while rerouting both cached proxies before old
cleanup; exact parent-generation identity rejects obsolete branches. No new native output or
texture registration is created during candidate preparation. Scene-service branches also retain
scene/spawn quotas instead of resetting via Bind. Source/destination World ownership, failed
branches, retired cleanup and final texture retirement are exercised locally; complete main
transaction hooks and source-edit reload still remain unimplemented and unaccepted.

The final coordinated-service regression rebuild passes ten targets;18 scoped native Windows
CTests pass (exit0,zero failures/skips,24.45s). The new fixture initially omitted the existing
query-source argument; the failed build remains retained and was corrected without API or
assertion changes. This local source remains outside the merged foundation.

## Current main transaction batch

The [main transaction checkpoint](ED-WF-05-Transaction-Windows.md) supersedes the implementation
gaps in the earlier uncommitted local checkpoint: the main facade now installs strict generation
World/resource/scene-service hooks. Fourteen affected targets build and23 scoped cases pass
(exit0,58.31s); its main C++ fixture has96 successful replacements/576 expected rejections.
Two real no-argument Windows GUI launches also pass with the original visibility/response/order
assertions; isolated-desktop failures are retained. Automatic source-edit/compiler/watcher
admission and full immutable-head Cloud/Shipping/CI remain pending. Work-order acceptance stays4/9.

## Live script workflow checkpoint (unaccepted)

The main configured ScriptProject Play path now connects the existing source-only SDK compiler
to owning background content observation, watcher debouncing and strict prepared-generation
admission. Stop cancels and drains observation/compiler workers before releasing World/module
owners. SceneOnly/manual DLL behavior remains available. Compiler phases use readable stage
labels and sanitized output reaches Console; paused Play can admit current results.

The local three-case gate passed with real exit 0: compiler/coordinator 2.97s, actual SDK game
build/publication 289.26s and script-only compilation/source guards 156.27s (448.64s total,
zero failures). Its observer tests cover equal-size/time edits, generated-cache exclusion,
addition/rename/deletion, saved settings changes, content budget and cancellation.

A real Windows Editor Ctrl+P smoke admitted one authored Zig replacement while old code continued
ticking: counter4027, Scene1/entity4 retained, ordinary close exit0 and authored baseline unchanged.
Its original receipt describes cancellation as real compilation, but the retained log only proves
ScriptSnapshot-stage preparation cancellation; active compiler-child termination was not observed
in that smoke. The strengthened 32-cycle harness now waits for CompileAndTest and retains handles
to owned compiler descendants to verify they terminate after Stop. That original run was interrupted after11 replacements during the next compile. The operator
subsequently confirmed in this chat that they manually closed the Editor. Classify this as
operator-interrupted, not a product crash/regression; it is still not a32-cycle pass. The original
logs/exit1 remain preserved, and no child numeric exit code was recorded in that older harness.
New harness failure receipts record the actual child exit code and the main opt-in diagnostic
records its loop-exit reason. Source/binary snapshot and logs remain under
`build/editor-workflow-native/ed-wf05-live-script-*`.

The diagnostic PR763 Windows CI still fails the unchanged 35-second startup gate. At `c3a65497`,
the splash responds to6478 probes, BeginFrame returns, and no first-UI checkpoint follows. The
root cause is not established. Additional opt-in checkpoints narrow display/event/metrics work;
no deadline/assertion is relaxed. These live-script results were captured as uncommitted local work and must still
pass immutable-head Cloud/Shipping/CI, remaining native scenarios and final package acceptance.
Accepted work stays4/9 and Editor0/8.

A packaging follow-up enables the actual32-cycle harness against the extracted ZIP on explicit
Windows package workflow runs and automatically on the active implementation branch. It owns the Editor/SDK process tree, preserves timeout/failure
output and rejects partial receipts; it cannot publish a ZIP after that gate fails. Portable
package admission now passes9/9 synthetic tests, which do not establish native GUI acceptance.

The immutable `0f901cb3` Cloud run is complete and FAIL: Development configure/build exit0,
full CTest exit8 (498/499 pass,zero skip/retry), supported Shipping configure/build exit0. The
sole failure is pointer_buttons: after a successful button2 press and drawframes59..66, the
original0.15-second release window received no new consumer/draw bytes. Existing evidence cannot
distinguish input delay, frame stall and observation scheduling; no deadline/assertion is relaxed.
The archive is532675 bytes, SHA256 `fc0ff88d8377596db13c37ef5b9ba29dac33cbaaa583c06476822412565de771`.
All old archives remain unchanged. This blocks merge; it does not erase the local slice evidence.

Operator clarification: the11-replacement local run was deliberately closed by the user. This
explains the interruption; it does not change its retained wrapper exit1 or supply the remaining
21 replacements. Full32-cycle acceptance moves to isolated hosted Windows.

[Live-script Windows receipt](ED-WF-05-Live-Scripts-Windows.json) records the scoped/native
commands, source/binary/log hashes, real cancellation/job drain and the operator interruption.
[Cloud0f901cb3 receipt](ED-WF-05-Cloud-0f901cb3.json) records the failed full gate separately.


## Extracted Windows package and rejection coverage checkpoint (unaccepted)

[51bd8a16 package receipt](ED-WF-05-Package-51bd8a16.json) records the hosted Windows startup and extracted-ZIP32-cycle PASS, downloaded artifact/ZIP hashes and retained CI/Cloud failures. The full Cloud gate is FAIL495/499 (four Prefab GUI cases); required CI aggregates also fail on abandoned/cancelled macOS/iOS jobs. No merge, ED-WF-05 acceptance or nine-order progress increase follows from the Windows pass.

The supplemental real-Editor harness now injects compiler, state-load, candidate-create and candidate-start failures, including a candidate World write before failed start. Every old update verifies its retained World first. The harness requires unchanged active generation/handles and an increasing counter after each rejection, then a valid recovery commit, old compiler cancellation/drain and normal close. The packager refuses incomplete fault/recovery receipts. Its original local attempt observed all four rejections but has no terminal exit/receipt after interruption during recovery compilation; it is not a successful test or product crash finding. The original stderr and interruption-observed.json remain in the unique evidence directory. New IN_PROGRESS checkpoints are explicitly insufficient for package admission. Updated native/immutable-head gates remain required.

Readiness follow-up (pending): Linux now shares hidden-main first-frame reveal and X11 search requires actual visibility within the original15-second acquisition budget. Source-apply uses one10-second gesture/pixel budget and retains failure fixtures. This corrects phase ownership without claiming every Cloud failure root cause is proven. The worker-budget candidate is not adopted (its historical CTest also fails493/499). Windows startup/reload share existing-input-desktop selection and a foreground parent per process launch; no keys are sent to foreign windows. The first extracted-ZIP local fault attempt stopped before Play because foreground ownership was absent, and a separate local startup attempt rejected occlusion. Their original logs/exit codes remain retained; new exact-head native/full gates are still required.

Upstream integration includes main `1df6ab357da69ee3257097a9b5176695b7c96b9a` (PR #765). The bridge conflict preserves the World pointer needed by legacy generation rebinding and the new catalog-per-entity routing. Catalog bridges refuse the legacy single-scene branch/rebind path before dereferencing its absent scene pointer. The added regression preserves World bytes, primary, revision and usable Sprite API after refusal. Complete catalog-session migration remains a later integration gate, not an accepted replacement by this guard. Original startup/Cloud/CI failures and all accepted-count boundaries remain.

[Post-merge Windows receipt](ED-WF-05-Main-Integration-Windows.json): all9 targets build(exit0), and6 scoped CTests pass(exit0,5.64s), including the catalog/legacy guard regression and strict reload/packaged DLL lifetimes. These are CPU/service checks, not a new GUI or full-gate pass.

## 8870 native harness failures and owned launch follow-up (unaccepted)

The hosted Windows job passes its original five startup cases, then fails the package's launcher
foreground precondition before source-edit acceptance. Its artifact73590B SHA256
`514f5edab3313d3e04476f331a45f9dcb23d8361c32c8d2ed870650cc6986081` and complete log are retained.
The new32×32 Button launcher occupies a primary-work-area interior and uses physical coordinates,
bounded foreground acknowledgement and, when necessary, an actual guarded own-window mouse click.
It rejects foreign hit targets, capture/menu state and held buttons, and waits for its mouse-up
before pointer restoration. Three local native launcher-only trials pass with zero surviving
windows; they do not establish Editor/32-cycle acceptance. Initial one-pixel guard failures remain.
The Linux hosted display gate rejects packaged Play's X_SetInputFocus with BadMatch because its
private searches still admitted unmapped windows. Both now require onlyvisible, retaining all
existing waits/assertions. [Receipt](ED-WF-05-Launcher-Windows.json). Fresh immutable gates are required.
