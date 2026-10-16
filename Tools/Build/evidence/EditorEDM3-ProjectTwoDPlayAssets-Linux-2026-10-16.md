# Project-scoped cooked 2D Play assets — Linux, 2026-10-16

Beads: `nexora-zih.4`, supporting `nexora-zih`. Base: `5201d9f6e02d2a24406004174a50f61ad9cebf9b` (main after PR612).

`ProjectTwoDPlayAssets` connects current project Content to Runtime import/cook/bundle/store
and the independent `TwoDPlayAssets` immutable closure. Explicit host authorization and current
writer/project/Content scope precede IO. Exact source bytes, authoritative sidecars, immutable
payloads, artifacts and dependency graph agree before capture. Current disk project identity is
checked separately from the in-memory descriptor. Recovery, pending import/conflict, foreign root,
source drift and unsafe aliases reject. Matches repeats these checks before activation.

The result owns provenance and exact file baselines. A private in-memory cook store never leaks
pins or borrows. Old Atlas/Clip owners survive reimport, Content and workspace destruction and
serve the canonical GameplayBridge. Invalid regions and unsupported dependency types reject
through existing cooked closure admission. Capture changes no source, metadata, authoring history,
World, shared Runtime store or GPU state.

Final corrected-source validation:

- `cmake --preset linux-development` / `cmake --build --preset linux-development`: passed.
- `ctest --preset linux-development`: **331/332 passed, zero skips, 1059.22 seconds**.
  Existing `editor.linux_native_scene_center` failed because a center gesture did not Undo to the
  expected saved baseline. The full output is preserved; no source/test settings were changed.
  Same-source `ctest --preset linux-development -R '^editor\.linux_native_scene_center$'
  --output-on-failure` passed 1/1 in 22.56 seconds. All 332 distinct cases have passed, but this is
  not a clean-first-full-run claim or a root-cause/fix claim. Final hosted checks must also pass.
- New `twod.project_play_assets` passed in 0.01 seconds, including the corrected Compare policy.
  An unresolved out-of-closure conflict rejects both Capture and Matches; Keep restores admission.
- `cmake --preset linux-shipping` / `cmake --build --preset linux-shipping`: passed.
  Minimal Monolithic excludes Editor/testing and does not exercise the new capture class.
- `cmake -S . -B /tmp/nexora-editor-20261016/2d-off` reconfigured the existing task-local
  graphical, 2D-OFF, Zig-gameplay-OFF SDK configuration. Actual `NexoraEditorCore`,
  `NexoraMaterialImportTests` and `NexoraWorkspaceBudgetTests` built successfully.
  `ctest --test-dir /tmp/nexora-editor-20261016/2d-off -R '^editor\.(material_import|workspace_budget)$'
  --output-on-failure` passed 2/2 in 2.49 seconds. The new source/test are absent from the build;
  readelf confirms no native TwoD dependency in the Editor library. The Compare fix only changes
  the excluded optional implementation/test, so Shipping/OFF retain these scoped results.
- `python3 Tools/Build/TestDocumentationCI.py`: 16/16; seven changed Markdown files validated.
  Three C++ files formatted and both corrected translation units pass Clang syntax with
  `-Werror=shorten-64-to-32`. Root README, optional module graph and local SDK settings unchanged.
- Five corrected source/build SHA-256 hashes match the final gate and are retained in paired JSON.

The corrected-source existing native Play soak passed 32 cycles/128 lifecycle records with source
bytes/pixels unchanged and zero validation errors. Warmed RSS peak growth 1523712 bytes stays
below its 33554432-byte guard. This is existing fixture compatibility evidence, not a new native
main Editor Play binding or general leak freedom.

Provenance: initial test compilation caught combined CTAD declarations of different-sized arrays;
these are separated. Initial focused execution then caught extension-based Runtime importer
dispatch after real Content rename to `.asset`. The adapter now supplies a logical filename from
the validated sidecar importer while retaining the separately checked physical IO path. Focused
1/1 then passed in 0.02 seconds. Initial head `10f870c19a46d81394e217c1d7d49d87361a0794`
passed a full 332/332, zero skips, 1065.07 seconds, but review found that Compare remains unresolved
under the existing graphical ActionableConflict contract. Scope admission now rejects Pending and
Compare. The earlier head, hashes, test/soak data and review are retained in JSON as provenance;
initial-head hosted results are not merge evidence. See [review](https://github.com/jimlee1972/Nexora/pull/613#discussion_r4292994932).
Exact corrected-head hosted outcomes, merge and branch cleanup are recorded in PR613.

Limits only lower the existing 4096 assets / 64 MiB source lengths and separate 64 MiB cooked
payload lengths. Exact sidecar baselines are bounded to 4096 bytes each. Decoded data, temporary
cooking copies, vector capacity and allocator overhead are outside these logical bounds;
no whole-process RSS/VRAM bound is claimed. Serialized owner calls and filesystem observation
checks are not a transaction against hostile concurrent filesystem replacement.

Main Editor Play/Game binding, integrated preview/reload and physical acceptance remain open.
No ED milestone or 2D 9/11 / P0 6/8 progress count changes. No native Windows device acceptance
is inferred from Linux or hosted compile/test results. Minimal Shipping excludes Editor/testing.
