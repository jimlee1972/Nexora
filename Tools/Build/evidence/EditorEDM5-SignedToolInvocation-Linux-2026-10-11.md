# ED-M5 signed native tool invocation — Linux, 2026-10-11

Beads: `nexora-032.1.5`. Source starts from review-only foundation
`2af7282d8e4d24fc3c1c74b7e4e1c14a15f728a7`: accepted Main
`0eb7e9a6738d452013ff7bad0ca571ee3345e1ac` plus pending qualified-service,
native callback SDK and scalar-material plugin prerequisites (PRs 523, 525, 526).
Never merge the foundation. Replay only this feature onto Main after prerequisites;
require fresh integration and independent current-head hosted checks for Main merge.
Full Editor milestones remain **0/8**. This is a signed byte-call boundary rather
than graphical reference-tool edit/preview/save acceptance.

## Contract and actual native execution

`SignedExtensionHost::InvokeTool` first checks the invoker construction thread and
shared per-thread native-callback reentrancy guard, before inspecting mutable image,
trust, policy or service state. The exact image admission must retain its sealed
file identity and current trust/policy revisions. Calls then use the SDK's fresh
qualified active-provider lookup and bounded owning results. No private PluginHost,
registry, table, function or context escapes the bridge. Public class data layout,
required C plugin/lifecycle ABI, module dependencies and persisted formats are unchanged.

No invocation prepares, loads, polls, revokes or unloads an image. Removing a key
without prior Poll immediately prevents callbacks while native Snapshot still shows
Loaded. Restoring the same key does not revive the stale trust revision. Policy
changes, foreign provider registries, manual replacement and retired IDs reject.
Owners serialize mutation/unload and drain calls; no lease, lock or sandbox is added.
Only native callback exceptions are contained by the existing SDK; allocation or
other host exceptions are not represented as a universal no-throw guarantee.

Real production scalar-material and recursive native modules are SHA-256 hashed
and signed using the public RFC8032 TEST1 fixture seed/key, never a user credential.
Linux Prepare/Load admits actual sealed exact artifact bytes. Inspect uses production
material parsing/validation; Serialize preserves canonical owning output. A copied
registry passes; another signed host with the same numeric admission fails. Actual
native recursion through a second invoker rejects before host-state access and the
native callback counter remains exactly one. Trust/policy revoke, fresh reload,
retired IDs, wrong-thread rejection and owning-result survival are verified.

The caller separately authorizes execution, parses owning bytes and checks document
type/scope, current source, writer/recovery/Play state before deferred authoring or IO.
Manager selection, graphical contributions, document Undo/Save/reopen and GPU Preview
remain separate work. Native extensions are trusted in-process code, not isolated
against hostile memory access or filesystem races.

## Accepted graphical Linux gates

```sh
cmake --preset linux-development -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON \
  -DNEXORA_ENABLE_CRYPTOGRAPHY=ON -DNEXORA_ENABLE_SLANG=ON \
  -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON -DNEXORA_BUILD_SHOWCASE=ON \
  -DNEXORA_BUILD_PROJECT_PLAYER=ON -DNEXORA_ENABLE_PROJECT_PLAYER_NATIVE=ON \
  -DNEXORA_FEATURE_SCALAR_MATERIAL_TOOL=ON
cmake --build --preset linux-development -j4
ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping -j4
```

Configured toolchain and pinned ImGui/Vulkan sources were used. Corrected focus built
**8 remaining steps**, **7/7 in 0.64 s**. The initial partial compile failed because
ServiceRegistry registration requires an owning string; explicit string construction
corrected the test. The failed 92-step attempt is excluded from accepted gates.
Fresh full graphical Development built **310 remaining steps**, **247/247 tests in
595.72 s**, zero skips; actual signed tool invocation passed in **0.01 s**.
Minimal Monolithic Shipping passed **14 steps**; cache confirms Editor, SDK and scalar
tool forced OFF. No graphical Shipping, physical display or other-platform local
acceptance is claimed. Linux uses Xvfb and Mesa software Vulkan.

## Explicit unavailable-provider gate

A separate portable Development directory uses `NEXORA_CRYPTOGRAPHY_BACKEND=NONE`,
Cryptography/Editor/SDK/scalar tool ON and graphical/native backends, Slang/Zig,
Showcase and ProjectPlayer OFF. This changes no graphical build cache.

```sh
cmake --preset linux-development -B build/linux-signed-tool-provider-none \
  -DNEXORA_CRYPTOGRAPHY_BACKEND=NONE -DNEXORA_ENABLE_CRYPTOGRAPHY=ON \
  -DNEXORA_ENABLE_EDITOR=ON -DNEXORA_ENABLE_EDITOR_SDK=ON \
  -DNEXORA_FEATURE_SCALAR_MATERIAL_TOOL=ON \
  -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=OFF -DNEXORA_ENABLE_NATIVE_BACKENDS=OFF \
  -DNEXORA_ENABLE_WINDOW_PRESENTATION=OFF -DNEXORA_ENABLE_SLANG=OFF \
  -DNEXORA_ENABLE_ZIG_GAMEPLAY=OFF -DNEXORA_BUILD_SHOWCASE=OFF \
  -DNEXORA_BUILD_PROJECT_PLAYER=OFF -DNEXORA_ENABLE_PROJECT_PLAYER_NATIVE=OFF \
  -DNEXORA_FEATURE_EXAMPLE_PLUGIN=OFF
cmake --build build/linux-signed-tool-provider-none \
  --target NexoraSignedToolInvocationTests NexoraSignedExtensionTests -j4
ctest --test-dir build/linux-signed-tool-provider-none \
  -R '^editor\.(signed_tool_invocation|signed_extension_admission)$' --output-on-failure
```

The real targets built **97 steps**; **2/2 in 0.01 s**. Missing signed admissions and
wrong-thread calls reject; signed native execution remains explicitly unavailable.
This is unavailable-provider acceptance, not successful native material invocation.
Documentation regressions passed **16/16 in 0.553 s**; changed-document links, five
touched C++ format checks and diff whitespace checks pass. Root README is unchanged.
