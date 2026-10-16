# Project-scoped cooked 2D Play assets — Linux, 2026-10-16

Beads: `nexora-zih.4`, supporting `nexora-zih`. Base: `5201d9f6` (main after PR612).

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

Final local validation:

- `cmake --preset linux-development` and `cmake --build --preset linux-development`: passed.
- `ctest --preset linux-development`: **332/332 passed, zero skips, 1065.07 seconds**.
  New `twod.project_play_assets` passed in 0.01 seconds; focused 1/1 in 0.02 seconds.
- `cmake --preset linux-shipping` / `cmake --build --preset linux-shipping`: passed.
  Minimal Monolithic excludes Editor/testing and does not exercise the new capture class.
- `cmake -S . -B /tmp/nexora-editor-20261016/2d-off` reconfigured the existing task-local
  graphical, 2D-OFF, Zig-gameplay-OFF SDK configuration. Actual `NexoraEditorCore`,
  `NexoraMaterialImportTests` and `NexoraWorkspaceBudgetTests` built successfully.
  `ctest --test-dir /tmp/nexora-editor-20261016/2d-off -R '^editor\.(material_import|workspace_budget)$'
  --output-on-failure` passed 2/2 in 2.49 seconds. The new source/test are absent from the build;
  readelf confirms no native TwoD dependency in the Editor library.
- `python3 Tools/Build/TestDocumentationCI.py`: 16/16; seven changed Markdown files validated.
  Three C++ files formatted and both new translation units pass Clang syntax with
  `-Werror=shorten-64-to-32`. Root README, optional module graph and local SDK settings unchanged.
- Five final source/build SHA-256 hashes match the full gate and are retained in the paired JSON.

The existing native Play soak passed 32 cycles/128 lifecycle records with source bytes/pixels
unchanged and zero validation errors. Warmed RSS peak growth 4825088 bytes stays below its
33554432-byte guard. This is compatibility evidence for the existing fixture, not proof of a new
native main Editor Play binding or general leak freedom. Exact-head hosted outcomes, merge and
branch cleanup are recorded in the PR.

The first focused build caught an invalid combined declaration of CTAD arrays with different
lengths in the new test; declarations are separated. No production behavior changed for that fix.
The first focused execution then caught extension-based Runtime importer dispatch after a real
Content rename to `.asset`. The capture adapter now supplies a logical filename from the already
validated sidecar importer; the physical source path remains separately validated and recorded.
The real rename/Undo fixture remains as regression coverage.

Limits only lower the existing 4096 assets / 64 MiB source lengths and separate 64 MiB cooked
payload lengths. Exact sidecar baselines are bounded to 4096 bytes each. Decoded data, temporary
cooking copies, vector capacity and allocator overhead are separate from these logical limits;
no whole-process RSS/VRAM bound is claimed. Serialized owner calls and filesystem observation
checks are not a transaction against hostile concurrent filesystem replacement.

Main Editor Play/Game binding, integrated preview/reload and physical acceptance remain open.
No ED milestone or 2D 9/11 / P0 6/8 progress count changes. No native Windows device acceptance
is inferred from Linux or hosted compile/test results. Minimal Shipping excludes Editor/testing.
