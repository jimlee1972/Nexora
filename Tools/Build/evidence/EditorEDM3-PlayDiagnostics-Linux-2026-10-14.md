# ED-M3 copied Play diagnostics — 2026-10-14

Beads: `nexora-rd7.2.6`. Initial base: main `015bc87a`; prepared in an isolated worktree while
PR592 ran CI. Integrated merged PR592/main `d6ea19a8` with unchanged diagnostic source hashes;
final complete local validation passes before publication.
[Raw validation scope and source hashes](EditorEDM3-PlayDiagnostics-Linux-2026-10-14.json).

## Behavior and contract

The existing V3 ABI declares `get_diagnostics`, but the native Editor previously left it null.
The Play host now returns owning scalar diagnostics for its isolated Play clone. No stable C wire,
capability bits, project/scene format or authoring permission changes. A new additive Runtime C++
query sums entity-vector sizes over active scene records in O(scene count), without allocations,
entity/transform traversal, cached snapshots or returned borrows.

`frame` counts admitted finite/nonnegative module Update invocations, advancing before each module
callback even if it reports failure. Create/Start observe zero; fixed ticks/Step, invalid deltas and
absent modules do not advance it. It is not a render/presentation frame, fixed-tick count or timestamp.
`scene_entities` includes only current active scenes; activation, spawn/despawn and unloading are
visible immediately. `debug_lines` is zero because debug drawing stays unavailable. `api_errors`
counts non-OK integer-result host service callbacks, including rejected diagnostic requests. It
excludes void log/deallocate, pointer-result allocation, module callback failures and external/native
errors; existing error channels are retained.

Both counters saturate at UINT64_MAX and reset per binding. State remains available during
Create/Start/Stop/Destroy and clears after module destruction, before clone teardown. Null output
returns InvalidArgument; unbound requests return Lifecycle without changing output bytes. Calls are
synchronous on the serialized owner thread, never recursively query the module host mutex, and
retain no entity/native borrow. Copied callback pointers must not outlive the embedding object.
Asset resolution/debug drawing and native debugger/IDE remain separate work. Matching C++ consumers
use the updated Runtime symbol; no World/public host layout or C ABI change is introduced.

## Independent regression and native evidence

The new initial regression fails the old null-callback host with `diagnostics module load failed`.
The candidate passes 32 repeated isolated module lifecycles covering current active/inactive and
unloading/unloaded scenes, load/activate/spawn/despawn, owning copies, preserved invalid-request
outputs, scoped API errors, optional FixedUpdate, invalid Update deltas, failure, unload/reload,
Create/Start/Stop/Destroy availability and unchanged authored scene bytes. A private application
fixture seeds near-limit counters; real callbacks reach UINT64_MAX and repeated increments saturate.

A controlled fault forcing only the published entity count to zero fails with `Create diagnostics
not reset or active-only`. Correct source is restored byte-for-byte, rebuilt, and all four Play
regressions pass (0.11s). No assertions/deadlines are weakened.

The actual dynamically loaded gameplay fixtures require diagnostics during Start and every Update,
checking the expected frame sequence, nonempty active Play entities and zero unavailable debug lines.
They emit a diagnostic marker which the Linux native Game workflow requires. Real Xvfb/lavapipe
Game View gameplay, scene, physics and input cases all pass (4/4,18.47s), preserving existing source,
Console, pause/step/stop and pixel checks. These are Linux cloud runs, not physical-host acceptance.

## Final gates

- `cmake --preset linux-development`: pass on merged main `d6ea19a8` plus this change.
- `cmake --build --preset linux-development`: pass,447 integration rebuild actions.
- `ctest --preset linux-development --output-on-failure`:256/256,812.15s,no skips.
  This includes the mandatory native diagnostic callback checks and the merged hierarchy storage fix.
- `cmake --preset linux-shipping` and `cmake --build --preset linux-shipping`: pass,
  14 integration rebuild actions after the initial74-action build. Minimal/Monolithic with
  Editor/ImGui/SDK off; this does not establish a Shipping graphical Editor.
- `ctest --preset linux-development -R '^editor.play_(diagnostics|gameplay_module|scene_services|physics_services)$' --output-on-failure`:
  4/4,0.11s after controlled fault restoration.
- `ctest --preset linux-development -R '^editor.linux_native_(gameplay_module|play_scene_services|play_physics_services|game_input)$' --output-on-failure`:
  4/4,18.47s after restoration; all four also pass in the final complete gate.
- `python3 Tools/Build/TestDocumentationCI.py`:16/16. Six touched C++ files pass
  `clang-format --dry-run --Werror`; eight source/config hashes, new evidence links, synchronized
  roadmap entries, unchanged root README and `git diff --check` pass.

Final-head hosted CI will run on publication. Full ED-M3 and physical0/8 acceptance remain open.
