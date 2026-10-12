# ED-M5 bounded native tool callbacks — Linux, 2026-10-11

Beads: `nexora-032.1.3`, depending on qualified-service task `nexora-032.1.2` / PR 523.
Current delivery replays only this SDK feature onto Main
`2a0a7d57430fd9306275cc1d6441453e0c1526a5`, including the accepted artifact
verification, atomic-property and prefab-history changes. The frozen runtime is
`7d5e31fd522e9cdc4f8e5939f34545dc6008836f`; the pre-gate review source is `489bc9724a578bb4ffb711dc7c25bbe6acf80c55`. No review-only
prerequisite copies remain. The previous Main-660 integration passed all 36
hosted checks at `6af07337`, but artifact-verification delivery subsequently
caused README/test-CMake conflicts. Both independent contracts and targets were
preserved in this replay; the new head requires new hosted checks.
This is byte-callback SDK support; graphical reference-tool acceptance and full
Editor milestones remain **0/8**.

## Public contract and actual execution

Foundation's optional C `EditorToolAbi.h` defines separately versioned schema/interface
1, four single-bit Inspect/Edit/Preview/Serialize operations and explicit status codes.
The required plugin ABI and cooperative lifecycle schema are unchanged. Registered
native tables are readable and immutable for their admission; native code is trusted,
obeys buffer capacities and does not throw. This is not a native-code sandbox.

Editor `NativeToolInvoker` checks its construction thread and a per-thread guard shared
across invokers before registry access. Each synchronous call performs a fresh qualified
admission/provider lookup; it retains no table, function or context. Declared table sizes
are bounded to 4096 bytes; schema/interface/mask/callback and capacities are checked
before allocation/invocation. Host-owned input/output are each bounded to 64 KiB and
respect smaller provider limits. Success must explicitly report a size within capacity.
Unknown status, missing/oversized success size and unexpected exceptions reject; every
failure discards logical output. Results own their data and survive native unload.
Consumers serialize with host/registry mutation and drain calls before unload; there
is no lifetime lease or automatic in-flight accounting.

Fourteen real compiled native modules execute through PluginHost, rather than injected
callbacks. They prove binary/empty and exact 64 KiB output, copied registries, two hosts
with equal IDs, manual replacement, revoked copies, reload/retired IDs, malformed table
fields, unknown and known failure statuses, capacity/missing-size rejection, exception
containment, provider-specific limits and actual cross-invoker recursion rejection.
A separate C translation unit compiles and calls the public C table locally; this C
consumer is not represented as a dynamically loaded C plugin.

The SDK exposes no SceneDocument/workspace/GPU objects and performs no file IO, authoring
history or publication. Byte-operation declarations grant no document or IO authority.
A future host must authorize execution, parse owning outcomes and recheck document type,
current scope/source/writer/recovery/Play state before deferred authoring or IO.
Graphical controls, production reference-plugin integration, backend restoration and
edit/preview/save/reopen acceptance remain separate work. Public C++ consumers rebuild;
module graph and persisted formats are unchanged.

## Historical accepted Linux gates

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

Pinned ImGui/Vulkan source caches and the configured Linux toolchain were used.
Expanded fourteen-module focused gate built **32 remaining steps**, **2/2 in 0.01 s**.
Full graphical Development rebuilt **336 remaining steps**, **244/244 in 593.63 s**,
zero skips, including native Inspector, project upgrade, scene tabs/preview, Game View,
Showcase and ProjectPlayer. Minimal Monolithic Shipping passed **5 steps**, with Editor
and SDK excluded by profile; this is not graphical Shipping execution. Software Vulkan
and Xvfb do not provide physical-display acceptance. Other platforms require their own
current-head hosted checks and are not attributed to Linux execution.

Documentation regressions passed **16/16 in 0.560 s**; changed-document links, touched
C/C++ formatting and diff whitespace checks pass. Root README is unchanged.

## Historical Main-660 integration acceptance

The exact commands above passed on the frozen Main-660 source: graphical Development
rebuilt **403 steps**, then **247/247 tests in 639.15 s**, zero skips. Minimal
Monolithic Shipping passed **14 steps**. The prerequisite is delivered on Main;
no review-only prerequisite copies remain in this SDK branch. Main advances during
the running gate are recorded rather than attributed to the frozen source.

## Current Main-2a0a reconciliation acceptance

The same graphical Development/Minimal Shipping commands above passed on the
frozen current-main runtime: **366 build steps**, **250/250 tests in 652.72 s**,
zero failures and skips, then **5 Monolithic Shipping steps**. Actual native
scene tabs, preview, meshes, reflected Inspector, project upgrade, Play/Game and
build-console workflows ran alongside artifact verification and the SDK's
fourteen compiled callback fixtures. Shipping excludes Editor/SDK by profile;
it is not graphical Shipping execution. Original callback behavior and all
budgets/assertions are unchanged. Other target platforms require the new
head's hosted checks; previous-head results are not carried forward as approval.
