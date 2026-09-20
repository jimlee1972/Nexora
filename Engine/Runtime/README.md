# V1-M4 through V1-M12 runtime contracts

`NexoraRuntime` is the dependency-ordered, platform-neutral baseline for the remaining V1
milestones. It deliberately contains no SDK-specific physics, media, mobile, or editor backend.
Instead, it makes the ownership and safety boundaries executable before those integrations land.

| Milestone | Executable baseline |
| --- | --- |
| M4 | Additive scene lifecycle, stable entity IDs, deferred safe unload, double-precision transforms |
| M5 | Hash-validated asset generations, dependency-cycle rejection, pinning and rollback |
| M6 | Plugin ABI validation, service discovery and undo transactions |
| M7 | Device-neutral input routing, stable touch IDs, virtual-list materialization and locale fallback |
| M8 | Separate character intent and resolved motion; query-only navigation paths do not write transforms |
| M9 | Animation playback, bounded audio voices, reference-counted residency and a timestamped media queue |
| M10 | Separate cell/bundle identity, observable RAM/VRAM use, HLOD state and occupied-cell pins |
| M11 | App lifecycle, pressure policy and native WebView pointer ownership |
| M12 | Profile-driven plugin, shader, optional-asset and headless presentation stripping |

The contract test `runtime.v1_m4_m12_contracts` exercises every row, including localization
fallback, unreachable navigation, animation looping, audio voice limits, media back-pressure and
seek invalidation. Platform SDK adapters and production authoring tools remain future work and
must preserve these interfaces rather than bypassing their lifecycle checks.

## Zig gameplay bridge

`GameplayModuleHost` executes the versioned `NexoraGameModuleV2` C ABI while the original V1 layouts
remain declared for source compatibility. It validates the host and
module structure sizes, ABI version, and required callbacks before initialization. Update, reload,
and unload operations are serialized; a replacement module is initialized before the active module
is shut down, and a rejected replacement leaves the active module running.

The host table exposes size-checked component reads/writes, event subscription, and tick control.
Modules may additionally provide state save/load callbacks. Reload serializes the active state,
initializes and restores the candidate, and only then retires the active module; migration failure
keeps the active module alive. `GetReloadStats()` exposes successful reload count, migrated bytes,
and wall-clock reload duration for profiler integration.

Configure with `-DNEXORA_ENABLE_ZIG_GAMEPLAY=ON` to compile the minimal Zig GameModule and run the
`gameplay.zig_abi_smoke` test. Zig 0.14.0 is the pinned CI toolchain. This is the first executable
toolchain gate. Dynamic-library/editor orchestration, mobile cross-compilation, and device execution
remain required follow-up gates.

Desktop CI builds the same module on Linux, Windows, and macOS. A separate CI smoke matrix also
runs `zig build-obj` for `aarch64-linux-android` and `aarch64-ios`; these checks validate object
generation only and do not claim Android NDK or iOS SDK linking, packaging, or runtime execution.
