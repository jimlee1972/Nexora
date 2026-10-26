# Packaged Play host prerequisite — Windows 2026-10-26

This is subsequent uncommitted phase-4 integration work, separate from PR #720's common producer.
The main Editor Play command is now wired to the candidate; native pixels and full phase 4 remain unaccepted.

`PlaySession::StartExternal` borrows the host-owned isolated Play World instead of cloning the
currently edited Scene. Pause/Step/input/failure/inspection policy is shared. External/remapped
IDs cannot apply transforms back to authoring; Stop releases the borrow without destroying its
World. Old owned-clone Play and parenting/apply-back tests remain unchanged and pass.

The existing Editor V3 module host accepts a complete copied Scene V1 table. It advertises the
optional service, retains it through module Stop/Destroy, then clears it. Incomplete tables and
conflicting custom-2D binding reject. Existing legacy/2D providers retain their behavior.

The native scene palette moved without algorithm changes to Apps/Common/NativeSceneView.h;
the Player wrapper retains its names and old native StaticView test. PackagedPlayContext owns a
Play GameWorld, verified catalog, common Runtime session and palette. It stages all CPU owners
before invoking the host's draw callback, then commits after native preparation. CPU admission
rejection redraws the old World/palette; native failure returns its status with the old World kept.
Actual render surfaces own the fence uploads; context descriptors are borrowed only for Draw.

MSVC x64 local configure/build plus nine selected tests pass, zero failures/skips, 9.35 s:
`runtime.entity_parenting`, `api.gameplay_host_bridge`, `runtime.external_play_session`,
`runtime.v1_m6_editor_sdk`, `runtime.native_static_view`, `editor.play_gameplay_module`,
`editor.packaged_play_services`, `editor.play_diagnostics`, `editor.play_scene_services`.
Targets include the Editor, Player, actual SDK fixture and corresponding test executables.
Logs: `build/scene-settings/packaged-play-20261026-055125-*`; latest receipt exit 0.

The actual SDK DLL starts configured Level01 while the isolated authoring World is named Title,
requests Title and Level01 after callbacks, and observes fresh scene IDs with one retained scene
and three entities. Native **CPU data** snapshots preserve the shared mesh, scalar PBR and affine
instance, with camera X sequence 10/1/10. Failed float-camera admission and a rejected draw marker
preserve the old serialized World/camera/palette, which can draw again. The draw markers are CPU
stand-ins, not native GPU/pixel acceptance. Stop still sees the scene service; SDK allocations are
paired, and the authoring bytes and empty transform apply-back stay unchanged throughout.

An earlier test incorrectly counted Process success with no pending request as a scene commit;
it now counts actual primary-ID changes. The original failed log is retained. Production request
semantics and assertions were not weakened. Complete new exact-head Linux/Shipping/CI and main
application startup/save decisions, rendering and final Editor ZIP remain pending.


## Main command and graphical Build candidate

The main Play command now captures/cooks saved configured scenes and owns a PackagedPlayContext;
module loading follows the first admitted native draw. The same session survives scene changes.
Stop/exception teardown stops requests before module Stop/Destroy and releases the Play borrow
before the owner. Dirty decisions offer Save/Use Saved/Cancel; Save As continues only the matching
original project/document, with cancel/stale dialog state discarding continuation. Graphical Build
runs an owning request on a worker and publishes on the owner thread after source/document/content/
project guards; Cancel is cooperative. Build controls, busy/cancel and all three saved choices are
exercised at 1x/2x. The final toolchain bundle and end-to-end graphical Build remain pending.

Local gate `packaged-play-20261026-062152` passed 10/10 with zero failures/skips in 13.68 s,
including `editor.project_build_settings_ui`. Two actual Windows graphical Editor runs exercised
configured Level01, Title and Level01 through SDK requests, with 1,200 acquired/presented frames,
2,397 scene draws, three commits, exit 0 and exact source hash conservation. Receipts/logs:
`editor-packaged-20261026-061456.*` and `editor-red-20261026-062322.*` under build/scene-settings.
The second uses the same red-material project generator as the new Linux native pixel test.
These runs use `--play` after graphical startup, which enters the same Play command handler;
they do not claim a physical Play-button click or Windows pixel readback.

Computer Use captured neither window state nor pixels: the original window capture and a fresh
selection/activation recovery both timed out. No alternate Windows UI automation was used.
The new `editor.linux_packaged_play` case requires actual Game viewport red centroids left/right/
left, three commits, normal close and unchanged saved sources under Vulkan/Xvfb. It is registered
only when the actual SDK visual fixture and display tools exist. It has not yet run on Linux.


