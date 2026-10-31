# ED-WF-04: transactional Runtime and typed Zig reload

Status: **in progress, not accepted**. [Issue #745](https://github.com/jimlee1972/Nexora/issues/745),
Epic #741, Beads `nexora-whr.4`. Implementation starts from latest merged main
`70be4f773135e6975a0a7d996b36dcc952e6b087` after ED-WF-03/#756 acceptance, retaining the same
Editor branch and isolated worktree. Accepted progress remains **3/9**, graphical Editor **0/8**.

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
