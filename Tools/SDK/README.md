# Windows script-only 2D SDK

The fixed `sdk-2d-windows-x64-v0.1.0` release provides a Monolithic Shipping DX12 generic
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

## Release and evidence

`windows-2d-sdk.yml` builds candidates on MSVC, then tests the exact ZIP in an independent Windows
job. That job physically denies access to renamed VS/Windows SDK installations, uses fresh Zig
caches and has no engine source checkout. The producer and consumer are separate jobs. A tagged
release promotes the tested ZIP bytes; `release` publication never rebuilds or overwrites an SDK.
The full Build workflow remains required before merging the implementation.

Consumer evidence includes an asset-bearing script-only fixture, portable PE checks and two
independent finite headless host runs. Native graphics, controller, audio, DPI and physical game
acceptance remain `NOT_RUN` unless separately observed. They do not block this development SDK.
See the workflow artifacts and the game integration evidence for real NX-00 deterministic runs.