The final saved-dialog/owner-worker candidate passes the 10-test host/UI gate at
`packaged-play-20261026-062508` (13.55 s) and the 14-test shared producer/ABI/CLI gate at
`phase4-gate-20261026-062708` (143.49 s), zero failures/skips. The actual scene-only and SDK
script producers now run through GameBuildCoordinator's owning worker, reject a duplicate Start,
return an owning result, preserve prior output under owner/source/tamper guards, and exercise
worker cancellation without publication. This covers the worker plus producer; a complete GUI
button-to-final-publication display run remains separate. Documentation routing tests pass 16/16
with the actual Python executable alias; changed-document validation reports zero errors. The
first Windows attempt hit the OS python3 app alias (WinError 1920); original logs remain retained.


## Installed-tool candidate

The shared CLI now resolves the same executable-relative runtime/tools as the GUI when explicit
profile/compiler options are omitted. A private Runtime directory holds the game host and flat
DLL inventory; Tools holds CMake/Ninja/Zig and their required compiler/module resources, with
licenses. No Nexora/game source checkout, examples or extra 2D applications are staged.
The optional packager accepts the explicit trusted tool roots and preserves the original minimal
Editor startup packaging mode. Five portable packaging admission tests pass. The added installed
CLI regression and real worker producer pass 14/14 in 128.32 s at `phase4-gate-20261026-063534`.

Actual candidate `candidate-editor-tools-20261026-063747` runs from a foreign CWD with PATH
restricted to Windows system directories. Its default browser presents eight frames; its default
installed-tool CLI builds/tests the actual Zig script and multi-scene game. Build.commands proves
all CMake/Ninja/Zig paths use the candidate's Tools tree. After hiding the fixture source directory,
the produced game executes 100 hardware DX12 frames, 99 module updates, Level01/Title/Level01,
three commits, one geometry upload, 99 geometry hits, stable one-scene/one-instance/two-entity state,
and exit 0/drain. All 25 output hashes remain unchanged. Receipt:
`build/scene-settings/portable-editor-20261026-064016.json` and corresponding portable-*.logs.
This is an uncommitted candidate, not a final ZIP or captured Windows pixel acceptance.

The Linux display CI job previously omitted Zig; its success could not prove the SDK-driven
pixel tests were registered. The candidate now pins Zig there, explicitly requires both the
standalone packaged-player and Editor packaged-Play tests, executes both, and retains their logs.
The Editor case additionally uses actual submitted Build menu/settings/script/Build controls,
waits for the worker-published inventory and verifies the produced module ABI. These new native
GUI cases have not yet run. They do not bypass their pixel, source-conservation or native-close
oracles. Actual control positions/busy state are copied only in opt-in test-build observations.


The standard candidate packager verifier also passes from a fresh Unicode temporary installation:
`candidate-relocation-20261026-064717/result.json` and `game-build.json`, with restricted PATH,
help/default browser/create/read-only reopen/duplicate rejection, 120-frame graphical open and
8-frame graphical creation, installed-tool script compilation, source hiding, 100 native frames,
three commits, exit 0/drain and output hashes unchanged. The larger red-triangle native Editor
fixture subsequently passes 1,200 frames/three commits/source conservation at `editor-red-20261026-065219`.
These remain candidate execution/packaging receipts, not a final committed-source ZIP.

PR #720's common producer/native-launcher slice merged as be8477d8 after all 41 exact-head checks
passed. Full Linux Development: 466 registered, zero failures, existing font skip, 1744.77 s;
Windows: 405, 1103.38 s; macOS: 384, 989.98 s. Linux Shipping, sanitizer/TSan, feature-off and
native/platform gates passed. Complete check receipt: build/scene-settings/phase4-ci720-final.json;
selected full job logs retained there. New Editor integration remains unaccepted pending its own
exact-head CI and explicitly registered SDK-driven native pixel tests. Uncommitted work was safely
stashed/restored on the same branch over the merged main; unrelated PR719 SDK changes are retained.

A local rebuild overlapped the owned 1,200-frame Editor run, causing the Windows loader to hold
NexoraRuntime.dll and MSVC LNK1104. This was execution ordering, not a source fix; the original
phase4-gate-20261026-065205 failure is retained. Editor exited normally; subsequent build/run
operations are serialized and the clean retry must pass before source publication.


