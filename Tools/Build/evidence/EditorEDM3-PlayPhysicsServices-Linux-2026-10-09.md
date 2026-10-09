# Editor ED-M3 bounded Play CPU collider queries — Linux cloud acceptance

Date: 2026-10-09. Scope: `nexora-rd7.2.2`, on accepted main `9b599c5d` with bounded Play
scene/entity services. No rigid-body/backend stepping or complete gameplay milestone claim.

## Actual implementation and evidence

With gameplay simulation enabled, the real Play V3 host admits Physics spawn descriptors into a
fixed owning 256-entry local-AABB/entity/scene array. Finite ordered bounds and translated root
corners are validated before World entity creation. Every query prepares a fresh real CPU
`PhysicsWorld` from eight current exact affine-transformed corners of active-scene colliders.
Returned entity/distance/point values are copied. Equal-distance Runtime hits select the lower
body ID independently of unordered insertion. The C/Zig layout and capability bits do not change.

Real V3 Create/Start invokes scene/Physics callbacks and verifies distance 2 and point Z=1, using
a maximum finite negative direction; ordinary/subnormal rays also resolve. Tests then mutate
transforms, reparent under mirrored/nonuniform scale and add child rotation/nonuniform scale to
produce inherited shear. A ray through the resulting conservative AABB verifies all eight corners
rather than a stale local/root proxy. This remains conservative world-AABB collision, not oriented
shape collision. Invalid/null/nonfinite/zero/miss rays, malformed bounds, publication overflow and
invalid live world matrices preserve the full caller output. Invalid live colliders fail the whole
query; no partial result is returned.

Tests cover exact live 256 quota, deletion/external-command pruning, lifetime 4096 spawn quota
without replenishment after despawn, inactive/active/unloading/unloaded scenes, cascade deletion,
Clear/rebind and real module Stop/Destroy query lifetime. Authored Editor bytes remain unchanged.

The real compiled Physics gameplay fixture runs in the actual native Game View. Its Start uses
the production host to load/activate a scene, spawn a Light/Physics entity, raycast at its world
position, prove copied distance 2 and point Z=4, despawn it, and require a miss with the previous
hit unchanged. The native test checks the actual bounded evidence and real Core/gameplay logs,
plus the existing visible mesh motion, Pause/Step/Stop and unchanged source scene. Fixture failure
rejects gameplay Start; there is no test-only production hook.

## Executed validation

Linux cloud: GCC 14.2, CMake 3.31.6, Ninja, Slang 2026.18, Zig, Xvfb and software Mesa Vulkan.
Heavy builds and complete native gates are serialized.

```bash
cmake --preset linux-development \
  -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON -DNEXORA_ENABLE_SLANG=ON \
  -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON -DNEXORA_BUILD_SHOWCASE=ON
cmake --build --preset linux-development -j 4
ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping -j 4
```

Complete graphical Development passed **199/199, zero skips, 363.42 seconds**. The actual native
Physics fixture passed in 5.19 seconds. Default Minimal Monolithic Shipping passed 74 build steps.
Earlier focused Runtime tie/real V3/Game/material/scene/dynamic-native checks passed **8/8 in
9.97 seconds**; the new static V3 Physics fixture passed 0.08 seconds. Touched C++ clang-format,
native Python syntax and `git diff --check` pass. Existing SceneFiles, recovery, GPU/RSS profiling,
Console, Game material/input, Shader and Showcase gates remain present and pass.

```bash
cmake --preset linux-development -B build/linux-physics-off \
  -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON \
  -DNEXORA_ENABLE_GAMEPLAY_SIMULATION=OFF -DNEXORA_ENABLE_AI_RUNTIME_BRIDGE=OFF
cmake --build build/linux-physics-off \
  --target NexoraPlayPhysicsServicesTests NexoraPlaySceneServicesTests -j 4
ctest --test-dir build/linux-physics-off \
  -R 'editor.play_physics_services|editor.play_scene_services|runtime.v1_m8_feature_strip' \
  --output-on-failure
```

Actual simulation-OFF build passed 85 steps and **3/3 tests in 0.05 seconds**. The real host's
raycast slot is null, Physics spawn and valid direct queries return Unsupported with unchanged
outputs, and the existing feature-strip contract passes. The initial configure that left the
dependent AI bridge enabled was rejected by the existing configuration guard; the final compatible
flags above disable both. No guard was bypassed or relaxed.

## Remaining boundary

This adapter performs synchronous serialized CPU AABB queries only. It does not import authored
collider components, advance dynamics, execute a native rigid-body backend, resolve assets or
implement debug drawing/diagnostics. Stop still discards created/deleted Play entities; transform
Apply Changes excludes them. Expanded input devices and complete ED-M3 acceptance remain open.
Physical GPU/display acceptance and non-Linux local execution are not claimed; hosted CI is
assessed separately before merging.
