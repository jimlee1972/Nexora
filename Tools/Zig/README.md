# Script-only Zig game projects

The engine owns `build.zig`, `game_entry.zig`, the V3 export and host allocation.
Authored projects contain a `project.json` and gameplay scripts, never copies of these files.
Build products and caches belong in the caller's binary directory, outside the authored project.
Editor build actions must use this same entry; an Editor UI integration is not implemented here.

```json
{"schema": 1, "module_name": "MyGame", "script": "scripts/game.zig"}
```

The script exports `pub const Game = struct { ... };`, default-initializable with `.{}`.
It imports `nexora`, uses `nx.game.Context`, and may implement these public methods:

```zig
pub fn init(self: *Game, context: *const nx.game.Context) !void
pub fn start(self: *Game, context: *const nx.game.Context) !void
pub fn update(self: *Game, context: *const nx.game.Context, delta_seconds: f64) !void
pub fn fixedUpdate(self: *Game, context: *const nx.game.Context, delta_seconds: f64) !void
pub fn stop(self: *Game, context: *const nx.game.Context) void
pub fn deinit(self: *Game, context: *const nx.game.Context) void
```

Omitted callbacks are no-ops. Only a script implementing `fixedUpdate` advertises the fixed-update
capability. The adapter does not impose simulation policy or a tick rate. State migration callbacks
are not exposed by this initial facade; `save_state`/`load_state` stay null. Existing raw V3 modules
remain compatible and need not migrate.

## Ownership and errors

One aligned instance is allocated through the host's `gameplay_state` allocation owner. The SDK
retains a copy of the negotiated V3 host prefix, zero-fills absent suffixes and never reads older
hosts beyond their advertised size. Missing services report `error.Unsupported` through Context.
The Context and its service tables are borrowed for that instance's lifetime; do not retain them
after `deinit` or use them from a different thread. Lifecycle callbacks remain on the host owner
thread with the same order as raw V3.

`init` must unwind its own partially acquired resources on failure (use `errdefer`). The SDK frees
its instance and leaves the output null; it does not call `deinit` on failed initialization.
After successful initialization, `deinit` releases game-owned resources and the SDK frees its own
instance with the same host, owner tag, size and alignment. Failed `start` is followed by host
`destroy`/script `deinit`; scripts must support this path. `stop` precedes normal destruction.
No background/deferred work is introduced by the adapter.

`error.InvalidArgument` maps to -1, `error.Unsupported` to -2, all other script errors to -3.
Context logging is optional and best-effort; an oversized diagnostic is discarded. Context scene
names are UTF-8 byte slices with checked u32 lengths. Author all Zig files as UTF-8 without BOM.

## Shared command-line / Editor build entry

```cmake
include("${NEXORA_SOURCE_DIR}/CMake/Modules/NexoraGameProject.cmake")
nexora_add_game_module(game_module
  PROJECT "${CMAKE_SOURCE_DIR}/Game/project.json"
  OUTPUT "${CMAKE_BINARY_DIR}/module"
  ZIG "${ZIG_EXECUTABLE}")
```

The caller owns engine/compiler version pinning, runtime target and export/package policy.
The shared function validates the manifest and project-relative script, builds a ReleaseSafe
dynamic module, then runs entry-contract and authored-script tests independently. Windows uses
x86_64-windows-msvc baseline; other hosts use Zig's native target. Script path components below
the project root must not be symlinks. The game source tree is read-only to the builder.

Advanced callers can invoke the engine build file directly with absolute script/cache/prefix
paths: `zig build --build-file <SDK>/Tools/Zig/build.zig -Dscript=<script> -Dname=<module>`;
add `test` to run both test runners and `-Dtarget=...` for an explicit target. Use Zig 0.14.0.
The SDK path is an installation implementation detail, not an authored project file.

Regression coverage: `gameplay.zig_game_script_adapter` validates lifecycle and failure ownership;
`gameplay.zig_script_project` builds an external script-only project in a Unicode/space path,
executes both test runners, verifies untouched sources and rejects parent traversal.

On macOS, use the shared CMake entry: it reuses the existing Apple host-linker workaround for Zig 0.14 dynamic modules and test executables. The raw Zig build-file command does not apply that workaround.

Local validation and acceptance boundaries: [Windows evidence](../Build/evidence/Zig-Script-SDK-Windows-2026-10-24.md).
