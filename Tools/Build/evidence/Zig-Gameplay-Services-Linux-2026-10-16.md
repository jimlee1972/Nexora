# Zig gameplay services Linux acceptance

Scope: optional public Zig/C11 services and their engine host adapters. No combat mechanics,
progression design or formal game artwork is introduced. Existing V3, 2D and pointer prefixes
remain compatible. The module owns action IDs, content choices and save schemas; the host owns
native devices, service context lifetime, storage directories and rendering resources.

## Acceptance paths

- `gameplay.zig_services_consumer`: real Zig/C++ every-field layouts, absent/older-prefix
  negotiation, supplied Atlas/Clip decode and resolution, action/pointer capture, copied font
  registration and Chinese-codepoint UI commands, voice control and permanent save/reopen.
- `runtime.gameplay_action_input`, `api.gameplay_input_zig_abi`: binding/device/focus/user
  behavior, mouse position/delta/wheel/buttons, bounded future layouts and errors.
- `runtime.gameplay_audio`: actual sample mixing, WAV decode, resampling, loop/pause/bus/gain,
  copied embedded data, stale handles, resident generations and unavailable output behavior.
- `runtime.gameplay_persistent_storage`: restart, checksum/scope/schema, atomic replacement,
  probing, error preservation, symlink/path rejection and owner-thread contracts.
- `twod.gameplay_ui`, `gameplay.zig_ui_wire_layout`: supplied atlases/glyphs, UTF-8, button
  activation/navigation/focus, owning frames and native overlay packet admission.
- `twod.gameplay_content`, `gameplay.zig_content_wire_layout`: existing cooked codec roundtrip,
  dependency/region validation, atomic generation replacement, count/storage budgets and ownership.
- `runtime.native_static_view`, `runtime.linux_native_static_player`: real package World poses,
  explicit V3 library loading, fixed/update callbacks, input/service/storage calls, presentation,
  unchanged package bytes and unload-before-native-teardown. The dynamic fixture uses the same
  C ABI as Zig; the separate services consumer is compiled with actual Zig 0.14.0.

## Environment and commands

Linux x86_64, GNU C++ 14.2, Zig 0.14.0, Slang 2026.18. Development uses modular linkage;
Shipping uses monolithic linkage. Native checks use Xvfb and Mesa lavapipe; they establish
correctness only. Build tools, X11 headers and native test utilities were provisioned in `/tmp`
without committing generated output or machine settings. Native test execution requires local
socket permission. Vulkan ICD selection points to the provisioned lavapipe ICD.

Configuration enables `NEXORA_ENABLE_ZIG_GAMEPLAY`, `NEXORA_ENABLE_SLANG`,
`NEXORA_BUILD_PROJECT_PLAYER` and `NEXORA_ENABLE_PROJECT_PLAYER_NATIVE`; Showcase is disabled.
The host tool/compiler/include/library paths are explicit CMake cache overrides for this environment.

```sh
cmake --preset linux-development
cmake --build --preset linux-development -j 4
ctest --preset linux-development --output-on-failure -j 4
cmake --preset linux-shipping
cmake --build --preset linux-shipping -j 2
build/linux-shipping/Gameplay/Zig/NexoraZigTwoD --frames 60
```

Final results: Development configure and full build passed; the full CTest preset passed
**195/195 tests** with no failures or skips (66.67 seconds). Shipping configure and full build
passed; the static Zig 2D CPU smoke returned PASS, reloaded successfully, moved to x=1 after
60 frames and reported balanced allocations. Changed Markdown links and bilingual roadmap
pairs also passed the repository documentation validator. The initial full test attempt found
a missing `clang++` in the environment; after provisioning Clang 19.1.7, the complete preset
was rerun successfully. Native checks ran with local socket permission, not a skipped Xvfb run.

## Limits

No physical audio device/controller, readable production Chinese font/artwork, Windows/macOS/mobile
execution or performance acceptance is claimed. Linux ALSA is the built-in audio output backend;
other hosts can attach a sink. Linux joystick polling supplies raw rebindable controls; other
platform adapters remain host work. Glyph fixtures prove UTF-8/metric/wire behavior, not typography.
A game supplies actual cooked assets and font/WAV data. Save/load uses explicit schema-owned bytes,
not raw entity/pool handles, and a dedicated single-writer directory per scope.

ProjectPlayer launches explicit trusted V3 code against its existing immutable StaticView palette;
it does not advertise 2D/UI/content support or become a complete 2D project exporter. The 2D Zig
module host supplies those tables and registered Sprite/Clip resolution. Editor Play attachment
and complete clean Windows product acceptance remain separate; no roadmap percentage is inflated.
