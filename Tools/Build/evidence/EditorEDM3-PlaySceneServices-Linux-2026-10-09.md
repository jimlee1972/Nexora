# ED-M3 Play scene/entity services: Linux evidence

Date: 2026-10-09. Beads tasks: `nexora-rd7.2.1` and `nexora-rd7.2.1.1`.
Initial source base: accepted Game materials PR #454 (`f47c6f15`), including native GPU timing and
capture. This supporting implementation does not accept complete gameplay services or ED-M3.
Final integration includes accepted Console PR #455 (`41dcd394`) and profiler contract PR #456;
both Core producer evidence and actual dynamic scene-service evidence remain required in the
combined native fixture, with their independent test targets retained.

The Editor's real V3 host now advertises implemented Scene API and host allocator capabilities.
Static/dynamic loading requires a Play-kind World. Load creates empty in-memory scenes, not project
file imports: owning UTF-8 names are NUL-free, 1–256 bytes; persistence is 0/1. Activate uses existing
loaded-inactive lifecycle. Spawn copies the known descriptor prefix, validates flags/reserved data,
finite position and enabled Camera/Light/Mesh values before creating an entity. Future suffixes and
unused component data are ignored. Physics returns Unsupported; other optional service slots remain
null. Scene/material formats and stable C/Zig wires are unchanged.

The owner admits at most 32 scenes and 4096 spawns over one module binding. Only successful calls
consume quota; despawn does not replenish it. Deletion uses atomic World commands, including
descendants. Failed calls retain outputs and publish no partial object. Callbacks run on the
serialized game thread and retain no wire or movable World-storage pointer. Stop/Destroy retain
services, then Unload clears the binding before clone teardown. Stop discards created/deleted
entities, while transform Apply Changes continues excluding them. Editor source data and Undo are
separate from these Play-only mutations.

World creation now reserves UINT64_MAX as the exhausted watermark and supports IDs through
UINT64_MAX-1. It rejects before mutation rather than wrapping, advances allocation only after
successful insertion, and rejects reserved-max imported entity IDs. Save/replacement and owning
Play clones preserve near-limit identities/watermarks. Full uint64 mesh/shader IDs remain valid.

Host: Linux x86_64, GCC 14.2, CMake 3.31.6, Slang 2026.18, clang-format 19, Mesa lavapipe/Xvfb.
Development is Modular with graphical shell, Slang, Zig and Showcase enabled. Full native gates
are serialized on the four-CPU cloud host. Minimal Shipping is Monolithic, Editor OFF, native
Presentation ON. Windows/macOS are not executed locally; hosted results belong to the PR/Beads.

```bash
source /workspace/.nexora/env.sh
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development
ctest --preset linux-development -R '(world_identity_exhaustion|play_scene_services|play_gameplay_module|game_world_facade|gameplay_host_bridge|bounded_scene_save|cooked_scene|runtime_capture)' --output-on-failure
cmake --preset linux-shipping
cmake --build --preset linux-shipping
git diff --check
```

Initial focused portable gate passed **8/8**, no skips, in **6.16 seconds**. Refined focused gate,
including actual dynamic native scene services, passed **9/9**, no skips, in **10.90 seconds**.
After integrating accepted Console #455 and profiler contract #456, final serial Development
configure/build/test passed **197/197**, no skips, in **358.03 seconds**. Minimal Shipping
configure/build passed. The acknowledged SceneFiles workflow passed **37.68 seconds** inside that
complete gate. Touched C++ clang-format, combined native Python syntax and `git diff --check` passed.

The first serial full gate passed 194/195 in 356.81 seconds, with the existing SceneFiles Open/edit
fixture failing its adoption assertion. All new services/identity/native cases passed. Unchanged
isolated SceneFiles passed **1/1** in **37.91 seconds**. The fixture now waits for the production
startup association written after successful deferred Open before later edit/save/Undo assertions;
no deadline inflation, request retry or test-only application hook is used. Its focused gate passed
**1/1** in **37.78 seconds**. Beads `nexora-nqn.8` tracks this acceptance correction. The observed
failure and lack of acknowledgement are recorded without inferring a proven root cause.

Portable actual-module tests cover copied component values, all flags and invalid descriptor
fields, Unicode names, oversize-before-read, null pointers/outputs, inactive/unloading/unloaded
scenes, future suffixes/unused fields, full uint64 resource IDs, cascade/stale deletion, exact/over
lifetime quotas, repeated Play/Stop/Destroy and Editor isolation. World tests cover last scene/entity
allocation, reserved-max import, repeated rejection without publication, replacement and owning clone
exhaustion. The independent dynamic library fixture invokes all four scene callbacks successfully
inside the actual Linux Editor before native Game movement, Pause/Step/Stop and unchanged-scene checks.

Physics, asset resolution, debug drawing/diagnostics services, expanded devices/users, module hot
reload and full ED-M3 acceptance remain open.
