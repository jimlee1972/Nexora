# Packaged native game candidate — Windows 2026-10-26

Scope: the native Player slice of phase 4 in the
[scene delivery plan](../../../Roadmap/en/Project_Scene_Delivery_Plan.md).
The code is subsequent work on merged PR #718, not part of that Runtime-only PR's acceptance.
Shared build/CLI and zero-argument launch now have additional candidate evidence below;
Editor Play, graphical build coordination and final Editor ZIP remain open.

## Candidate and verification boundary

`--run-game-package` loads the configured startup from NXGAME and attaches the actual SDK Zig
module after its first presented frame. Requests execute after callbacks. The candidate World,
geometry/material palette and camera are admitted, and the native draw records successfully,
before allocation-free World/palette publication. Surface-owned uploads/resident geometry retain
their own fence lifetime; CPU palette spans expire after DrawScene. Stop cancels pending work,
then module Stop/Destroy finish while session/services remain alive, followed by surface drain.
Camera-less scenes use an isolated default camera; empty scenes present the clear background.
StaticView still requires a camera and nonempty geometry. Backend/device failure remains fatal.

The existing native StaticView CPU admission test also guards the separate game fallback/empty
policy. Selected local tests passed 4/4, zero failures/skips, 4.42 s:
`runtime.game_scene_session`, `runtime.game_scene_zig_module`,
`runtime.game_project_player_cli`, `runtime.native_static_view`.
Configure/build/test logs: `build/scene-settings/native-game-20261026-023208-*`.
Build targets included the real Player, session fixture, three SDK modules and legacy native tests.

## Actual Windows desktop DX12 runs

These ran on the developer PC's actual desktop, using hardware (`software_rasterizer=false`),
not the restricted tool session, VM or clean host. Each retained a successful exit, complete
flushed JSON, stdout/stderr and package/player SHA-256 in
`build/scene-settings/native-runs/desktop-20261026-023333-{case}.*`.

| Case | Actual observation |
| --- | --- |
| good | Non-Main Level01 startup → Title → Level01; 100 presents/draws, 3 commits including startup, 99 module updates; normal Stop/Destroy/drain |
| bad-camera | Valid package with a Title FOV that becomes invalid after native float narrowing; 1 rejected preparation, script confirms exact startup instance/camera retained, then retries Level01; 100 presents/draws, 2 commits, exit 0 |
| stress | Actual SDK module requests 120 alternating replacements; 300 presents/draws, 121 commits including startup, 299 module updates; retained World remains 1 scene / 1 instance / 3 entities |
| empty | Configured camera-less empty startup; 4 successful clear-frame presents, 0 mesh draw calls, 1 scene/instance, 0 entities, reported camera 0, no module; drain/exit 0 |

All nonempty cases retain one geometry upload, 65,536 backend native-allocation bytes, 150 CPU
comparison-key bytes and 3 scene texture uploads. Good/failure runs report 99 geometry hits;
stress reports 299 hits. These are copied backend allocation/cache counters, not physical VRAM
measurements. After every run the process exited; no residual NexoraProjectPlayer remained.
The caller used an unrelated CWD and generated cooked fixtures without authored scene files.
Complete application relocation/source removal remains a separate delivery gate.

`build/scene-settings/native-source-inputs.json` records matching candidate/isolated source inputs
and binary hashes. The Player SHA-256 for this native slice, before later launcher/build work, is
`0ebc7c5386513f71e8e785f6a36fafb27d9e57cf1a70743210e5990338968224`.

## Preserved failures and remaining acceptance

The first restricted-session execution recorded a bounded presentation timeout; it could not
present on the user's desktop. Real desktop execution succeeded separately. Earlier native
attempts and raw logs remain under their original names. A localized MSVC include-prefix encoding
error in the isolated CMake cache prevented reliable incremental header tracking. Its original
files and a compiler-prefix probe remain under `build/scene-settings/dependency-prefix-before`.
The exact compiler output prefix was restored, affected translation units were rebuilt, and
Ninja now records 61 valid dependencies for the Player object, including NativeGamePlayer.h and
StaticView.h. The final native runs use the newly linked executable; no old-binary result is
substituted for this source.

The visual module also completed 1,200 hardware presents and the same two transitions. However,
Windows screenshot capture returned `Computer Use app approval timed out`; no captured-pixel
claim is made. A new Linux Vulkan/Xvfb test observes actual left → right → left red-mesh pixels,
failure conservation, 120 transitions and empty startup, but it has not yet run. Full exact-head
Linux Development/Shipping, sanitizer/feature-off/platform gates and Windows GUI pixels remain
pending for this candidate. Scene-delivery acceptance is 3/5 solely after the separate Runtime
PR #718 full CI/merge; complete graphical Editor remains 0/8.

## Zero-argument relocated game candidate

The later launcher uses fixed adjacent `Game.launch`, `Game.nxgame` and optional Game module;
normal launch requires no user arguments. Metadata is an exact bounded 24-byte wire descriptor.
The developer-PC relocation copied renamed Game.exe, the actual SDK module, package, launcher,
LICENSE and 11 dependency DLLs (seven Nexora, four CRT), with no scene/engine/game sources.
It ran from a foreign CWD containing invalid decoy launcher/package files, then moved to
`build/scene-settings/relocated-零參數 game µ` and ran again from that foreign CWD.
Both runs exited 0 after 100 hardware frames, 99 updates and three startup/A/B/A commits,
drained and preserved every artifact hash. Receipts, SHA-256, exact commands and stdout/stderr:
`build/scene-settings/native-runs/argless-{relocated,unicode}.*`.

These verify manually assembled source-free game relocation on this developer PC, not the
new shared producer, a clean host or the final Editor ZIP. The new producer is separately
covered in the [shared build evidence](SharedGameBuild-Windows-2026-10-26.md).
