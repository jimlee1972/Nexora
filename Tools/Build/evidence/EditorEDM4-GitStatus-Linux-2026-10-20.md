# ED-M4 bounded real Git status — Linux, 2026-10-20

Beads delivery: `nexora-gu7.22.1`; parent M4-06 `nexora-gu7.22` remains open.
Prototype base main: `6bed775d80bab8a4766293eed8b9db9fb52d835d`.

## Scope

The owner explicitly queries an authorized local Git executable/repository through managed
BuildProcess. There is no shell, remote operation or index mutation command; optional index
locks, fsmonitor and untracked-cache updates are disabled. Canonical roots support `.git`
directories and linked-worktree gitfiles. Configuration/environment remain trusted, not isolated.

Porcelain-v1 NUL records preserve raw owning names, both status lanes, conflicts and original
rename/copy paths. Complete output admission is 256 KiB, 4,096 entries and 4,096 bytes/path;
malformed, truncated, duplicate or dropped data never partially replaces a capture. Normal
untracked directories may be collapsed; ignored paths and submodule recursion are not requested.
Missing tools, failed native exits, cancellation and memory/budget failure are not clean trees.
Same-scope last-good data survives failures. Scope/project/provider replacement cannot resurrect
old results. Owner calls serialize, JobSystem outlives all calls, and Shutdown drains workers and
managed children. Captures own every byte and retain no document/native callback borrows.

The C++ API is additive; public consumers rebuild. No stable C/Gameplay ABI, module dependency
or persisted project/scene format changes. Observations grant no filesystem write authority or
source-version lock. Graphical status/diff/merge/project routing and the full M4-06 goal remain open.

## Preliminary verification

GCC14.2/Linux x86_64, actual Git2.52.0. The new provider/parser/test were compiled directly with
the unchanged product BuildProcess/Core/Foundation libraries. Both strict parser and actual
native-process integration programs exited 0. This verifies the new source against real process
services; the independent full Shipping and CTest gates remain pending. The initial independent product
build completed 773 steps and actual parser/provider CTest passed 2/2 in 0.42 s before the managed
environment restarted; no interrupted Shipping/full-suite result is counted as passed.

Parser cases include available clean output, both lanes, destination/source ordering, conflict
states, whitespace/newlines/non-UTF8 names, owning copies, malformed/truncated/duplicate records,
invalid paths and exact path/count/output bounds. Actual disposable Git repositories cover clean,
staged rename, external edits, Unicode names, merge conflicts, linked worktrees and unchanged
index/source bytes. Deliberately malformed/flooding/sleeping native executables are negative
fault fixtures only; successful provider observations come from real Git. Missing tool/corrupt
HEAD preserve last-good; queued/running cancellation, permanent stale-scope suppression,
Shutdown and retained snapshots pass. No unrelated repository or PR is changed by these tests.

After recovery, private one-shot allocation faults were added using the existing private test-seam
convention. Intake, consumed outcome-copy, partial decoder storage and capture construction
failures preserve exact last-good and drain jobs; a complete retry recovers. The final actual
product focused tests pass 2/2 in 0.45 s, including all allocation cases. No callback or allocator
pointer is retained by the fault seams; captures must still be released before SDK library unload.

## Complete delivery gate

Final source: Development Modular, Editor/graphical shell and verification-only ProjectPlayer ON,
native ProjectPlayer/Slang/Zig OFF, pinned local ImGui/Vulkan-header dependency cache. After the
initial 773-step build, fault-test builds and a fresh final configure/build passed. Minimal
Monolithic Shipping configure/build passed 102 steps; that profile strips Editor by policy.

```sh
cmake --preset linux-development
cmake --build --preset linux-development
cmake --preset linux-shipping -DNEXORA_ENABLE_SLANG=OFF -DNEXORA_ENABLE_ZIG_GAMEPLAY=OFF
cmake --build --preset linux-shipping
CTEST_PARALLEL_LEVEL=1 ctest --preset linux-development
```

Complete CTest passed 291 registered: 279 actual passes, 12 existing native feature/environment
skips, zero failures, 739.20 s. Parser passed 0.03 s and actual Git provider 0.48 s, including all
native fault/allocator/cancellation/stale/teardown branches. The 100k hierarchy lifecycle passed
289.73 s with original timeout multiplier 1; timing variance is not repaired or a production
performance budget. Source hashes remained unchanged during the gate.

Documentation tests 16/16, Markdown/local links/bilingual pairing, touched C++ formatting and
diff checks pass before publication. Hosted platform outcomes belong to the PR checks, not this
Linux record. Graphical status/diff/merge, full M4-06, Editor milestones and physical-host
acceptance remain open.

## Current-main integration

Main `78a0d429fc9317668baad1e623273de2646d463d` adds the independently delivered mesh thumbnail
provider. The normal integration completed Development configure/build (318 steps), Shipping
configure/build and the entire Linux gate: 292 registered, 280 actual passes, 12 existing skips,
zero failures, 623.81 s. Actual Git provider passed 0.43 s, mesh thumbnails 0.14 s and the 100k
hierarchy lifecycle 235.25 s at the original timeout multiplier 1. No assertion/deadline was relaxed.

Main then advanced to `ecfd42563a3ecdae9674090a23ceb22e870251ea` with validated Chrome self elapsed
intervals. That baseline is integrated normally before publication; its full Linux gate is in
progress. No result from the earlier baseline is substituted for that integration gate.

### Final delivery baseline

The final branch also integrates main `8fd501c46d9736ce5295c0ee12991eeca6518e20` (checked StaticView
report output and bounded diagnostics). Fresh Development configure/build passed 317 steps;
Minimal Shipping configure/build passed the 9-step metadata integration after the earlier complete
102-step build. All five provider/parser/fault source hashes remain unchanged.

The final complete Linux gate passed 294 registered tests: 282 actual passes, 12 existing native
feature/environment skips, zero failures, 714.94 s. Actual Git provider passed 0.45 s and the
unchanged 100k lifecycle 247.79 s at timeout multiplier 1. The full LastTest log is archived outside
the repository. Documentation/format/diff checks are repeated before publication; hosted outcomes
remain separately recorded on the PR. No graphical source-control or full milestone claim is made.
