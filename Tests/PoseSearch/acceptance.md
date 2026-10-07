# V2-M8 portable Pose Search acceptance — 2026-10-07

✅ Accepted scope: the optional Foundation-only `NexoraPoseSearch` module, translation feature
extraction, deterministic database rebuild/fingerprint, weighted tag-constrained exact search,
transactional invalid-input rejection, and profile stripping. V2-M8 remains incomplete; V2 stays
46%. This record does not accept GPU animation, motion warping, retargeting, compression, editor
authoring, Motion Matching, or target hardware.

The Linux cloud checkout starts at `27bc395f6bb3315302cd607e5090ad3b662498aa` with the implementation
in this change. Existing local SDK setup is loaded through `/workspace/.nexora/env.sh`. No generated
build output or machine-local preset is committed.

| Command | Result |
| --- | --- |
| `cmake --preset linux-development` | PASS |
| `cmake --build --preset linux-development` | PASS; Modular module/test compiled with warnings as errors |
| `ctest --preset linux-development` | PASS; 104/104, zero failed, 149.94 seconds |
| `cmake --preset linux-shipping` | PASS; Shipping Minimal / Monolithic |
| `cmake --build --preset linux-shipping` | PASS; optional PoseSearch stripped |
| `cmake --build work/pose-search-monolithic --target NexoraPoseSearchTests` | PASS; Shipping Full / Monolithic / LTO |
| `ctest --test-dir work/pose-search-monolithic -R 'animation.v2_m8_pose_search' --output-on-failure` | PASS; 1/1 |
| `ctest --test-dir work/pose-search-monolithic -R 'build.pose_search_profiles' --output-on-failure` | PASS; 1/1, all six configured profiles |
| `ASAN_OPTIONS=detect_leaks=0 work/pose-search-sanitizers` | PASS; AddressSanitizer and UndefinedBehaviorSanitizer |

The explicit Full fixture configure command is:

```bash
cmake -S . -B work/pose-search-monolithic -G Ninja \
  -DCMAKE_BUILD_TYPE=Shipping -DNEXORA_SHIPPING_PROFILE=Full \
  -DNEXORA_LINK_MODE=Monolithic -DNEXORA_ENABLE_NATIVE_BACKENDS=OFF \
  -DNEXORA_ENABLE_WINDOW_PRESENTATION=OFF -DNEXORA_ENABLE_EDITOR=OFF \
  -DNEXORA_ENABLE_SLANG=OFF -DNEXORA_ENABLE_POSE_SEARCH=ON -DBUILD_TESTING=ON
```

The sanitizer fixture compile command is:

```bash
c++ -std=c++20 -DNEXORA_POSE_SEARCH_STATIC=1 -Wall -Wextra -Wpedantic -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer -g \
  -IEngine/PoseSearch/include Engine/PoseSearch/src/PoseSearch.cpp \
  Tests/PoseSearch/PoseSearchTests.cpp -o work/pose-search-sanitizers
```

LeakSanitizer's first run failed because this execution environment uses ptrace; the successful
rerun explicitly disabled leak detection. That rerun verifies ASan/UBSan only, not leak detection.
Windows/macOS/native-device execution was not run in this Linux environment.

The test fixes a canonical little-endian/FNV fixture (`0x3f4b96b6fe5d6232`), compares builds after
reordering input, rejects duplicate keys and malformed/oversized input without losing prior
results, checks constraint/weight/schema identity, owns source copies, canonicalizes signed zero,
and handles finite extreme values with double intermediates. Sixteen tag-filtered queries of a
65,536-sample database agree with an independent nearest-neighbour reference. Feature tests cover
root-relative position, finite-difference velocity, trajectory layout, translation invariance,
invalid timing, non-finite inputs, representability, and dimension limits.

The build gate checks actual CMake File API targets, dependencies, linkage, and compile-source
entries in Development, explicit OFF, Shipping Minimal, Shipping Full, Shipping Dedicated, and
headless configurations. It neither downloads SDKs nor substitutes target existence for compiling
and executing the enabled module.
