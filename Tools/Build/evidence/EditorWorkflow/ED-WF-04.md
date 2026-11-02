# ED-WF-04: transactional Runtime and typed Zig reload

Status: **✅ accepted in merged [PR #758](https://github.com/jimlee1972/Nexora/pull/758)**.
Issue #745 and Beads `nexora-whr.4` are closed. Validated head `278fea636750d9aa2065c2a0075ca4c6791ae859`
and squash `c494eff255ec652e89e0577f5db1ae34fb66a7cc` share tree
`76f732f85e7c74a118d00496ca4b1c4066288783`. Accepted workflow count is **4/9 (44.4%)**;
graphical Editor milestones remain **0/8**.

## Final acceptance

The [Cloud receipt](ED-WF-04-Cloud.json) records all five exact-source Development/Shipping gates
first exit 0; Development CTest **495/495, zero skips/failures, 1897.30s**, clean source, OOM 0 and
unchanged previous archives. Archive SHA256 is
`08e61d10bbd1ec7108a8550e2bc6a5cc5acd47a16d355ed85f7178b06fe6cbcc` (378837 bytes).
The trailing archive-shell `ps|rg` exit 1 meant no matching processes; a separate postflight exited
0 with zero residual processes. The original archive and all gate exit codes were preserved.

[Final CI](ED-WF-04-CI.json) records all **42 successful checks/four completed workflows** before
squash merge and identical merged tree. Native Windows Runtime regression passed 3/3 in11.19s;
expanded historical scoped 21/21 and6/6 retain their exact-source scope. Latest hosted Windows
startup/branding/input/no-console passed5/5 in29.63s; intermediate package artifact13068967782
wrapper SHA256 is `b13dfcfe02aaab1d8012655a2ca710082b3bf1dc5dd2c1488ed53389b0dae111`.
Native macOS job124742723908 registered411:410 pass/one existing optional font-dependency skip/zero
failures in1096.58s; real Zig reload/unload/pinned-image admission passed in26.18s. Complete decoded
logs and hashes are retained in the task build area and GitHub Actions. These are hosted/native
proofs, not physical clean-host acceptance. The final nine-work-order ZIP is not accepted here.

Editor coordination05, source workflows06/07, animation/pools08 and integrated delivery09 remain
open. The following implementation and failed/repaired candidate observations are historical;
the final current acceptance is this section and its receipts.

## Typed data boundary

The SDK now exposes paired `saveState`/`loadState`, nonzero game schema versions and explicit
StateWriter/StateReader operations through the existing V3 slots/capability. NXGS codec version 1
and exact payload length bound the complete frame to 1 MiB. No Game/native pointer dump is generated.
Integer widths/endianness, finite floats, canonical booleans, checked byte slices, const deterministic
size/write serialization and borrowed reader lifetimes are documented in
[Tools/Zig](../../../Zig/README.md). Scripts without the pair keep null slots; one missing hook is
a compile error. C/Zig table layouts remain unchanged.

## Scoped Windows codec verification

Zig 0.14.0 on the existing Windows developer host:

```powershell
zig fmt Bindings/Zig/game.zig Tests/TwoD/GameScriptAdapter.zig
ctest --test-dir build/editor-workflow-native -C Development --output-on-failure --output-log build/editor-workflow-native/ed-wf04-zig-adapter-attempt1.log -R '^gameplay.zig_game_script_adapter$'
```

First attempt: **1/1 CTest passed**, exit 0, 19.73s (19.87s total). The actual Zig test runner
covers lifecycle/allocation failures and old empty scripts, plus explicit counter/scene/entity/
float/bool/bytes round-trip, exact framing, all truncation lengths, invalid magic/codec/schema/
payload/boolean/slice lengths, trailing data, null/oversize admission and writer limits/NaN.
The unit fixture's 32 updates are not 32 dynamic reloads and do not accept the work order.

## Runtime foundation verification

The Runtime now closes new registered work and waits for actual leases before state capture;
leases hold a shared registry through final release. Generation IDs are not recycled on reload of
the same owner. Stable per-generation host tables remain alive through callbacks and library
finalizers, including failed candidates. The strict transaction overload requires complete
begin/rollback/retire/commit hooks, quarantines retirement and reports restart-required when
isolation/migration is absent. Legacy module-state-only constructors remain source-compatible;
they do not claim arbitrary World/resource rollback. Editor integration must use the strict owner.
State size queries are bounded to 1 MiB and replacement failures preserve old generation/status.

Native build logs: `build/editor-workflow-native/ed-wf04-runtime-build-attempt*.log`.
Initial compile rejected Windows' `max` macro; this source now defines NOMINMAX before Windows.h.
The second and third incremental Runtime builds exited 0. The first two-test execution then exited
8: host passed, dynamic discovery correctly failed because the target-only build lacked the three
DLL fixtures. Explicit test-target dependencies now build those fixtures. The fourth build and
second baseline CTest both exited 0: **2/2 passed**, host 0.08s and dynamic generations 0.70s
(0.92s total). Barrier-test setup now clears the previous observed generation to require the
current wait rather than accepting an old notification. No assertion/deadline was weakened.

The new real-World C++ transaction fixture initially passed **1/1**, exit 0, 0.11s (0.26s total):
partial begin/create/restore/start failures, oversize state, bad ABI, absent migration/isolation,
old-generation retention, a real registered worker and a retired Stop that destroys its own
entity while the transferred live handle survives. Its owner-thread adjustment subsequently
passed again (0.15s, total 0.33s). This C++ fixture is separate from real Zig acceptance below.

## Real Zig/native DLL verification (2026-11-01, Asia/Taipei)

Six genuine script-only Zig libraries are built through the same SDK CMake entry (normal,
restore/init/start failure, changed schema, missing migration). Candidate filenames are distinct,
including Unicode/space paths. A native C++ bad-ABI library and a failing library with a real
static destructor provide ABI/finalizer injection; those two negative libraries are not claimed
as authored Zig. No probe handle artificially retains a retired DLL.

The first combined six-test run finished exit 8, **5 passed/1 failed**, 119.05s. The real Zig
fixture failed its first start before any reload: its host had already activated the scene, while
World::Activate correctly requires LoadedInactive. DLL loading/exports were independently verified;
a diagnostic rerun retained the same start failure with balanced allocation cleanup. The fixture
now starts with an inactive scene and creates a fresh Play World for final unload/load. The original
Load/activation/state assertions remain. No product activation behavior or deadline changed.

After correction the real Zig test passed 32 replacements/192 expected rejections in 9.15s,
alongside the World fixture (2/2, exit 0, 9.44s total). Finalizer injection then expanded the checks:

```powershell
cmake --build build/editor-workflow-native --config Development --target NexoraZigReloadTransactionTests NexoraGameplayReloadTransactionTests NexoraGameplayModuleHostTests NexoraDynamicGameplayModuleTests -j 4
ctest --test-dir build/editor-workflow-native -C Development --output-on-failure --output-log build/editor-workflow-native/ed-wf04-final-native-core-attempt1.log -R '^(runtime\.(gameplay_module_host|dynamic_gameplay_module|gameplay_reload_transaction|zig_reload_transaction)|gameplay\.(zig_game_script_adapter|zig_script_project))$'
```

Final combined native gate: **6/6 passed, exit 0, 136.98s**. Host 0.06s, World transaction 0.11s,
native dynamic baseline 0.65s, real Zig reload 10.36s, codec 4.59s, external script project 121.04s.
The real Zig test proves **32 successful unique-DLL replacements and 224 expected rejections**,
counter/scene/entity continuity, old implementation callable after every rejection, actual leased
old-generation Zig work adding 100 ticks before capture, retired Stop isolation, unloaded old DLLs,
32 failed-candidate static-destructor callbacks with valid host/staging context, complete allocation
cleanup and non-recycled generations through unload/load. Reopening the active loader handle is
rejected before loader/create can corrupt shared globals. These are native module/World checks,
not an Editor GUI, animation/pool or physical-device acceptance claim.

The external project test additionally compiles and rejects one-sided hooks, zero schema and
native-pointer encoding, while retaining unchanged-source and authored-test/traversal failures.
Logs remain under ignored `build/editor-workflow-native/ed-wf04-*`; expected/real failed attempts
are not discarded. Markdown link/bilingual checks passed for 12 current documents and three JSON
receipts parsed after reusing the existing pinned task-local markdown-it/mdurl dependency path.

## Acceptance boundary

### Current-main integration and retained failures

After the owned-host prefix bound was tightened, the six-test native gate passed again:
**6/6, exit 0, 126.93s** (host 0.07s, World 0.12s, dynamic 0.66s, real Zig 10.13s,
codec 6.46s, script project 109.32s). The candidate host prefix is clamped after staging as well.
An initial consumer build hit a raw Zig cache access denial; task-local global/local Zig caches
resolved the environmental error and all 16 consumer targets subsequently built successfully.

The worktree then integrated newer main `a21c1ca2293e4200de8dcaa4b892ffef34687c9c` (#757 themes).
Its 17 consumer targets built successfully. On the Default Windows desktop, the unchanged
17-test Editor regression finished **exit 8, 16 passed/1 failed, 334.04s**. The real GUI Build
workflow passed (302.76s), as did Play/services/2D, both appearance scales and the native splash.
Only `editor.windows_startup_integration` failed (10.64s): the first revealed Project Browser
had insufficient bright UI regions. The retained bitmap and startup log showed correct hidden/
first-frame/splash/reveal sequencing but an overly dark palette.

The appearance update decoded display swatches to linear values unconditionally. DX12/Metal's
mixed Scene/UI surfaces use legacy UNORM attachments, whereas Vulkan can negotiate an sRGB
attachment. The repair reports the actual attachment transfer through SurfaceDiagnostics and
sets a per-context theme encoding before each UI frame. Palette values are decoded only for
an attachment that encodes sRGB. Meaningful secondary text also gains 4.5:1 WindowBg contrast.
CPU tests cover both themes on both encodings and unchanged DPI geometry. The original startup
test, image assertions, dataset and deadlines remain unchanged. Scene/Game shaders, assets and
the mixed-surface mode remain unchanged.

The repair build exited 0. The first unchanged three-test retry failed all three: the expanded
CPU setup had left Light selected before an existing assertion requiring Dark, and the startup
SHOW-event capture was occluded by another foreground application. Explicitly restoring Dark
before the original menu-input assertions corrected only the new setup; all original assertions
remain. A second startup-only retry retained the same occluded SHOW capture (exit 1, 8.50s).
A third run temporarily minimized and restored that identified foreground window without closing
it: both appearance scales passed (8.69s/7.79s), but startup failed its unchanged foreground/input
check (19.50s; whole run exit 1, 36.35s). The capture still showed another application's pixels,
so no genuine Editor scanout pass is inferred from it.

A separate no-argument launch through Computer Use exposed one responsive Editor window, but
Windows.Graphics.Capture failed twice (`FrameArrived timed out` / `window capture timed out`).
The probe process was cleaned up after checking its exact PID/path. Read-only diagnostics report
Default input desktop, active console session 64 and WTSActive; these do not prove usable native
capture/input. An interactive-desktop retry remains pending. The failing evidence is retained;
no capture threshold, deadline or startup acceptance was weakened to bypass this limitation.

The Runtime/data foundation has scoped passes above. Rebuilt Editor/Player regression, complete
Codex Cloud Development configure/build/CTest, linkage-sensitive Shipping configure/build,
exact-head CI and a merged PR are required before acceptance. Editor background watcher/build
coordination belongs to ED-WF-05; Sprite/pool integrated acceptance remains ED-WF-06–09.

## Published candidate 9547cc8a: retained Windows passes and macOS failure

Draft [PR #758](https://github.com/jimlee1972/Nexora/pull/758) initially published commit
`9547cc8afa1e4c69d5dd79b5e54e5c358cf152da`, tree `3f9ae84f244ee7e4e27e1007cdfa4a569c9d98f9`.
All 21 scoped Windows consumer targets built. The first current-main aggregate retained a splash
capture failure on the isolated desktop (20 passed/1 failed, exit 1, 197.32s). On the Default
desktop, the unchanged splash test passed separately (1.04s total), then the unchanged complete
scoped gate passed **21/21, exit 0, 166.08s**. The exact commands, toolchain, test inventory and
binary/source hashes are in [the scoped Windows receipt](ED-WF-04-Windows.json).

[Hosted Windows package evidence](ED-WF-04-Windows-CI.json) is separate from that local scope:
run `41226660454`, job `124612117854` passed native branding/startup/input/no-console checks
**5/5, exit 0, 21.27s**, including startup integration 18.97s. The actual extracted/relocated
Editor and game passed as well; extracted Play rendered 1,200 frames with three scene commits
and unchanged source hashes. The captured Dark WindowBg is RGB (56,56,56), matching the intended
display gray rather than the over-dark decoded value. The package root has only NexoraEditor.exe.
Artifact `13052765182` has SHA256 `eab5c490a1d220952d4fa6e6346248dd8b71bff86821db5eadb2ca9d8e8ddaa4`;
the inner Editor ZIP is 115756541 bytes, SHA256
`ef3929c63e6f7bfb5e5cbc445a50f7f1cc67faa508e6746ee7f7affd4e59a8ba`.
This is a work-order candidate, not the final delivery or physical/clean-host acceptance.

The [initial candidate's complete Cloud receipt](ED-WF-04-Cloud-Initial.json) records all five
gates first-attempt exit 0: Development configure 2.326s/build 517.542s/CTest 1991.496s,
Shipping configure 0.737s/build 67.244s. CTest itself reports **495/495 passed, no failure/skip,
1991.38s**; the real Zig transaction took 6.56422s. Source remained clean, OOM count was zero,
and prior #754/#756 archives were unchanged. The 379114-byte archive has SHA256
`6ab758ffdf8b5569937cce488ae6db734398152fbbd3564bb6a0350883c50e71`.
Shipping testing is OFF. This Linux-only pass does not override the macOS failure below or
validate the subsequent repair. The [initial full CI inventory](ED-WF-04-CI-Initial.json) has
42 completed checks: both macOS desktop events and their two result aggregators failed;
the remaining checks succeeded. No failed check is bypassed.

macOS desktop job `124612286930` then finished **exit 8, 409 passed/1 skipped/1 failed of 411,
885.22s**. The unchanged portable-font fixture skipped for its optional Python dependencies.
`runtime.zig_reload_transaction` aborted at the retained-candidate mapping assertion (0.34s).
The original decoded log is preserved locally as `ed-wf04-macos-ci-9547-failure.log`; the
[failure/probe receipt](ED-WF-04-MacOS.json) records its hash and scope. The failed gate is not
treated as an accepted platform result.

## macOS unloadability repair: native verification pending

Apple's [dlclose manual](https://github.com/apple-oss-distributions/dyld/blob/main/doc/man/man3/dlclose.3)
documents TLS/Objective-C images as non-unloadable. A Windows-hosted **cross-compilation only**
of the real Zig SDK entry for aarch64-macos found `__thread_bss`/`__thread_vars` from the default
panic recursion counter. A probe using Zig's standard message-and-trap panic namespace removed
both sections. The SDK now uses that handler only for non-test macOS entries; safety checks and
fatal panic behavior remain, while tests/other platforms keep the full handler. This object-file
probe does not substitute for actual macOS loading/unloading.

Strict Runtime dynamic transactions now reject known TLS/Objective-C-pinned macOS images before
initial creation or replacement staging, reporting restart required. Legacy constructors retain
their loading/reload contract. The added C++ TLS library fixture requires initial rejection without
host allocation and rejected replacement with the old World/generation still callable; execution
of that additional macOS fixture remains pending. The existing
32 replacements, 224 ordinary rejections, unload/finalizer assertions and deadlines remain intact;
failure diagnostics now identify the round/case/mapping. No failing assertion was removed.

Applicable native regression, full exact-source Cloud Development/Shipping and all CI must pass
again for this repair before PR #758 can merge. Progress remains **3/9**.

The repaired source's [scoped Windows core receipt](ED-WF-04-Windows-Fix.json) now records
**6/6 passed, exit 0, 142.81s** after successful native rebuilds. Real Zig reload took 11.26s,
codec 8.17s and authored external project 121.97s. This is native Windows module/World/SDK proof,
not execution of the additional Apple-only fixture. The actual modified SDK entry also compiled
to an aarch64-macos object with no TLS sections (1116784 bytes; SHA256
`a69fe848e3747f0830082867192aefc576d374efb1bbcda4fcd84aa61c26f6f5`).
Native macOS, complete repaired-source Cloud gates and CI remain pending.
