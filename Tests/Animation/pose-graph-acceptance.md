# V2-M8 synchronized compressed TRS graph acceptance

Scope: portable visual sampling/blending in the optional Foundation-only Animation module.
V2-M8 remains open and overall V2 progress remains 46%.

## Delivered contract

- Owned compressed clips, explicit common skeleton ID/joint count, and aggregate 1,048,576-sample budget.
- Existing marker clocks drive uniform compressed pose sampling, including cyclic marker segments.
- Normalized weighted translation/scale and leader-hemisphere weighted quaternion nlerp.
- Canonical clip order, weight changes without clock reset, zero-weight exclusion, all-zero pause.
- Transactional clip replacement and staged clock updates; invalid input preserves the live graph.
- Complete local TRS output consumed by the existing local retargeter.
- Optional/profile stripping remains Foundation-only; no new Runtime or C ABI dependency.

The skeleton ID asserts caller-authored identity; the graph cannot verify a joint naming schema.
Uniform frames include an authored endpoint at duration. Loop seams require authored continuity.
The graph performs no hierarchy/transform application, event dispatch, root-motion extraction,
inertialization, motion warping, or GPU work. Leader changes can change the quaternion hemisphere
reference. Weighted nlerp does not promise multiway slerp or smooth transitions.

## Independent coverage

`animation.v2_m8_synchronized_trs_graph` verifies known two-joint marker times and weighted TRS
values, input ownership/order, leader switches, wrap segments, pause/resume, retarget consumption,
170/-170-degree short-arc rotation and q/-q equivalence. It also exercises float maximum/denormal
pose channels and weights, maximum finite double ticks, malformed skeleton/storage/weight/tick
rejection, previous-state preservation, and exact/over-budget aggregate clip samples.
`build.animation_profiles` checks all three actual source files and Foundation-only target graphs
in six enabled/disabled/Shipping/headless configurations.

## Local validation

Linux cloud, GCC 14.2; graphical Editor enabled. Commands:

```bash
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development
```

Result: configure/build passed; 160/160 tests passed, 0 failures/skips, 330.10 seconds.
This includes native graphical Editor and software-Vulkan/virtual-display probes.

Shipping Minimal configure/build passed with Animation stripped:

```bash
cmake --preset linux-shipping
cmake --build --preset linux-shipping
```

Shipping Full/Monolithic:

```bash
cmake -S . -B work/animation-sync-monolithic -G Ninja -DCMAKE_BUILD_TYPE=Shipping \
  -DNEXORA_SHIPPING_PROFILE=Full -DNEXORA_LINK_MODE=Monolithic \
  -DNEXORA_ENABLE_NATIVE_BACKENDS=OFF -DNEXORA_ENABLE_WINDOW_PRESENTATION=OFF \
  -DNEXORA_ENABLE_EDITOR=OFF -DNEXORA_ENABLE_SLANG=OFF \
  -DNEXORA_ENABLE_ZIG_GAMEPLAY=OFF -DNEXORA_ENABLE_MIMALLOC=OFF -DBUILD_TESTING=ON
cmake --build work/animation-sync-monolithic --target NexoraPoseGraphTests
ctest --test-dir work/animation-sync-monolithic -R animation.v2_m8_synchronized_trs_graph --output-on-failure
```

Result: 1/1 passed, 0.04 seconds.

Standalone sanitizer gate:

```bash
c++ -std=c++20 -fsanitize=address,undefined -fno-omit-frame-pointer \
  -IEngine/Animation/include -IEngine/Foundation/include \
  Engine/Animation/src/Pose.cpp Engine/Animation/src/SyncGroup.cpp \
  Engine/Animation/src/PoseGraph.cpp Tests/Animation/PoseGraphTests.cpp \
  -o work/trs-graph-sanitizers
ASAN_OPTIONS=detect_leaks=0 work/trs-graph-sanitizers
```

Result: passed without ASan/UBSan diagnostics. Leak detection was disabled because this environment
uses ptrace; leak validation is not claimed.

Exact Ubuntu GCC 13.3 compile/run also passed with warnings-as-errors:

```bash
work/gcc13/root/usr/bin/x86_64-linux-gnu-g++-13 -std=c++20 -Wall -Wextra -Wpedantic -Werror \
  -IEngine/Animation/include -IEngine/Foundation/include \
  Engine/Animation/src/Pose.cpp Engine/Animation/src/SyncGroup.cpp \
  Engine/Animation/src/PoseGraph.cpp Tests/Animation/PoseGraphTests.cpp \
  -L/usr/lib/gcc/x86_64-linux-gnu/14 -o work/gcc13/trs-graph-tests
work/gcc13/trs-graph-tests
```

The compiler/front end is GCC 13.3; linking uses the host GCC 14 standard library runtime.

Hosted cross-platform CI results belong in the PR.
No local Windows/macOS/mobile or physical-GPU validation is claimed.
