# V2-M8 compressed pose and local retarget acceptance

Date: 2026-10-07. Host: Linux cloud, GCC 14.2; preinstalled toolchain selected by
`source /workspace/.nexora/env.sh`. Scope: optional Foundation-only `NexoraAnimation`.

The contract test round-trips 256 frames × 32 joints (8,192 poses), checks scalar and angular
error, fractional sampling, compressed payload size, exact little-endian header, q/-q and
signed-zero canonicalization, truncated/invalid/versioned data, finite extremes, strong
load/build rejection, and moved-from safety. Retarget tests independently check bind identity,
90-degree bind-axis alignment, explicit 2× translation ratio, scale ratio, unmapped target bind
preservation, invalid/cyclic/missing-parent/duplicate mapping rejection, and numeric overflow.

Validation commands:

```bash
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development
```

Result: Development configure/build passed; CTest reports 106 tests, 0 failures.
Seven existing native display tests were skipped: `showcase.linux_vulkan_interaction`,
`showcase.linux_vulkan_virtual_display`, `showcase.linux_vulkan_rendering_room`,
`showcase.linux_vulkan_camera_input`, `window_presentation.vulkan_scene`,
`window_presentation.vulkan_pbr`, and `window_presentation.vulkan_scene_upload_reuse`.
These skips do not establish Vulkan display acceptance.

```bash
cmake --preset linux-shipping -DFETCHCONTENT_SOURCE_DIR_NEXORA_VULKAN_HEADERS=/workspace/Nexora/build/linux-development/_deps/nexora_vulkan_headers-src
cmake --preset linux-shipping
cmake --build --preset linux-shipping
```

Result: Shipping Minimal / Monolithic configure/build passed. Initial fresh configuration
could not reach the network proxy to fetch Vulkan-Headers; the successful configuration reused
already-installed pinned headers from Development, without changing repository dependencies.

```bash
cmake -S . -B work/animation-shipping-full -G Ninja -DCMAKE_BUILD_TYPE=Shipping -DNEXORA_LINK_MODE=Monolithic -DNEXORA_SHIPPING_PROFILE=Full -DNEXORA_ENABLE_NATIVE_BACKENDS=OFF -DNEXORA_ENABLE_WINDOW_PRESENTATION=OFF -DNEXORA_ENABLE_ZIG_GAMEPLAY=OFF -DBUILD_TESTING=OFF
cmake --build work/animation-shipping-full --target NexoraAnimation
```

Result: the enabled Shipping Full / Monolithic animation library compiled and linked.
`build.animation_profiles` also checks the CMake File API and compile source graph for
Development, explicit OFF, Shipping Minimal, Shipping Full, Shipping Dedicated, and headless;
only Development/Full contain the animation target/source, with Foundation as the sole dependency.

This evidence accepts compressed pose storage and explicit compatible-hierarchy local
retargeting only. No Windows/macOS/Android/iOS, GPU animation, editor/cook integration,
IK/topology-changing retarget, motion warping, inertialization, sync groups, or full V2-M8
acceptance is claimed. Overall V2 milestone progress remains 46%.
