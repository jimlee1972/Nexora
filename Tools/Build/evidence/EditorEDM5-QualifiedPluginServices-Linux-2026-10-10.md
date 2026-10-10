# ED-M5 qualified plugin service borrowing — Linux, 2026-10-10

Beads: `nexora-032.1.2`. Source starts from accepted Main
`81dbc3dc2cca8c0a54f81f0ed3ee8d78cc8321b2`. PR 521's separate native Save input
fix merged while this full gate ran; current published-head hosted checks and a fresh
clean Main guard are still required. This is a Runtime tool-SDK prerequisite, not
complete graphical reference-plugin or ED-M5 acceptance. Full milestones remain **0/8**.

## Contract and actual native fixtures

The additive public `PluginHost::FindService(id, registry, name)` checks a loaded
admission, active owned provider and exact registry provider identity before returning
a borrowed pointer. It performs no registration, native callback, IO or lifecycle mutation.
Queries reject missing IDs/services, empty or embedded-NUL names and names over 256 bytes.
Serialized nonreentrant owner-thread access and release-before-mutation/unload still apply.
There is no lifetime lease, in-flight tracking, permission grant or native-code sandbox.
Required C plugin ABI, lifecycle schema, class data layout, module graph and persisted
formats are unchanged; public C++ consumers rebuild.

The new test dynamically loads actual built ExamplePlugin and lifecycle modes 2, 3
and 13. Two hosts deliberately have identical numeric admission IDs and service names;
cross-host lookup rejects. Active registry copies qualify, while manual same-name
replacement does not. Unload revokes copied visibility without removing the manual
service or another host's live provider. Reload receives a new ID and rejects the
retired ID. A real worker remains mapped while shutdown is pending but its service
is already unavailable; cooperative polling quiesces before unload. A legacy mapping
remains resident/restart-required with service visibility revoked. The real 64-service
fixture accepts an exact 256-byte UTF-8 name and rejects a 257-byte lookup. Borrowed
results are consumed before each mutation/unload, and rejected queries conserve state.

## Validation

Linux cloud uses the configured GCC/CMake/Ninja toolchain, Slang, Zig, pinned ImGui
and Vulkan header source caches, Xvfb and Mesa software Vulkan. No physical-host or
other-platform local acceptance is claimed.

```sh
cmake --preset linux-development -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON \
  -DNEXORA_ENABLE_CRYPTOGRAPHY=ON -DNEXORA_ENABLE_SLANG=ON \
  -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON -DNEXORA_BUILD_SHOWCASE=ON \
  -DNEXORA_BUILD_PROJECT_PLAYER=ON -DNEXORA_ENABLE_PROJECT_PLAYER_NATIVE=ON
cmake --build --preset linux-development -j4
ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping -j4
```

Expanded focused SDK/lifecycle/ownership tests passed **4/4 in 0.03 s**. Fresh full
graphical Development configure/build passed **332 steps**; **242/242 in 592.58 s**,
zero skips, including actual native Inspector, scene tabs and scene preview. Minimal
Monolithic Shipping configure/build passed **5 steps**, proving the minimal profile
with SDK/Editor excluded rather than a graphical Shipping build.

An explicit Full Monolithic Shipping SDK configuration verifies the new API where
it is enabled:

```sh
cmake --preset linux-shipping -B build/linux-qualified-service-shipping \
  -DNEXORA_SHIPPING_PROFILE=Full -DBUILD_TESTING=ON \
  -DNEXORA_ENABLE_EDITOR_SDK=ON -DNEXORA_FEATURE_EXAMPLE_PLUGIN=ON
cmake --build build/linux-qualified-service-shipping --target \
  NexoraPluginServiceOwnershipTests NexoraPluginLifecycleTests \
  NexoraPluginLifecycleC NexoraV1M6Tests -j4
ctest --test-dir build/linux-qualified-service-shipping \
  -R '^runtime\.(plugin_service_ownership|plugin_lifecycle|plugin_lifecycle_c_abi|v1_m6_editor_sdk)$' \
  --output-on-failure
```

Full SDK build passed **88 steps**, actual native tests **4/4 in 0.02 s**. Documentation
regressions passed **16/16 in 0.560 s**; changed-document links, touched C++ formatting
and diff whitespace checks pass. Root README is unchanged. Hosted Windows/macOS and
other targets require independent current-head CI; none are attributed to Linux cloud.
