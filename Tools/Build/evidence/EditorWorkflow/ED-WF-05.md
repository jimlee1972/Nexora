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
No implementation, full gate or native acceptance result has been earned by this baseline.
