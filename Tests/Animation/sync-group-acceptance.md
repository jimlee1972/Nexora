# V2-M8 portable marker synchronization acceptance

Date: 2026-10-08. Host: Linux cloud, GCC 14.2. Toolchain:
`source /workspace/.nexora/env.sh`. Scope: `SyncGroup` in the Foundation-only optional `NexoraAnimation` module
(C++ namespace `nexora::animation`) and the Runtime visual synchronization entry point.

`animation.v2_m8_marker_sync` checks independent marker-segment expectations, cyclic marker order,
wrap segments, normalized fallback, stable ID tie-breaking, weight/leader switching, input ownership,
transactional rejection, all-zero pause/resume, tick partitioning, 256 members/256 markers, subnormal
durations, and DBL_MAX ticks/durations. `animation.v2_m8_sync_group_graph` drives two Runtime graphs
across loops and leader changes, checking visual poses and zero seek displacement. The existing
`runtime.v1_m9_presentation` gate checks rejected seeks, crossfade cancellation, and ordinary
post-seek root motion. No event or motor/controller ownership is transferred to the sync group.

```bash
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development
```

Result: configure/build passed; final CTest passed 108/108, 0 failures, 0 skips, in 140.31 seconds.
This includes the seven existing Linux virtual-display/Vulkan probes using software Vulkan.
The local preset has the graphical editor disabled; this is not physical GPU or graphical editor
acceptance.

```bash
cmake --preset linux-shipping
cmake --build --preset linux-shipping
```

Result: Shipping Minimal / Monolithic configure/build passed, with Animation stripped.

```bash
cmake -S . -B work/animation-sync-monolithic -G Ninja -DCMAKE_BUILD_TYPE=Shipping -DNEXORA_SHIPPING_PROFILE=Full -DNEXORA_LINK_MODE=Monolithic -DNEXORA_ENABLE_NATIVE_BACKENDS=OFF -DNEXORA_ENABLE_WINDOW_PRESENTATION=OFF -DNEXORA_ENABLE_EDITOR=OFF -DNEXORA_ENABLE_SLANG=OFF -DNEXORA_ENABLE_ZIG_GAMEPLAY=OFF -DNEXORA_ENABLE_MIMALLOC=OFF -DBUILD_TESTING=ON
cmake --build work/animation-sync-monolithic --target NexoraSyncGroupTests NexoraV1M9Tests NexoraSyncGroupGraphTests
ctest --test-dir work/animation-sync-monolithic -R 'animation.v2_m8_(marker_sync|sync_group_graph)|runtime.v1_m9_presentation' --output-on-failure
```

Result: enabled Shipping Full / Monolithic build and all three selected gates passed.
`build.animation_profiles` inspects actual targets and both implementation sources in Development,
explicit OFF, Shipping Minimal, Shipping Full, Shipping Dedicated, and headless configurations.
Enabled Animation retains Foundation as its only dependency; disabled profiles contain neither
the Animation target nor its sources. Only the integration test explicitly links Animation/Runtime.

```bash
c++ -std=c++20 -fsanitize=address,undefined -fno-omit-frame-pointer -IEngine/Animation/include Engine/Animation/src/SyncGroup.cpp Tests/Animation/SyncGroupTests.cpp -o work/marker-sync-sanitizers
ASAN_OPTIONS=detect_leaks=0 work/marker-sync-sanitizers
```

Result: ASan/UBSan passed. Leak detection is disabled in this ptrace-constrained environment;
no leak-check acceptance is claimed. Changed Markdown validation and `git diff --check` passed.

The initial push workflow [37723799069](https://github.com/jimlee1972/Nexora/actions/runs/37723799069)
failed four Linux jobs because GCC 13.3 crashed compiling nested aggregate extreme-value fixtures.
The same Ubuntu GCC 13.3.0-6ubuntu2~24.04.1 packages were unpacked under `work/gcc13`; the original
fixture reproduced the identical `gimplify_var_or_parm_decl` ICE. Explicit field assignment retains
all values/assertions and fixes compilation with warnings-as-errors. The corrected standalone
sync-group test also builds and runs with that compiler (using the host libstdc++ runtime):

```bash
work/gcc13/root/usr/bin/x86_64-linux-gnu-g++-13 -std=c++20 -Wall -Wextra -Wpedantic -Werror -IEngine/Animation/include -c Tests/Animation/SyncGroupTests.cpp -o work/gcc13/sync-group-fixed.o
work/gcc13/root/usr/bin/x86_64-linux-gnu-g++-13 -std=c++20 -Wall -Wextra -Wpedantic -Werror -IEngine/Animation/include Engine/Animation/src/SyncGroup.cpp Tests/Animation/SyncGroupTests.cpp -L/usr/lib/gcc/x86_64-linux-gnu/14 -o work/gcc13/sync-group-tests
work/gcc13/sync-group-tests
```

The full local gate, Monolithic selected gates, and ASan/UBSan above passed again after the fixture
change. The initial failing workflow is not acceptance evidence; hosted CI must pass the corrected
PR head before merge.

This evidence accepts portable looping marker clocks and the translation graph's visual seek.
Marker authoring, compressed TRS graph integration, inertialization, motion warping, GPU animation,
and optional Motion Matching remain open. V2-M8 remains unaccepted and total V2 progress stays 46%.
Windows/macOS/Android/iOS validation is not part of this local Linux evidence.
