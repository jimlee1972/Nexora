# V2-M8 portable inertial TRS blend acceptance

Scope: velocity-aware local TRS correction in optional Foundation-only Animation.
V2-M8 remains open; overall V2 progress remains 46%.

## Delivered behavior

- Owned outgoing-minus-incoming translation/log-scale and shortest relative rotation-vector offsets.
- Optional caller-provided residual derivatives; no outgoing clip sampling after Begin.
- Quintic boundary conditions: initial offset/velocity and zero initial residual acceleration;
  residual position/velocity/acceleration zero at a finite deadline.
- Current incoming poses remain live throughout the correction; normalized owning TRS output.
- 1–512 joints, transactional Begin and Update, zero-tick sampling, saturated large ticks, Reset.
- Invalid channels, mismatched counts, duration-scaled velocity overflow/vanishing, reconstructed
  translation/scale overflow or scale collapse reject without partial output or clock advancement.
- Consumes the synchronized compressed TRS graph output without changing module/C ABI dependencies.

Rotation-vector velocity means the derivative of the principal relative rotation vector in
parent-local axes, not world angular velocity. Consumers own skeleton identity/joint order,
history estimates, rotation branch handling near pi, interruption policy and clip/event authority.
Velocity can overshoot; this is not a monotonicity or joint-limit guarantee. Runtime adapters,
transform/root-motion authority, motion warping, GPU execution and editor/cook integration remain
open. Floating-point precision affects tiny clock increments and coefficient rounding.

## Independent acceptance tests

`animation.v2_m8_inertial_pose_blend` checks initial pose and finite-difference residual velocities,
known midpoint TRS, 101 samples against an expanded polynomial reference, moving targets, terminal
pose, short-arc 170/-170-degree rotation, q/-q equivalence, noncommuting axes against independent
Slerp, quaternion normalization, compressed-graph consumption, tick partitioning, deadlines/reset,
invalid-state preservation, finite maximum/denormal extremes, large-target cancellation and tiny
departures, extreme initial scale endpoints, overflow/collapse, and joint budgets.
`build.animation_profiles` checks all four implementation sources and the Foundation-only graph
in six optional/Shipping/headless configurations.

## Local validation

Linux cloud, GCC 14.2, graphical Editor enabled. Final gate:

```bash
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development
```

Result: configure/build passed; 161/161 tests passed, 0 failures/skips, 340.62 seconds.
Includes graphical Editor native probes and software-Vulkan/virtual-display acceptance.

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
cmake --build work/animation-sync-monolithic --target NexoraInertialBlendTests
ctest --test-dir work/animation-sync-monolithic -R animation.v2_m8_inertial_pose_blend --output-on-failure
```

Shipping Full/Monolithic result: 1/1 passed, 0.01 seconds.

Standalone ASan/UBSan:

```bash
c++ -std=c++20 -fsanitize=address,undefined -fno-omit-frame-pointer \
  -IEngine/Animation/include -IEngine/Foundation/include \
  Engine/Animation/src/Pose.cpp Engine/Animation/src/SyncGroup.cpp \
  Engine/Animation/src/PoseGraph.cpp Engine/Animation/src/InertialBlend.cpp \
  Tests/Animation/InertialBlendTests.cpp -o work/inertia-sanitizers
ASAN_OPTIONS=detect_leaks=0 work/inertia-sanitizers
```

Result: passed without diagnostics. Leak detection unavailable under ptrace; no leak gate claimed.

Exact Ubuntu GCC 13.3 front-end with host GCC 14 library runtime:

```bash
work/gcc13/root/usr/bin/x86_64-linux-gnu-g++-13 -std=c++20 -Wall -Wextra -Wpedantic -Werror \
  -IEngine/Animation/include -IEngine/Foundation/include \
  Engine/Animation/src/Pose.cpp Engine/Animation/src/SyncGroup.cpp \
  Engine/Animation/src/PoseGraph.cpp Engine/Animation/src/InertialBlend.cpp \
  Tests/Animation/InertialBlendTests.cpp -L/usr/lib/gcc/x86_64-linux-gnu/14 \
  -o work/gcc13/inertia-tests
work/gcc13/inertia-tests
```

Result: compiled and passed. Hosted CI results belong in the PR; no local Windows/macOS/mobile
or physical-GPU validation claimed.
