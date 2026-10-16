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

## Native acceptance readiness correction

The initial PR CI Linux Development run 38865662727 passed 337/338 tests but failed
`twod.cooked_product_native` with no observed keyboard movement. The harness sent a single
directed XSendEvent key press as soon as the window mapped, before requiring a presented UI
frame. It now waits up to eight seconds for the same supplied-glyph pixel oracle, then refocuses
the window and sends XTEST keyboard input. All movement, Save/Open, resize, audio-submission,
reload and teardown assertions remain required. Five consecutive corrected native runs pass
(31.14 seconds total). The original 313/313 gate above predates this harness-only correction.
Configure/build pass again. The post-correction full gate executes all 313 tests in 1008.76
seconds: 312 pass, with only the pre-existing `editor.linux_scene_files` xdotool window-operation
failure; its isolated recheck passes in 38.86 seconds. The corrected 2D native case passes in
that full gate. Isolated Shipping CPU/native acceptance also passes: 900 frames, 114 movement
frames, one SAVE and one OPEN, and final 800x600 output. This is not reported as a new clean
313/313 run. The pre-correction same-head push CI run38865655927 independently passes all jobs.


## Movement and SAVE acknowledgement follow-up

PR #618's original push workflow `38918493994` failed `twod.cooked_product_native` twice
(jobs `117032296232` and `117046283419`), each with 341/342 tests passing and the same
"native keyboard did not move Zig entity" assertion. The identical head's PR Linux workflow
passed. Eight consecutive local original-harness runs also passed; the specific CI event-loss
cause was not reproduced or attributed to an X11/runtime bug.

The test previously assumed one XTEST D press plus 150 ms proved native movement before
SAVE/OPEN. It now uses a five-second bounded acknowledgement loop: refocus, pulse the physical
key, click SAVE, then require an actual CRC-valid schema-1 position slot with positive X.
It releases D before OPEN and additionally requires final X to match that acknowledged save.
All existing movement, frame, UI, resize, audio-submission, reload, corrupt-data and teardown
assertions remain. A failed acknowledgement retains the host's stderr in `native.log`.
No application, input-service, native backend, storage format or production CLI changed.

Native verification uses Zig 0.14.0, Xvfb 21.1.12, xdotool 3.20160805.1 and Mesa lavapipe 24.0.5,
with the existing `ProductAcceptance.py --app .../NexoraZigTwoD --compiler .../NexoraTwoDAssetCompiler
--evidence-dir <fresh-directory> --native` command. The fixed driver passes five consecutive
native runs in 34.54 seconds, each retaining all 900 frame, movement, Save/Open and 800x600 checks.
A task-local xdotool wrapper deliberately dropped the first `keydown d`: the original harness
failed its movement assertion, while the new driver passed with real subsequent XTEST input
and two native SAVE clicks. This is a controlled missed-first-input experiment, not proof of
what happened inside the earlier CI X server. Dropping every D press fails the new bounded
movement/save acknowledgement; no successful acceptance report is emitted.

The tools/compiler and wrapper live outside the repository. This is Linux software-device
harness evidence and does not close physical Windows, audibility, DPI or M6/M7 acceptance.

The follow-up full Linux gate passes **190/190, zero failures/skips, 135.96 seconds**:
`cmake --preset linux-development -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON`,
`cmake --build --preset linux-development --parallel 4`, and
`ctest --preset linux-development --output-on-failure`.
The cloud cache keeps Graphical Editor and Slang optional features off; canonical precompiled
Slang artifacts are supplied to native RHI tests, and the native product command above is run
separately. This local 190-test scope is not the 342-test hosted desktop configuration.
Xvfb and xdotool are extracted into a task-local prefix; the execution needs socket permission.
No repository preset, feature default or CI job has been relaxed.
