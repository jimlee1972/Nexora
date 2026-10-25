# Shared installed-game producer — Windows 2026-10-26

Candidate phase-4 scope: exact saved-source capture, common engine-owned producer/publication,
actual SDK/CMake/Zig compilation and source-free installed-game inventory. Contracts:
[build](../../../Engine/Editor/GameBuild.md), [Runtime catalog](../../../Engine/Runtime/GameProjectPackage.md),
[native candidate](NativeGamePlayer-Windows-2026-10-26.md).

MSVC x64 / Zig 0.14.0 developer-PC validation uses the isolated developer-preview source/build.
The build service freezes authored script sources and materializes embedded canonical SDK glue
in its private operation. The existing adapter compiles the actual gameplay DLL and runs its
entry/script tests; Player V3 inspection opens that DLL without gameplay initialization. Native
CPU admission checks all configured scenes and the complete staged Player/runtime closure.

The integration fixture opens a real project, persists non-Main startup/build settings, captures
two saved scenes and builds explicit scene-only and scripted variants. It exercises owner/cancel
guards, exact scene staleness, script-file additions, prepared-file tampering, foreign previous-output
protection, successful replacement and compiler failure preserving the old artifact. The actual
Editor executable's CLI separately uses the same producer/publisher from a read-only workspace.
No authored scene/source, SDK, compiler/cache or Editor dependency enters the game output.

Local Windows commands:

```powershell
cmake --preset windows-development -DNEXORA_BUILD_PROJECT_PLAYER=ON -DNEXORA_ZIG_EXECUTABLE=G:/proj/Nexora/.tools/zig-win-x86_64-0.14.0/zig.exe
cmake --build --preset windows-development --target NexoraGameBuildTests NexoraGameBuildCaptureTests NexoraGameProjectExportTests NexoraProjectBuildSettingsTests --parallel 3
ctest --preset windows-development --output-on-failure -R '^editor.(game_build|game_build_capture|game_project_export|project_build_settings)$'
```

Initial common producer passed 4/4, zero failures/skips, 106.96 s. ABI/profile/staged runtime
and CLI extensions passed subsequent gates at 100.75 s and 102.71 s. The authored shared-OBJ/
fixed-identity extension passed 4/4 in 101.73 s. Logs remain under `build/scene-settings/`
with `game-build-20261026-{043135,043550,044047,044443}-*` stamps. Later final-source tests and
source/binary receipts must supplement these historical slices before this candidate is accepted.

Earlier compilation failures (unsigned optional comparison and the AssetUuid nil test) were
corrected to the declared types; MSVC warnings remain errors. The first capture fixture's camera
return-type and per-reader-generation comparisons were corrected without weakening serialized
scene/source/identity conservation. Original failed logs remain under their stamps.

## Final-source local slice

The combined gate `phase4-gate-20261026-045312` passed 14/14, zero failures/skips, 116.79 s:
four producer/settings tests, game launch, queued session/actual Zig module, game/legacy CLIs,
legacy package/native admission, V3 module host, host bridge and C consumer. The API validator
still reports 130 documented C exports. Full configure/build/test logs remain under that stamp.

`build/scene-settings/phase4-source-inputs.json` retains 49 compared source inputs and six binary
SHA-256 hashes. Forty-eight inputs are byte-identical; committed `Bindings/Zig/scene.zig` differs
only by Git's CRLF checkout versus the isolated LF file. Normalized source is identical, and both
raw hashes are retained. This is not represented as 49 byte-identical inputs.

The same final Player reran the four real Windows hardware cases (normal A/B/A, rejected camera
and retry, 120 replacements, empty fallback), exit 0/drain with stable counters. Logs/receipts:
`build/scene-settings/native-runs/desktop-source-20261026-045312-*`. No residual Game,
NexoraProjectPlayer, Editor or build-test process remained.

## Actual producer artifact relocation

The actual service/SDK integration fixture retained its scripted artifact and copied it to
`build/scene-settings/produced-game-20261026-045715-零參數 µ`. Its 25 files comprise renamed
Game executable, module, package/launch, eight configured engine DLLs, eight configured CRT DLLs,
LICENSE and source/tool/command/inventory receipts. No Scene/Zig/C++/header, SDK/compiler/cache or
Editor dependency is present. The original fixture project was moved aside within the owned test
directory; its original path was unavailable during the run.

With no arguments, from a foreign CWD containing invalid decoy Game.launch/Game.nxgame, this
producer-created artifact passed 100 hardware DX12 presents/draws, 99 module updates and three
configured Level01 → Title → Level01 commits. Two saved scenes share an actual imported OBJ;
authored IDs overlap. The World ends with one scene/instance and two entities, one geometry upload,
99 hits, 65536 backend allocation bytes and 150 CPU comparison bytes. It exits 0/drains and all
25 artifact SHA-256 hashes remain unchanged. Receipts: `build/scene-settings/produced-game-latest.json`,
`produced-build-20261026-045715.*`, `native-runs/produced-game-20261026-050014.*`.
This is developer-PC evidence, not clean-host or captured-pixel acceptance. Historical manually
assembled relocation remains separate.

Pending: full exact-head Linux Development/Shipping/platform/sanitizer gates, graphical build owner, Editor
Play, Windows pixels and final Editor-only ZIP. No additional phase or full graphical milestone
is accepted by these CPU/build tests. Existing Runtime/API phase acceptance remains 3/5 overall.
