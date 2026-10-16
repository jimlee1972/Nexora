# Cooked 2D gameplay services and Windows input — 2026-10-16

Initial full-gate baseline: `8a077f50`; integrated base after PR #611: `5bdb7a84`. Tasks: `nexora-j4q.7.2`, `nexora-j4q.7.3`.

## Delivered behavior and observed acceptance

✅ Windows `NativeGameplayGamepad` loads System32 XInput, copies four physical slots through the
existing action provider, and clears disconnected slots. A platform-neutral decoder test verifies
button/axis/trigger wire mapping, reserved zeros, slot isolation, focus/unchanged-packet recovery,
held/edge semantics, bounds and wrong-thread rejection. No physical controller is simulated.
Interactive Zig 2D movement/pause now uses rebindable action snapshots, with WASD/Space or default
slot-0 stick/A controls. The C/Zig ABI is unchanged; Windows C++ consumers rebuild for ownership.

✅ The static Zig 2D host's explicit `--validation-package` mode reads cooked Atlas/Clip and WAV
through Core asynchronous VFS. Cooked hash/type/identity/dependency checks precede publication.
It supplies a readable ASCII fixture font and Save/Open UI, resumes the fixture position across
processes, verifies byte-identical immutable cooked reload, migrates the existing Zig module,
and shuts down callbacks, audio, storage and surface in order. Existing procedural sample mode
and arbitrary module paths remain separate; validation requires the known static sample.

✅ Development and isolated Monolithic Shipping each pass two processes: X 0→1 on first run and
1→2 after resume. Each run has 60 UI frames, 48,000 stereo frames submitted through real ALSA null
output, cooked/module reload, fresh-owner save reopen and balanced module allocation. Corrupt Cook,
wrong cooked type, missing WAV (actual async failure), and corrupt persistent save all reject.
The rejected save is retained without replacement; package hashes remain unchanged.

✅ Linux Xvfb/software Vulkan runs 900 integrated frames in both configurations. Retained evidence
checks supplied `2` glyph pixels against the panel, real X11 D-key movement, distinct native mouse
Save/Open press/release cycles, 800×600 resize, UI/Sprite draws, 720,000 submitted audio frames,
immutable cooked reload and balanced teardown. Shipping reports 16,200 native UI draws, 30,600
GPU Sprite instances and one resize generation. [Captured fixture](native-ui.png) is taken at
640×480 before the tested resize. ASCII fixture pixels are not production typography acceptance.

[Development CPU results](development.json), [Development native results](native.json) and
[isolated Shipping results](shipping.json) retain the actual counters and explicit platform scope.

## Validation commands

The task-local environment supplies CMake 4.4.4, Ninja, GCC 14.2, Zig 0.14.0, Slang 2026.18,
Clang 19, extracted X11/OpenSSL SDKs, Xvfb/xdotool and Mesa lavapipe. No system/home files or
machine overrides are committed. Writable Zig/Mesa caches are outside the repository. Native tests
require the executor's supported network permission to create local Unix sockets.

```bash
cmake --preset linux-development -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON -DNEXORA_ZIG_EXECUTABLE=/workspace/work/2d-roadmap/zig-tools/ziglang/zig -DNEXORA_BUILD_SHOWCASE=OFF -DNEXORA_ENABLE_SLANG=ON -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON -DNEXORA_BUILD_PROJECT_PLAYER=ON -DNEXORA_ENABLE_PROJECT_PLAYER_NATIVE=ON
cmake --preset linux-development
cmake --build --preset linux-development --parallel 4
ctest --preset linux-development --parallel 1
cmake --preset linux-shipping -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON -DNEXORA_ZIG_EXECUTABLE=/workspace/work/2d-roadmap/zig-tools/ziglang/zig -DNEXORA_BUILD_SHOWCASE=OFF -DNEXORA_ENABLE_SLANG=ON
cmake --build --preset linux-shipping --parallel 2
python3 Tests/TwoD/ProductAcceptance.py --app build/linux-shipping/Gameplay/Zig/NexoraZigTwoD --compiler build/linux-development/Tools/TwoD/NexoraTwoDAssetCompiler --evidence-dir /workspace/work/2d-roadmap/product-shipping --shipping --native
```

Final full Development gate: **311/311 passed, zero failures/skips, 1002.89 seconds**. Optional
Showcase is off; its independent product milestones are not tested by this record. Full Shipping
build, dedicated product CPU/native cases and isolated Shipping CPU/native acceptance pass.
The [original Zig/native regression](zig-native.json) also passes 600 frames, positive keyboard
movement and balanced reload.

