# Windows script-only 2D SDK

SDK 0.2.0 is published in the immutable [`sdk-2d-windows-x64-v0.2.0` release](https://github.com/jimlee1972/Nexora/releases/tag/sdk-2d-windows-x64-v0.2.0) and provides a Monolithic Shipping DX12 generic
host, asset compiler, Zig bindings/build entry, export tools, Ninja 1.13.2 and redistributable CRT.
Game developers install Zig 0.14.0, CMake 3.25+ and Python 3.10+; MSVC, Windows SDK and engine
source are not consumer requirements. The SDK is for development, not the complete game.

Verify the archive SHA-256 against a reviewed game lock before extraction. Then run
`python Tools/SDK/SDK.py verify <sdk-root>`; `sdk.json` records version, source, target, ABI,
configuration and all payload hashes. The manifest excludes only itself. Keep SDK code read-only;
put Zig caches and build products in the consumer build directory. Never silently build engine
source after SDK validation/download failure. Published versions are immutable.

Before `project(... LANGUAGES NONE)`, select Ninja and set `CMAKE_MAKE_PROGRAM` to
`<sdk>/Tools/Ninja/ninja.exe`. Include `CMake/Modules/NexoraGameProject.cmake` and use
`nexora_add_game_module` as documented in `Tools/Zig/README.md`. Export with Python:

```text
Tools/TwoD/ExportGame.py Game/game.json --runtime <sdk>/bin/NexoraGameHost.exe
  --executable-name MyGame.exe --compiler <sdk>/bin/NexoraTwoDAssetCompiler.exe
  --vc-redist <sdk>/bin --output <fresh-package>
```

The copied host resolves `game.nx2game` beside its actual executable, even from another working
directory. No game C++ compilation, child Player or launcher is required. The optional executable
name leaves existing exporter callers unchanged. PE normal and delay imports are checked in Python;
Editor/non-system/missing CRT dependencies reject before publication. No new Gameplay ABI is added.
Keep the entire exported game directory together; players do not need SDK tools or Python.
Asset preparation keeps compiler arguments ASCII-relative and atomically publishes through Python,
so Unicode project/output paths do not pass through the legacy compiler's narrow Windows argv.

## Release and evidence

`windows-2d-sdk.yml` builds candidates on MSVC, then tests the exact ZIP in an independent Windows
job. That job physically denies access to renamed VS/Windows SDK installations, uses fresh Zig
caches and has no engine source checkout. The producer and consumer are separate jobs. A tagged
release promotes the tested ZIP bytes; `release` publication never rebuilds or overwrites an SDK.
The full Build workflow remains required before merging the implementation.

The original0.1.0 consumer evidence covers an asset-bearing script-only fixture, portable PE
checks and two independent finite headless host runs. Its historical native/physical scope is
unchanged. The0.2.0 native DX12/offline requirements below gate promotion; physical controller,
audio, DPI, formal assets and target-PC acceptance remain separate. See workflow artifacts and
game integration evidence for actual diagnostic/NX-00 observations.


## 0.2.0 HD2D delivery

0.1.0 remains available and immutable. 0.2.0 adds flat point lights, bounded planar soft shadows,
explicit linear emission masks/selective bloom, independently optional diagnostics/debug-map
suffixes and the reusable Zig DEV ONLY scene. Original services/Sprite prefixes remain compatible.
The archive includes paired-mask preparation and diagnostic staging tools plus Noto OFL/NOTICE
under `licenses/`; retain those notices when embedding the diagnostic raster font in a game.
No normal maps, 3D height shadows, HDR display output or Editor light-authoring tools are included.

Release promotion also checks the consumer receipt SHA-256 against the actual candidate ZIP,
its source commit, real native diagnostic result and offline rebuild. The source-free independent Windows consumer builds the actual reusable Zig scene using only the
candidate ZIP, executes its 144-frame headless fallback and native DX12 path, validates complete
inventories, then reuses the same SDK offline. Consumer validation is diagnostic engineering
acceptance; physical display/input, formal assets and target-PC FPS remain NOT_RUN. Tagged release
promotion requires the successful main push candidate and full Build at the same source SHA;
no release rebuild or overwrite is permitted. The game lock pins the resulting archive SHA-256.

## Retrying immutable publication

The tag identifies the tested SDK source, not the publisher implementation. If publication fails before creating a release, retain the tag and failure logs. The workflow accepts a `release/windows-2d-sdk-v<version>` recovery branch at an already validated publisher commit. It requires a successful full push Build at that publisher commit, resolves the existing immutable version tag, and separately requires successful candidate and full push Build at the tagged source. It downloads those original artifacts and validates the unique consumer receipt recursively, because Windows workspace and runner-temp files can give upload-artifact a common drive root. Missing or multiple receipts fail. Source, ZIP SHA, embedded manifest, native DX12 and offline-rebuild checks remain mandatory. This does not rebuild or overwrite any release; `gh release create` still rejects an existing version.
