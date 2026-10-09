# ED-M3 frozen scalar Game materials — Linux cloud evidence

Scope: `nexora-rd7.1.1`, an independent supporting slice of `nexora-rd7.1`. This accepts
Start-time owning scalar PBR values/mesh-entity UUID assignments and the application's native
Game draw path. Full ED-M3, texture/shader graphs, dynamic material-reference authoring and
simultaneous native Scene/Game canvases remain open. No stable C/Zig wire or Runtime material
shader ID is changed.

## Ownership and fallback

Before Play clone creation, the application validates catalog generation and converts the
document's mesh material references into an owning palette. Slot zero is neutral; at most 63
distinct resolved UUIDs are admitted. Missing/budget-rejected references remain authored and
visible admitted meshes report unavailable material counts. Unsupported opaque versions remain
unresolved. The snapshot never borrows a catalog, document, World, file or GPU resource.

After fixed ticks, Game frame preparation revalidates the live isolated World, copies selected
camera world position and exact affine instance matrices, and generates Renderer tangents. Native
PBR data is owned through DrawScene; existing Presentation protecting-frame fences retain
submitted storage. Tangent failure keeps geometry with all batch slots reset to zero on the legacy
Lambert path. No resolved authored material also uses Lambert. Runtime-created entities receive
neutral shading. Stop/apply-and-stop releases the snapshot; new Start captures current authoring.
Prepared frames retain values after Stop and catalog/palette destruction.

## Verification

The portable `editor.game_material_preview` test performs actual catalog reimport, source deletion
and document reassignment after freezing; frozen red values/assignments remain independent of
fresh green/missing snapshots. It checks every supported scalar, full-width legacy shader ID,
deduplication, 64-slot budget, mesh-only assignment admission, neutral/missing fallback, degenerate
UV tangents, tangent conversion failure, camera position, Runtime-created entities, isolated
Pause/Step/Stop and frame ownership after teardown.

`editor.linux_native_game_materials` launches the actual graphical Editor under Xvfb/lavapipe with
schema-3 opaque UUID references and an OBJ. It acknowledges more than 20 red PBR pixels (red >150
and >2×green/blue), pauses, changes the source to green, and verifies the paused Game image remains
stable. It steps once and stops with unchanged scene bytes. A read-only reopen acknowledges more
than 20 green pixels from the new source and preserves source/scene bytes. Disk editing is not
assumed to imply watcher publication: actual reimport/delete/reassignment are exercised separately
by the portable fixture. Existing native Game view/gameplay/input tests continue to pass.

Commands use the installed GCC 14.2, CMake 3.31.6, Slang, Zig, Mesa software Vulkan, Xvfb and xdotool:

```bash
source /workspace/.nexora/env.sh
cmake --preset linux-development -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON \
  -DNEXORA_ENABLE_SLANG=ON -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON -DNEXORA_BUILD_SHOWCASE=ON
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development
ctest --preset linux-development -R 'editor\.(game_material_preview|game_view_preview|linux_native_game.*|material_catalog)' --output-on-failure
cmake --preset linux-shipping
cmake --build --preset linux-shipping
git diff --check
```

Source base: accepted native GPU PR #452, main `7bc2ba2fe5e5c2a7bf52dd956214c96e85246e75`.
The initial focused gate passed **7/7**, no skips, in **14.17 seconds**. The final required
configure/build/serial test gate passed **188/188**, no skips, in **358.33 seconds**, with graphical
shell, Slang, Zig and Showcase enabled. Minimal Monolithic Shipping configure/build also passed
with Editor disabled and native Presentation enabled. `git diff --check` passed.

An earlier simultaneous run of two full graphical gates passed 187/188, with one Scene-center Undo
fixture failure. The unchanged fixture passed isolated reproduction **1/1** in **23.17 seconds**,
then the full gate passed serially with unchanged implementation and deadlines. Future native
gates are serialized on this four-CPU cloud host. An initial baseline graphical configuration
without optional Zig/Showcase also passed 159/159 in 240.30 seconds; final acceptance uses the
expanded 188-test gate above.
Windows/macOS native fixtures and physical GPU/display/DPI/IME acceptance were not run on this
Linux cloud host; hosted compile checks and physical acceptance are separate evidence.
