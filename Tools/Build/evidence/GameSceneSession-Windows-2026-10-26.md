# Queued game scene session — Windows 2026-10-26

Scope: portable phase-3 Runtime/C/Zig candidate in the
[scene delivery plan](../../../Roadmap/en/Project_Scene_Delivery_Plan.md).
This accepts no native GPU/pixel, Editor Play/build or final ZIP gate.

## Implemented candidate

The optional Asset Pipeline session owns a verified catalog and borrows an owner-thread Play
GameWorld. Requests queue during callbacks. At a subsequent frame boundary it copies/remaps a
candidate World, prepares an owning presentation and only then publishes without allocation.
Failure/exception/Stop during preparation preserves live scenes, camera, resources and bindings.
Replace retains persistent scenes; Additive retains the primary camera; unload rejects persistent,
last-owned and stale instances. Successful retirement removes audio, physics and character bindings
and obsolete scene records. Templates/render frames retain shared immutable CPU asset ownership.

The appended services getter and dedicated Scene V1 baseline preserve the frozen V3 ABI and all
historical services prefixes. Typed Zig helpers negotiate the optional suffix. The actual engine
SDK builds a ReleaseSafe authored fixture DLL whose callbacks request Level01 → Title → Level01.
Its caller verifies that each request applies after the callback, then Stop/Destroy releases the
module allocation while the borrowed service is still valid. See the
[ownership/transition contract](../../../Engine/Runtime/GameSceneSession.md).

## Developer-PC verification

MSVC x64, Zig 0.14.0 and the existing isolated developer-preview source/build were used:

```powershell
cmake --preset windows-development -DNEXORA_BUILD_PROJECT_PLAYER=ON -DNEXORA_ZIG_EXECUTABLE=G:/proj/Nexora/.tools/zig-win-x86_64-0.14.0/zig.exe
cmake --build --preset windows-development --target NexoraGameSceneSessionTests NexoraGameProjectPackageTests NexoraProjectPlayer NexoraApiM6CConsumer --parallel 3
ctest --preset windows-development --output-on-failure -R '^(runtime.(game_scene_session|game_scene_zig_module|game_project_package|game_project_player_cli)|api.(m6_c_consumer|m6_manifest_compatibility|gameplay_scene_zig_abi|gameplay_input_zig_abi|gameplay_lifecycle_zig_abi|audio_state_baseline_guard|lifecycle_baseline_guard|scene_baseline_guard))$'
```

All 12 selected tests passed, zero failures/skips, 25.13 s. Logs and stderr are retained as
`build/scene-settings/session-20261026-012418-{configure,build,test}.log` and matching stderr logs;
`session-latest.json` records exit 0. The new ABI mutation guard also passed independently (5 tests).
The manifest validator reports 130 documented exports.

`build/scene-settings/session-source-inputs.json` records all 23 matching candidate/isolated
source inputs and SHA-256 hashes for the session test executable, Runtime DLL and actual Zig DLL.
CTest's retained `Testing/Temporary/LastTest.log` includes the successful real-module diagnostic.

The session tests cover remapped parent/child IDs, non-Main startup, persistent live entities,
shared CPU resource lifetime, 120 alternating replacements with stable retained scene/entity/binding
and presentation-marker counts, additive/unload and primary-camera policy, duplicate/stale/missing
rejection, unchanged invalid/short C outputs, wrong-thread rejection, old services prefix bytes,
prepare rejection/exception/cancellation and audio/physics/character conservation/cleanup. Marker
counts are CPU stand-ins; they are explicitly not GPU resource-retirement evidence.

Earlier failed attempts remain under their original session stamps: duplicate draft Zig declarations
were removed; the Windows DLL test locator was corrected from the POSIX `lib` directory to Zig's
Windows `bin` directory; the ignored new ABI guard was explicitly copied into the isolated test tree.
No production admission or assertions were weakened to obtain a passing result.

## Remaining gates

Complete Linux Development configure/build/CTest, Linux Shipping linkage, feature-off, sanitizer,
TSan and platform CI remain required for this candidate's eventual exact source head. The dependency
PR #717 has passed 38/38 exact-head checks and merged as `7968bf62`; those dependency checks do not
validate this session implementation. Native rendering must stage all required resources before publication
and retire GPU owners after outstanding work drains. The common build entry, Editor Play, relocated
game acceptance and final executable Editor-only ZIP remain open. Overall acceptance is now 2/5, solely from the separate package phase.


## PR 718 first CI attempt

Head `de809f460b977e789811eebae8043d1b64a67a72`, PR run `40221050600`,
failed Linux/macOS compilation in GameSceneSessionTests.cpp: four C POD test values used partial
aggregate initialization, rejected by the existing `-Werror=missing-field-initializers` policy.
Linux job `121252610721`, build-contract `121252610686` and macOS `121252610782` retain the
original diagnostics. The correction value-initializes the complete POD and explicitly assigns
struct_size/abi_version. No production code, warning flags, assertions or ABI baselines change.
That first head was not merged; complete updated-head gates were required. Subsequent local
integration validation is separate from the original 12-test source receipt above.

## Exact-head acceptance and merge

✅ PR #718 head `8726bb7fa2e58cb3c1a928dd9537edabf6a69368` passed all 38 checks
across PR run `40224199007` and push run `40224195192`. The expected-head guarded squash
merge produced `b45d880ea48aa6ab239b488f5fda7153ae2b401b` on main. The original failed
attempt remains retained; no warning policy or production admission was weakened.

Full hosted Linux Development registered 461 tests (1700.75 s), Windows 401 (1115.08 s)
and macOS 380 (902.13 s), zero failures with the existing font skip. Shipping, sanitizers/TSan,
feature-off, native display, packaging and platform smoke checks passed. Check receipt and
full selected job logs are retained at `build/scene-settings/phase3-ci718.json` and
`phase3-ci718-job-*.log`. These are hosted-platform results, separate from local Windows tests.

This closes only the Runtime session/API phase: scene delivery is 3/5, graphical Editor 0/8.
Common build, native pixels, Editor Play and final Editor ZIP still require integrated acceptance.
