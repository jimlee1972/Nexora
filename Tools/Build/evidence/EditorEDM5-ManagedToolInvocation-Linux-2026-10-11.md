# ED-M5 scoped managed native tool invocation — Linux, 2026-10-11

Beads: `nexora-032.1.6`, depending on signed bridge `nexora-032.1.5` / PR 527 and
Plugin Manager `nexora-62u.3.2` / PR 487. Review-only foundation
`6f18693f` starts from accepted Main
`1023b19aac8ad84ffd1b7b05a429c3e6f295cfad`
plus pending Manager/native SDK/corrected scalar-material/signed-bridge prerequisites and
the corrected, rendered initial-dock source-selection fixture. Qualified service admission
is already on Main. Runtime implementation is frozen in private commit `9cb9808b`;
the corrected full replay gate passed as recorded below.
The initial full gate below used foundation `096a175a` on Main `0eb7e9a6`;
PR 486 merged during that gate. Current replay retains both Prefab/Extensions
shortcuts with their independent focus/input guards. Fresh replay acceptance is
recorded separately after the updated full gate.
Never merge the foundation. Main delivery replays only this feature after prerequisites,
with fresh integration acceptance and independent current-head hosted checks.
Graphical reference-tool acceptance and full Editor milestones remain **0/8**.

## Public owner contract

`ManagedToolSelection` owns exact package identity/version, nonwrapping manager instance,
project scope/configuration revision and native admission. `SelectTool` observes enabled
metadata, retaining no native pointer. Manager-owned `NativeToolInvoker` checks its
construction thread/shared per-thread callback guard before mutable manager/project/host
inspection. Arbitrarily supplied invokers cannot substitute another construction thread.
Instance allocation is atomic and never wraps/reuses old identities; exhaustion rejects.

Each call checks exact token and resolved current workspace before selected Loaded state
and the signed host's current image/trust/policy and qualified-provider checks. Stale,
disabled, retired, foreign, wrong-thread/reentrant and read-only/unresolved project calls
reject with no callback. Calls do not poll or change lifecycle. Built-in Editor/Foundation
module names join existing core/reflection declarations, permitting actual public-API tool
manifests without fake installed engine packages. Other dependencies still require enabled
installed providers. No package enables or enrolls trust automatically.

Successful outcomes own bounded bytes and survive native disable/unload. Actual document
scope/type, current source, Play state, parsing, authoring, writer/recovery and deferred
Undo/IO remain independent host obligations. Native permission masks remain admission
metadata rather than an OS sandbox or publication grant. Existing serialized drain-before-
mutation/unload contracts remain. Public C++ class layout changes and consumers rebuild;
required plugin C/lifecycle ABI, persisted formats and module dependencies are unchanged.

## Actual signed native workflow

Real production scalar-material artifact bytes are hashed and signed with RFC8032's public
TEST1 fixture key, reviewed, installed unchanged, then explicitly enabled via Plugin Manager.
Inspect/Edit/Serialize use production parsing/validation, with roughness 0.75 and unchanged
other lanes. GPU Preview rejects as unavailable before callback. Invalid scope/configuration/
identity/version/admission/oversized identity and foreign managers with identical numeric
admission, scope and configuration reject. Actual installed bytes remain exact.

Read-only observer, unresolved recovery and external workspace change reject without
rewriting files or polling lifecycle. Exact restoration permits current calls. Disable,
fresh re-enable, publisher/policy revoke/restore and detach invalidate old observations;
owning results remain valid. A real signed recursive native fixture attempts Manager
selection/invocation from its callback: both reject reentry and its callback count is one.
Actual wrong-thread tests include enabled providers, not only missing metadata.

## Initial graphical Linux gates

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

Configured Linux toolchain and pinned ImGui/Vulkan caches were used. Actual focus built
**50 steps**, **4/4 in 0.08 s**; managed invocation passed in **0.04 s**. Fresh full
Development built **337 remaining steps**, **251/251 in 620.02 s**, zero skips.
Minimal Monolithic Shipping passed **5 steps**, excluding Editor, SDK and scalar tool.
Xvfb/software Vulkan are not physical-display acceptance. No other-platform local or
graphical Shipping execution is claimed. Native graphical extensions regressions remain
part of the full gate; this does not claim graphical material-document contributions.

## Corrected unavailable-provider focus

A separate portable Development build uses Cryptography provider NONE, Editor/SDK/scalar tool
ON and graphical/native backends, Slang/Zig, Showcase and ProjectPlayer OFF. Its initial build
failed under `-Werror` because two OpenSSL-fixture-only file helpers were unused. Guarding those
helpers with the existing signed-fixture platform/provider condition changes no product code.
The failed initial build is excluded. The corrected provider-NONE manager/signed/managed tests
passed **3/3 in 0.02 s**. Normal graphical Development rebuilt the managed test in **2 steps**;
its four focused tests passed again **4/4 in 0.08 s**. These focused results precede the current
Main replay; current replay acceptance is recorded below.

## Corrected current-Main full acceptance

On foundation `6f18693f` / Main `1023b19a`, the same graphical Development commands above
built **398 steps**, **255/255 in 665.09 s**, zero failed/skipped. Actual managed invocation
passed in **0.05 s** and the actual signed Manager graphical workflow in **25.75 s**.
The scalar grammar/subnormal fix and correctly observed initial-dock source selection are
included; old hosted scalar/source-selection failures are not accepted as successful gates.
Minimal Monolithic Shipping passed **5 steps**. This still does not claim graphical material
editing or native GPU Preview, which are independent downstream work.

The existing portable Development directory was freshly configured with Cryptography NONE,
SDK/scalar ON and graphical/native backends, presentation, Slang/Zig, Showcase and ProjectPlayer
OFF. Fresh build of `NexoraManagedToolInvocationTests`, `NexoraPluginManagerTests` and
`NexoraSignedToolInvocationTests` passed **52 steps**, **3/3 in 0.01 s**. Current missing-backend,
selection and owner-context rejection run without pretending that signed native execution
occurred in this unavailable-provider configuration. Both commands exited zero.

Final documentation CI passed **16/16 in 0.576 s**. Changed links including this new
evidence, touched C++ formatting and diff checks passed. Root milestone summary is
unchanged: this delivered owner is a supporting slice, not graphical reference-tool acceptance.
