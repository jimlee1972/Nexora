# ED-M5 public-API scalar material native plugin — Linux, 2026-10-11

Beads: `nexora-032.1.4`, depending on callback SDK task `nexora-032.1.3` / PR 525.
Review-only foundation `145f2000b324f00159faac2cbdeaf548006dbaf5` starts from
accepted signed-admission Main `e4f62f179350e24f20c3af630ac75e66f6444da4`, then copies
pending qualified-service PR 523 and callback SDK PR 525. Never merge the foundation.
The test-registration conflict retained both signed admission and SDK fixtures.
PR 515 merged during the full gate; its separate dormant-history change is not
attributed to the tested starting source. Eventual Main delivery requires landed
prerequisites, replay of only this feature and fresh full/current-head acceptance.

This is a production-subsystem-backed byte plugin, not complete graphical reference-tool
acceptance. Full Editor milestones remain **0/8**.

## Actual public backend and owning wire contract

`ScalarMaterialPlugin` includes only public Editor/Renderer/Foundation headers and uses
production `ImportMaterial`, `ValidateMaterialAsset` and new `ExportMaterial`. No mock
material parser or private engine accessor is used. Required plugin ABI and cooperative
lifecycle schema are unchanged. Module/manifest declares the public Editor dependency;
`NEXORA_FEATURE_SCALAR_MATERIAL_TOOL` is optional and defaults to the Editor build option.
The module is built but not automatically loaded; native code remains trusted.

Inspect/Serialize validate schema-1 scalar opaque PBR source and return canonical source.
Edit accepts `NXM1`, a little-endian uint32 lane, little-endian IEEE-754 binary32 value,
then exact source. Nine lanes cover base RGB, metallic, roughness, occlusion and emission
RGB. Production finite/range validation rejects NaN/infinity and unsupported data; only
selected canonical scalars and their derived Renderer reflection change. Input is at most
64 KiB including the 12-byte edit prefix. Public canonical export uses the classic locale,
float max_digits10 and a 1024-byte output limit. Output copies only after successful full
validation and capacity admission. No IO, document mutation, Undo or publication occurs.

The immutable service table supports Inspect/Edit/Serialize; GPU Preview is undeclared and
unavailable. Stateless synchronous lifecycle has no global shutdown flag: qualified host
visibility revokes per admission without disabling another live host. Calls serialize and
drain before unload; owning SDK outcomes survive unload. Hosts must separately authorize
native execution and document/IO operations and recheck current document/source/writer/
recovery/Play scope. Signed-package material-tool admission, UI controls, GPU preview,
Save/reopen/Undo and missing-plugin document restoration remain separate work. Unknown or
unsupported material payloads stay host-owned and must not be silently replaced.

## Real native and profile validation

Focused tests load the actual production module. They cover nine independent scalar edits,
unchanged unselected values and source bytes, owning canonical inspection, idempotent
serialization, signed zero/subnormal/minimum-normal/nextafter/one exact float bit roundtrips,
malformed wire/source, nonfinite/range/lane failures, insufficient native output capacity,
exact 64 KiB and oversize rejection, absent provider, two hosts with equal admission IDs,
independent unload, revoked copied registry and reload/retired IDs. No GPU Preview is claimed.

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

Configured Linux toolchain and pinned ImGui/Vulkan source caches were used. Focused build
completed **84 steps**, six actual material/backend/SDK/signed tests **6/6 in 0.63 s**.
Explicit feature-OFF configure and EditorCore build passed (no remaining build work), with
OFF cache and no optional module/test targets or CTest registration. Existing output files
are not represented as enabled-target acceptance. ON was restored before the full gate.
Full graphical Development rebuilt **300 remaining steps**, **246/246 in 593.47 s**,
zero skips. Minimal Monolithic Shipping passed **5 steps**, and its cache confirms the
material tool is forced OFF with Editor absent; this is not graphical Shipping execution.
Full native Inspector, project upgrade, scene tabs/preview, Game View and actual Showcase/
ProjectPlayer tests pass with Xvfb/software Vulkan. Physical-display and other-platform
execution are not attributed to this Linux gate. Module graph validates **25 modules**,
plugin manifests **2**. Root README remains unchanged.

Final documentation regressions passed **16/16 in 0.768 s**; changed-document links,
five touched C++ formatting and diff whitespace checks pass.
