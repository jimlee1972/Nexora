# SP-03 asset residency — cloud verification

Date: 2026-10-23 UTC. Base main: `0c6009e1df3ee6325c7f07164d7fed7a2bc978d4`.
Game target: Windows, offline single player, gameplay and transition policies in Zig.

## Implementation and bounded resources

The optional asset service preserves historical C/Zig prefixes and Content V1. It adds bounded
catalog requests/background reads, owner-thread cooked hash/identity/dependency validation,
explicit removal and expired-generation GPU retirement. Deferred packages can exceed 64 MiB;
per-file, active/live generation and staging limits remain explicit. Pool capacity is configurable.
See [the full contract](../../../Engine/TwoD/GameplayAssets.md).

The diagnostic consumer packages five 2048² Atlas/Clip pairs (>80 MiB installed), loads one pair
at a time, and exercises 15 transitions, 1800 pool instances and 30 real Animator events. It checks
stale pool handles, clip-before-atlas removal, zero active/retired/staging bytes, and corruption
rejection. A separate registry stress admits all five Atlases simultaneously with an explicit
96 MiB active/128 MiB live budget, then repeats eight complete unload/reload rounds. Retained
snapshots block retirement; exceeding the live budget preserves previous generations/outputs.
Actual process RSS/working set is sampled after each round; final-four growth over warm-up is
bounded by a 32 MiB regression tolerance when host readings are available. This is not a final
physical-PC process/VRAM budget or a long-session soak.

## Executed gates

- Full `linux-development` configure/build/CTest: **279 registered, 263 passed, 16 existing native
  prerequisite skips, zero failures**. Every new portable asset/wire/independent-Zig case executed.
  [Raw CTest](SP03-AssetResidency-Cloud-2026-10-23/development-tests.log) and
  [JUnit](SP03-AssetResidency-Cloud-2026-10-23/development-junit.xml). CTest text-log trailing
  whitespace is normalized for the repository whitespace gate; JUnit is unchanged.
- Full Monolithic `linux-shipping` configure/build passed. A separately compiled ReleaseSafe Zig
  module exported and ran the same >80 MiB, 15-transition, 1800-instance workload with the Shipping
  Player, with corruption/settings rejection and a non-Development inventory.
  [Consumer output](SP03-AssetResidency-Cloud-2026-10-23/shipping-consumer.log).
- Post-unload RSS stabilized at 222,638,080 bytes (~212.3 MiB) after the first round; all final
  resource counters returned to zero. [Observed values](SP03-AssetResidency-Cloud-2026-10-23/resident-observations.json)
  include the fixture's >80 MiB source corpus and allocator/thread caches, not just game content.
- Touched-file formatting, 114-export API metadata validation and Markdown links/fences passed.
  Repository-root README and existing milestone counts are unchanged.
The [commands](SP03-AssetResidency-Cloud-2026-10-23/commands.txt) describe the exact local setup.
Native 2048² generation fixture compiled with Development flags and X11 headers. The local cloud
executor lacks Slang/native GPU prerequisites; actual Vulkan pixel execution is a hosted CI gate.
Existing target/device skips must not be called passed. Windows/macOS CI, if linked from the PR,
ran on their hosted runners, not inside this Linux executor.

## Acceptance still open

The fixtures are diagnostic art and event workloads. Final chapters/boss/district/hub transitions,
all actual pickup/combat semantics, target-PC RAM/VRAM peaks and frame time, and physical Windows
acceptance remain unaccepted. Opaque bytes do not provide compressed/streaming audio or change
color conversion. SP-01/SP-02 target/product acceptance and SP-04–SP-07 remain unchanged.
