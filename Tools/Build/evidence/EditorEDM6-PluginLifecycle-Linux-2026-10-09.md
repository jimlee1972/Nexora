# ED-M6 cooperative plugin lifecycle — Linux cloud evidence

Beads: nexora-62u.3.1. Scope: real PluginHost prerequisite, not complete PluginManager or ED-M6.
Final source integrates accepted main bac7247be152c9a85e7eb757fbc0dee602417f90.

## Delivered contract

Optional independently versioned C lifecycle getter preserves the required engine ABI and legacy
registration signature. ABI checks precede lifecycle inspection/registration. Complete prefix,
schema, callback and result validation precede activation. Registration stages at most 64 attempted
services with names up to 256 UTF-8 bytes; invalid/duplicate/excess/throwing callbacks revoke the
whole admission. Provider visibility is shared weakly in registry copies, with no retained registry
pointer after synchronous registration. Manual services remain caller-owned.

Disable revokes visibility before requesting shutdown. Native unload requires actual acknowledged
quiescence. Pending/legacy/rejected shutdown mappings remain resident until restart, including host
destruction; small callback contexts are intentionally pinned without a registry/World borrow.
Callers drain their borrowed services/jobs first; owner-thread calls are serialized and nonreentrant.
There is no automatic in-flight service tracking, forced unload, sandbox, signature verification or
arbitrary native crash isolation. Native callbacks must obey the cooperative no-throw contract.

Owning observations retain path/ID/ABI/state/raw callback result/load and lifecycle errors, without
native handles. Lifetime admission is bounded to 128 rows per host and paths to 32 KiB NUL-free bytes.
Windows uses UTF-8-to-native-wide absolute paths; POSIX preserves native bytes. C++ consumers rebuild.
The ExamplePlugin implements shutdown/quiescence; the Foundation public header also compiles as C.

## Actual fixture proof

Fourteen compiled native modules are dynamically copied/loaded from a Unicode/spaced directory.
Checks cover real background-thread quiescence before native destructor events, immediate unload,
legacy admission/restart, failed shutdown/poll raw results, absent/mismatched ABI before other calls,
invalid lifecycle schema/size/callback, exception rollback, exact 64-service/name-256 UTF-8 admission,
65th-call rejection before reading an invalid pointer, invalid/null names/services, copied-registry
revocation, caller-name collision, short-lived registries and lifetime-128 reload exhaustion.
Owned observations survive state changes and manual service pointers remain visible.

Initial twelve-module focused tests passed 3/3 in 0.03s. Extending mode 13 exposed a journal matcher
that found mode 3 inside a mode-13 line. Complete-line matching fixed the false assertion;
fourteen-module focused tests passed 3/3 in 0.02s. Native unload/restart assertions were retained.
An initial unused-function warning in the legacy fixture was fixed by compiling its optional
callbacks only when the fixture exports the getter; production warnings/assertions were unchanged.

## Required gates

Linux cloud uses GCC 14.2, CMake 3.31.6, Ninja, Slang 2026.18, Zig, Xvfb and Mesa Vulkan software
rendering. Hosted Windows/macOS checks are separate; no physical display/device acceptance is claimed.

~~~sh
cmake --preset linux-development -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON \
  -DNEXORA_ENABLE_SLANG=ON -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON -DNEXORA_BUILD_SHOWCASE=ON \
  -DNEXORA_BUILD_PROJECT_PLAYER=ON
cmake --build --preset linux-development -j4
ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping -j4
cmake --preset linux-shipping -B build/linux-plugin-sdk-shipping \
  -DNEXORA_SHIPPING_PROFILE=Full -DBUILD_TESTING=ON \
  -DNEXORA_ENABLE_EDITOR_SDK=ON -DNEXORA_FEATURE_EXAMPLE_PLUGIN=ON
cmake --build build/linux-plugin-sdk-shipping --target \
  NexoraPluginLifecycleTests NexoraPluginLifecycleC NexoraV1M6Tests -j4
ctest --test-dir build/linux-plugin-sdk-shipping \
  -R 'runtime.plugin_lifecycle|runtime.v1_m6_editor_sdk' --output-on-failure
~~~

Initial graphical gate: 203/203, zero skips, 394.82s; Minimal Shipping: 74 steps.
Explicit Full Monolithic Shipping SDK build: 86 steps, actual focused tests 3/3 in 0.02s.
Final accepted-main integration: **205/205**, zero skips, **415.35s**;
Minimal Shipping rebuild passes; Full Shipping SDK rebuild and tests 3/3 in 0.02s pass.
Touched C++ clang-format and git diff checks pass. Root README and full milestone counts remain unchanged.