Detailed inspection confirms PR720's full Linux Development job did execute the standalone SDK
pixel case (8.13 s, 78/91/78 actual red samples, left/right/left), even though its separate display
job lacked Zig. The earlier pending-native-pixel statements describe the prior candidate point;
standalone native pixels are now accepted. New Editor pixels/GUI Build remain unrun. New host
admission handling also redraws the retained scene after Unsupported/InvalidDescriptor candidate
rejection, but never retries a lost device. CPU callback tests verify unchanged World and continued
old rendering for both rejection statuses, and exactly one draw call on DeviceLost. Latest 10/10
host/UI gate at packaged-play-20261026-065936 passes in 13.56 s, zero failures/skips. These markers
remain distinct from actual GPU fault injection. The post-lock clean 14/14 retry passed at
phase4-gate-20261026-065302, 116.07 s; final common-native-admission revision is revalidated separately.


Final admission source revision passes the 14-test producer/native-data/ABI/CLI gate at
`phase4-gate-20261026-070201`, 115.07 s, zero failures/skips. Source-input receipt
`build/scene-settings/editor-integration-source-inputs.json` compares all 24 changed C++/SDK/CMake
inputs to the isolated build with no mismatches. Five packaging admission tests and 12 changed
Markdown documents validate without errors; the 16 documentation-routing unit tests also passed.
The final common fixture additionally contains a disabled third scene; both the new graphical
Build case and installed-tool verifier require exactly two scene records and four union assets
(two scene payloads, one mesh, one material), making exclusion/deduplication explicit. That final
fixture/data extension is validated again by integrated native/packaging gates before delivery.


Latest compiled native Player revision also passes actual hardware DX12 good/bad-camera/stress/
empty runs at receipt prefix `native-runs/desktop-20261026-070923`: 100-frame A/B/A; one rejected
candidate retaining startup/camera then retry; 300 frames/121 commits/one upload/299 hits with one
scene, one instance and three entities; four-frame empty/default-camera startup. All exit 0/drain,
software_rasterizer=false. Exact binary/package hashes, commands/stdout/stderr are retained. This
revalidates actual execution after sharing the admission retry rule; it does not claim a real GPU
allocation/device-loss injection or captured Windows pixels.


Cancellation regression follow-up restores the known-valid authored script before both the
pre-cancelled producer and background-worker cancellation checks. Thus syntax failure cannot
masquerade as cancellation. The actual scene-only/script/CLI worker suite passes the focused
`editor.game_build` gate at `cancel-worker-20261026-072337`, 172.69 s, zero failures/skips.
This changes test preconditions only; production sources and package behavior are unchanged.
The earlier intermediate 14-test run also passed at phase4-gate-20261026-071925 (167.59 s).


## PR721 POSIX development-profile correction

Head 60055300 macOS job 121324539926 failed only editor.game_build: the new default-installed-tool
CLI looked beside Apps/Editor/NexoraEditor, but GameRuntime.profile belongs beside the separate
Apps/ProjectPlayer executable. Full original log: build/scene-settings/ci721-macos-first-failed.log.
This is not accepted or skipped. Windows uses a common bin directory and had passed the same case.

The correction generates the actual Player profile path after app targets are defined. Unmarked
development builds may use it; packaged build-info.json markers force internal Runtime/profile
and Tools paths, so missing shipped dependencies cannot be masked by a developer checkout. Actual
worker/CLI tests exercise a separate unmarked development directory and reject an incomplete marked
installation. Generated private headers are per-configuration; a configure-only harness includes the
actual Apps/Editor CMake file, validates three Ninja Multi-Config profile paths and no-Player generation.
Logs: profile-layout-config-{multi,no-player}.*. The finalized Windows 14-test gate passes at
phase4-gate-20261026-080220, 130.66 s, zero failures/skips. New Linux pixel evidence writes its
successful Play-stage receipt before graphical Build, retaining independent observations on failure.

The earlier f65acc36 ZIP remains a candidate; it is not the final artifact after this production
lookup correction. Its actual extraction nevertheless passed strict inventory and Windows startup:
extracted-editor-acceptance-20261026-074908 records exact zero-argument graphical initialization/
continued process lifetime (controlled process cleanup, not graceful-exit proof), a normal eight-frame
browser run, and default-Scene-mode packaged Play with 1,200 presents/three commits and all saved
sources unchanged. Captured Windows pixels and clean host remain unclaimed. A new source-matched ZIP
and complete updated-head CI are required before final delivery.
