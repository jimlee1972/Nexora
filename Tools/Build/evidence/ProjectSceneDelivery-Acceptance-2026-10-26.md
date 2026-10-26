# Project scene delivery: integrated acceptance

✅ The five scoped phases in the [delivery plan](../../../Roadmap/en/Project_Scene_Delivery_Plan.md)
are accepted. [PR #721](https://github.com/jimlee1972/Nexora/pull/721) source
`f327bd86260a2c59271c7120ca8bfd9457a461eb` passed all **41 exact-head checks**, then squash merged
as `e0036e7347cb2c4f993b61573f8efd7f2e7dd89d`. This completes this scene-delivery scope; full
graphical Editor milestones remain 0/8 and independent clean-host acceptance is not claimed.

✅ The tested source and squash merge have identical Git trees, verified with
`git rev-parse f327bd86^{tree}` and `git rev-parse e0036e73^{tree}`. Final publication changes
Markdown only; the executable/tool/runtime sources remain the accepted production tree.
The full comparison receipt is retained as `build/scene-settings/merged-source-tree-equivalence.json`.

## Final Windows Editor artifact

| Field | Verified value |
| --- | --- |
| File | `NexoraEditor-Windows-x64-f327bd86260a.zip` |
| Source | `f327bd86260a2c59271c7120ca8bfd9457a461eb` |
| Configuration | Windows x64 Development, DX12 |
| Size | 102,785,740 bytes |
| SHA-256 | `a5e97b52f0fd80801ff0310dcd5ba6cca6aa3e499a07b1208902fcbee5bc3cc7` |
| Inventory | All 18,501 inventory hashes checked against actual archive bytes |
| Root executable | `NexoraEditor.exe` only |

The local artifact and sidecar are retained under `build/scene-settings/editor-delivery-f327bd86260a/`.
Recipients can compare the following command's result with the SHA-256 above or the .zip.sha256
sidecar before extraction:

```powershell
Get-FileHash .\NexoraEditor-Windows-x64-f327bd86260a.zip -Algorithm SHA256
```

Private Runtime hosts/DLLs and CMake/Ninja/Zig resources/licenses are necessary build dependencies.
Compiler standard libraries and CMake modules are tool resources; no Nexora/game source checkout,
examples, TwoDPlayer, TwoDStudio or TwoDAssetCompiler is shipped. This is a source-free Editor/game
delivery contract, not a claim that compiler resources contain no source-format files.

Extract the whole ZIP, keep Runtime and Tools, then double-click NexoraEditor.exe. No startup
arguments are required. Create/Open selects the project directory implicitly as its root. The
Project tree lists project files; left-double-click opens a scene. The main Editor edits one scene
and protects dirty New/Open replacement with Save/Discard/Cancel.

Save the authored scenes, open Build > Project Build Settings, add scenes, choose Set Startup,
enable/exclude entries and Save Build Settings. The startup selection is independent of the last
Editor document. Build Game uses the project-relative output (default Build/Game); enable the
script option for a dedicated Scripts/project.json manifest, or leave it unchecked for scene-only.
Keep the whole published game folder and run Game.exe directly. Play uses the same saved enabled
scenes; dirty authoring offers Save/Use Saved/Cancel and cannot apply remapped game IDs back.

## Required hosted gates

Build run [40252656280](https://github.com/jimlee1972/Nexora/actions/runs/40252656280), its matching
push-event Build and SDK producer/consumer checks all succeeded at the exact source head. The
complete check receipt is `build/scene-settings/ci721-f327-final.json`; merge verification is
`phase4-merged721.json`. These are hosted results, not Linux/macOS execution on the Windows PC.

| Gate | Registered tests | Failures | Retained skips | Time |
| --- | --- | --- | --- | --- |
| Full Linux Development | 469 | 0 | twod.portable_font_bake | 1512.13 s |
| Full Windows Development | 407 | 0 | twod.portable_font_bake | 1389.05 s |
| Full macOS Development | 386 | 0 | twod.portable_font_bake | 988.56 s |
| Linux Editor/display suite | 235 | 0 | None | 1426.20 s |
| Linux sanitizers | 248 | 0 | Nine, listed below | 592.41 s |
| Linux TSan, native backends disabled | 236 | 0 | Two, listed below | 752.96 s |

Actual required commands include:

```bash
cmake --preset linux-development -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON -DNEXORA_ENABLE_SLANG=ON -DNEXORA_ENABLE_D3D12_WORK_GRAPHS=OFF -DNEXORA_ENABLE_VULKAN_BACKEND=ON -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON -DNEXORA_BUILD_PROJECT_PLAYER=ON -DNEXORA_ENABLE_PROJECT_PLAYER_NATIVE=ON
cmake --build --preset linux-development
ctest --preset linux-development -V --output-log build/linux-development/development-tests.log
cmake --preset linux-shipping -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON
cmake --build --preset linux-shipping
cmake --preset linux-sanitizers -DNEXORA_BUILD_VULKAN_SANITIZER_CONTROL=ON
cmake --build --preset linux-sanitizers
ctest --preset linux-sanitizers
cmake --preset linux-thread-sanitizer -DNEXORA_ENABLE_NATIVE_BACKENDS=OFF
cmake --build --preset linux-thread-sanitizer
ctest --preset linux-thread-sanitizer
```

Shipping, feature-off, module/plugin/shader contracts, native Vulkan validation and platform smoke
jobs passed. Sanitizer skips are window.x11_held_key_native, twod.portable_font_bake and
renderer.vulkan_texture_recording, vulkan_graphics_resource_reuse, vulkan_descriptor_isolation,
vulkan_recording_gpu_timing, vulkan_recording_resource_reuse, vulkan_recording_benchmark and
vulkan_recording_benchmark_reuse. TSan skips are the held-key native and font cases. Separate
native display/synchronization and sanitizer positive-control gates passed; these skips are not
relabelled as executed physical-device tests. Shipping disables CTest; its configure/build gate passed.

## Actual scene/build observations

✅ Linux Editor: Level01 startup while Title remains the last authoring document; actual red
pixels move left/right/left, 36 samples each, centroids 906.67/1002.67/906.67 inside viewport
648,265,604,180. Three scene commits, normal close/exit 0 and saved-source hashes unchanged.
The next Editor uses actual Build menu, Settings, script checkbox and Build button input. The
background producer publishes two scenes/four union assets, excludes the disabled third scene,
verifies the compiled module ABI and closes normally. No file injection substitutes for that Build.

Current display artifact
[12622741720](https://github.com/jimlee1972/Nexora/actions/runs/40252656280/artifacts/12622741720)
has SHA-256 `3539ef0a7add8ca63a1fae3d5b12b546557f7dab1a0f8fc8ddf52014d50aa409` and is retained as
`build/scene-settings/ci721-f327-display-artifact.zip`. It includes receipt.json, Play/build logs,
input coordinates, source hashes, Editor/core/native-validation logs and native Player evidence.

✅ Linux standalone: runtime.linux_native_game_player passes in 7.60 s; actual pixels move
left/right/left with 78/91/78 samples. Bad-camera rejection retains the active scene then retries;
stress presents 300 frames/121 commits, one scene/instance, three entities, one geometry upload/
299 hits, 4096 native geometry bytes and 150 CPU key bytes. Empty startup presents four frames.
All successful reports drain and exit 0. This is software Vulkan/Xvfb, not physical GPU evidence.

✅ Windows developer PC: the final package verifier uses system-only PATH, foreign Unicode CWD,
project create/reopen/read-only/existing-project protection, graphical open/creation, a shipped-tool
SDK script build, two scenes/four assets, source hiding and a 100-frame/three-commit hardware DX12
game run with unchanged hashes, drain and exit 0. Commands prove the shipped CMake/Ninja/Zig were used.

✅ Actual ZIP extraction at zip-extract-陰陽-20261026-092026 passes zero-argument graphical startup
and continued lifetime (controlled cleanup, not a graceful-close claim); a separate eight-frame
browser exits normally. Configured Play presents 1200 frames and commits three scenes, with saved
sources unchanged and exit 0. Receipt: extracted-editor-acceptance-20261026-092105. Temporarily
missing installed profile/CMake reject with exit 1, no publication and no developer fallback;
files are restored exactly. Missing CMake uses a generic launch-failure diagnostic. Receipt:
installed-missing-dependency-20261026-092147. No captured Windows pixels are claimed.

✅ A fresh final-source common producer retains final-launch-source-20261026-092920. After copying
its genuine published BuiltGame and safely hiding the authored source, Game.exe starts with
**zero arguments** from a foreign CWD containing invalid decoy package/launch files. It starts
Level01, presents 100 hardware DX12 frames, records 99 updates/three commits, one geometry upload/
99 hits, drains and exits 0. All 25 artifact hashes remain unchanged. The producer authored the
bounded test launch descriptor; it was not patched afterward. Receipts: final-launch-producer-latest.json
and final-zeroarg-latest.json; relocated output: final-game-20261026-093310-零參數 µ. Scoped process
inspection finds no residual processes under that game or final extracted Editor path.

The Windows 14-case producer/ABI/CLI gate passes in 209.25 s, zero failures/skips, including actual
default script CLI without tool overrides. The 10-case Play/native/UI gate passes in 20.51 s;
valid-script cancellation passes in 172.69 s. Packaging tests: five pass. Split-target and late
compiler discovery configure proofs cover three configurations with/without Player. All 24 changed
C++/SDK/CMake inputs match the isolated build byte-for-byte; the broader 1141-file build/code audit
has zero content differences after 21 line-ending-only normalizations. Two pure Python auxiliary
copies were synchronized; package verification already executes the current workspace versions.

## Preserved failures and limits

Original POSIX profile-layout, cancellation-precondition and first-configure empty-Zig failures
remain in the [host evidence](EditorPackagedPlayHost-Windows-2026-10-26.md). Failed artifacts
12620680567 and 12621397365 are retained with hashes; f65/1f ZIPs are historical candidates, not
delivery. The fix reads the actual Player target directory, prohibits developer fallback for
marked installations and defers tool generation until canonical Zig discovery completes.

Settings/core, deterministic cooked catalog/union, corruption/alias guards, Additive/unload/remapping,
persistent/stale handles, module Stop/teardown, historical ABI prefixes and StaticView compatibility
retain their accepted PR #716–#720 evidence. Native Unsupported/InvalidDescriptor rejection markers
are CPU-injected contract tests; they do not prove physical GPU allocation/device-loss injection.
Windows native good/bad-camera/stress/empty receipts remain at native-runs/desktop-20261026-070923-*.

The scope is native mesh/scalar-PBR packaged scenes. Unsupported required custom 2D, physics or
plugin behavior rejects explicitly. Windows developer-PC execution, hosted software Vulkan and
independent clean-host/device acceptance are different scopes. No clean-host, captured Windows
pixels, all-format gameplay support or full graphical Editor milestone is claimed.