The earlier 309-test expanded parallel gate had six unrelated Editor display failures (prefab
isolation, plugin manager, Xvfb lifetime, scene files, build console, scene preview). All six passed
in a serial recheck. The final full run is serial to avoid software-renderer/UI event contention.
During test development, the glyph oracle was corrected for documented linear-sampler edge
blending and resize generations for the contract's exclusion of initial surface creation; the
actual captured text and resize were correct. No unrelated Editor implementation was modified.

MinGW GCC 14-posix cross-compiles both `GameplayGamepad.cpp` and the final `TwoDHost.cpp` with
`-std=c++20 -Wall -Wextra -Wpedantic -Werror -DNEXORA_RUNTIME_STATIC=1` and the corresponding engine
include directories. This is cross-compilation only; Windows execution/MSVC was not run locally.
The native Windows audio adapter is delivered by merged PR #611 (`5bdb7a84`). After rebasing,
full Development/Shipping builds pass again, and all 11 targeted audio/input/UI/content/Zig/native/
product integration tests pass (9.46 seconds). The isolated Monolithic Shipping CPU/native flow
also passes again; `shipping.json` records this combined source. The earlier full 311-case run is
reported separately above, not relabeled as the now-registered 312-test configuration.

## Remaining host/integration gates

These are correctness fixtures: no game rules or formal art, generic project exporter, main Editor
project Play, realtime audio thread/DSP or production font shaping is added. Only finite bounded
X/Y fixture position is persisted, with schema-1 little-endian float64 bytes and the existing
scoped/checksummed storage service; no runtime handles or pointers are saved. Staging checks Linux
`ldd` or Windows `dumpbin` imports, excludes Editor and requires no Engine DLLs for normal Shipping.
Development test copies are explicitly labeled. OS C/C++ runtimes and drivers remain prerequisites.

Physical Windows XInput polling/model compatibility, audible waveOut playback/reopen, Windows
DPI/display changes, CJK/font appearance and clean-host integrated product acceptance remain
**NOT RUN**. Software Vulkan and ALSA null do not accept those gates or physical minimum-PC M6.
M7 is **PARTIAL / NOT ACCEPTED**; 9/11 (82%) remains unchanged.


The existing Windows Shipping CI job additionally builds the static 2D host/compiler, stages a
clean `NexoraTwoD-Shipping/` directory, and runs the CPU/cooked-service package checks without
physical-device claims. `twod-windows-shipping` uploads that package and separate evidence.
Each package includes a hashed `README.txt` with launch command/controls and explicit target-host
prerequisites. Local validation of this CI/staging addition: workflow YAML parsing, all 16
DocumentationCI routing tests, both product CTests and isolated Shipping CPU/native flow pass.
No C++ runtime behavior or required-check protection changes in this addition.


## Bounded Windows audio queue follow-up

Source review during integration found the initial Windows sink waited for the block it had just
submitted. Combined with an interactive host deriving samples from prior elapsed frame time,
that adds audio wait to the next frame's audio duration. The current sink instead copies/submits
through four fixed owned slots and waits only when reusing a pending slot. No hardware FPS gain
is claimed. Queue ownership, PCM16 conversion, four submissions without waits, pending-data
preservation, timeout, failed submit/retry and reset run through the same portable queue used by
Windows in `runtime.gameplay_audio_queue`. Native headers/events remain Windows-owned and reset
before buffers expire. Stop/pause/gain affect future blocks; output close drops queued tails.
The C ABI and external sink signature are unchanged. The final post-queue full Development gate passes **313/313, zero failures/skips, 1025.73 seconds**.
Full Shipping rebuild and isolated CPU/native product acceptance pass again. MinGW strictly
cross-compiles the queued backend and links the Windows audio probe. The 17 CI cleanup guard
cases and 16 documentation-routing cases pass. Physical Windows driver/latency/FPS/audibility
remain unverified; none of these fake-device or software/null runs accepts them.


The existing CI cleanup helper has a separate exact allowlist for this goal's
`feat/twod-cooked-services` branch. It can cancel only superseded same-repository owner-actor runs;
current-head push/PR runs, forks, contributors, other branches and ref-advancement races are kept.
Tests exercise the actual cleanup helper with those guards. No required check is disabled, and
no broader feature-branch policy or other-session allowlist is added by this goal.
