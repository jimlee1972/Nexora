# ED-M1: Bounded asynchronous mesh thumbnail prerequisite

Date: 2026-10-21 (Asia/Taipei). Base: `b8c31d72437a77cc190ad49eb70cf97b6c23fac6`.
Task: `nexora-gu7.1.1`; parent `nexora-gu7.1` remains open.

## Delivered behavior

✅ The actual imported OBJ mesh generates an owning opaque 64×64 RGBA8 image through
`BuildMeshThumbnail`. Fixed isometric framing derives from referenced positions; a depth buffer
and two-sided flat neutral shading describe the shape. Materials, texture sampling, GPU/native
thumbnail capture and Content-row rendering are separate work. There is no icon substitution,
triangle truncation, source write or renderer/backend change.

Actual triangle output from `NexoraMeshThumbnailTests` (PNG encoding of its PPM pixels):

![Actual CPU mesh shape preview](EditorEDM1-MeshThumbnail-Linux-2026-10-21.png)

The test independently checks the z-normal face's center RGBA `(119,132,152,255)`, background,
opaque alpha, covered-sample count and distinct quad coverage. A front face with changed normal
projects onto the same center with different shading; near/far order in the index payload does
not change that center's nearest color. Reverse winding and corrupted cached bounds preserve the
original image. Tiny/large finite positions remain valid. Invalid indices, nonfinite positions,
incomplete triangles, degenerate/empty geometry and over-budget inputs reject. Exact maximum
vertex/index payloads rasterize with the only visible triangle last, proving the whole admitted
index range is visited; cancellation during raster scanlines returns no partial image.

✅ `MeshThumbnailCache` submits a bounded owning geometry copy to the existing Core JobSystem
at Low priority. Workers borrow no Content/document/owner/GPU pointers. Completed workers expose
AwaitingPublish with no image. Poll checks the current nonzero project generation, UUID, exact
artifact revision and immutable geometry owner before publishing; Get rechecks ready entries.
Cancellation wins even after rendering completed. Superseding requests and explicit Clear use
fresh monotonic operation IDs; only the currently retained operation may publish into its entry.

✅ Cache limits admit 1..64 entries and 1..4 pending jobs, with lower configured limits tested too.
Cancelled/superseded work still occupies pending capacity until collection. Busy rejection preserves
the current entry. LRU request/read touches choose eviction; failed/cancelled same-key requests
deduplicate and require explicit invalidation before retry. Ready entries drop captured geometry;
published images are 16 KiB each (maximum 1 MiB in cache). Pending results, bounded geometry copies,
worker scratch, metadata/allocator capacity and externally retained images are separate accounting.
These are logical payload/work limits, not RSS or native VRAM guarantees.

## Lifecycle and regression evidence

The real single-worker pool is held behind a promise/future gate for deterministic queued cases.
Tests exercise revision replacement while blocked, a full pending budget, deleted sources,
generation replacement, explicit same-generation project reopen, stale lookups before collection,
visible worker failure/cancellation without automatic retry, stable-ID rename, hash-only revision
changes, same-hash geometry-owner replacement, stale-source Undo/rescheduling, LRU eviction,
retained image lifetime, cancellation/drain of uncollected queued work, invalid budgets and failed
submission against a stopped scheduler.

The first targeted compile caught an incomplete test-gate JobDescriptor initializer under Werror;
all descriptor fields were supplied before the passing run. Final targeted command:

```bash
cmake --build --preset linux-development --target NexoraMeshThumbnailTests
build/linux-development/Tests/Editor/NexoraMeshThumbnailTests /workspace/work/cloud-backlog/mesh-thumbnail.ppm
```

Result: `Mesh thumbnail pixels, ownership, cancellation, revision/project guards and bounds passed`.
The same production code/fixture is registered as `editor.mesh_thumbnails` with a 30-second timeout.
No deadline, assertion, existing test or platform gate was weakened.

## Full local gates and original timing failure

✅ `cmake --preset linux-development`, `cmake --build --preset linux-development` and
final portability-corrected `ctest --preset linux-development`: **195 passed, zero failures, 11
native Vulkan skips** (206 registered, 156.27 s). The new thumbnail test passes in 0.10 s.
The unavailable native display/GPU skips are not acceptance evidence. Formatting, changed
Markdown links and all 16 documentation-routing tests pass.

✅ `cmake --preset linux-shipping` and `cmake --build --preset linux-shipping` pass
with the existing Minimal/Monolithic configuration and Editor/testing disabled (41 build actions).
No new dependency, preset, deadline or platform policy is introduced. Final fixture output exactly
matches the committed PNG's RGB samples; that image is CPU output, not a graphical UI capture.

The final-source full gate first failed one existing test: `runtime.render_sync`, with
`rejecting a large cycle must stay linear`. Its 20,000-node cycle assertion retains a strict
**two-second** limit; the whole failed test took 11.21 s. Both Runtime source and test are
unchanged, and its executable does not link EditorCore. Earlier complete gates passed that
same test in 0.68/0.71 s. One bounded same-binary diagnostic passed all assertions in
0.680384 s wall / 0.670188 s user / 0.008659 s system. The subsequent unchanged whole-suite gate
passed in 122.70 s with RenderSync in 0.67 s, retaining its original assertion. After the fixture
portability correction below, the final full gate passes RenderSync in 0.90 s. This is not root-cause
remediation. `nexora-gu7.38.3` stays open for timing/scheduling versus algorithm-work diagnosis.
[Original failure excerpt and full-log fingerprint plus measured diagnostic](EditorEDM1-MeshThumbnail-RenderSync-2026-10-21.txt).

### Hosted Windows fixture portability correction

The first Windows mimalloc job (`118791061479`, PR head `559ecd3514b27cfe974bcc6eadd49b87fb35792d`)
failed compilation under MSVC Werror: `std::fill` inferred an `int` from literal zero and the STL
reported C4244 when assigning into `uint16_t` indices. The corrected fixture supplies
`std::uint16_t{0}` explicitly, and uses an explicitly float cached-bound fixture value. No dataset,
expected image, assertion or deadline changes. The [original compiler excerpt](EditorEDM1-MeshThumbnail-WindowsCompile-2026-10-21.txt)
is retained; the final full Linux gate above validates the corrected source.

The exact thumbnail feature branch now has an independent existing-CI cleanup group. It only
cancels its own obsolete owner-actor runs after checking repository identity and rechecking the
branch head before each cancellation. Both live-head push/PR runs remain. All 42 cleanup tests
pass, including contributor/fork/similarly-named exclusions and advancement races. This avoids
retaining superseded work without broadening cancellation to another roadmap session.

### SHA-256 evidence

Full logs remain in `/workspace/work/cloud-backlog`; fingerprints distinguish successful gates
from the retained failure. Source fingerprints below bind the actual producer and fixture.

- `thumbnail-configure-verified.log`: `85312d35b1c3e13c42b8162ff0c489a70d6a8cb1ceede1e6cfad16c37b53df1d`
- `thumbnail-build-verified.log`: `37a984626561198e3a3762488e4e0fdcf529c804492b8be50eceb8974fef4a1b`
- `thumbnail-ctest-verified.log`: `dcb9f155912fe820bd79c923b57b09b18e3f3872cb718ccdd266e53be8ebc04b`
- `thumbnail-ctest-quiescent.log`: `a868a46ad080c8d8bae38f16cc8d1bf5465813cd89426b6b566cbd1cc97c0550`
- `thumbnail-shipping-configure.log`: `bbfaae356fca0ab3b0d1af0051ffa00a32a3b5034e0fe167f4af9181e45f8e1d`
- `thumbnail-shipping-build.log`: `b8c6e600083b809555aa8a09c2dbd259a26a56f90d51a8df7dff13e635d9d4cd`
- `thumbnail-documentation-tests.log`: `116a57c992aa316a6b8a1474ab8113d7ff73029b17c7817de4b66b2a08f2030e`
- `thumbnail-fixture.log`: `8a8a84776d30498a61e590bea0b120a339b49c7bfdf9404adc4b2b80c5cdbb7d`
- `Engine/Editor/include/Nexora/Editor/MeshThumbnail.h`: `244bc26542dd2c5d4c34ecded675fce180fada2ed5f6c11dc3ee61e4e079ce8f`
- `Engine/Editor/src/MeshThumbnail.cpp`: `f9e8a4841aebeb86dcb7a621f6f76759b7fe0f6b73c0ca7b7301304900677a3d`
- `Tests/Editor/MeshThumbnailTests.cpp`: `b92e61fbab1d4cbda5a9559b7d88ed20dde70018c9064c937bb114bcd8879918`
- `Tools/Build/evidence/EditorEDM1-MeshThumbnail-Linux-2026-10-21.png`: `43db19f4d7f8b2a4981c6113c95bbac7866e856472a8c2eb5255510b5df57a93`

- `thumbnail-configure-portable.log`: `a91b995247c546c2f6d30fd2f40e882094ade0846b2dab20b1cd2ac70225fab8`
- `thumbnail-build-portable.log`: `560977e9f1ea9297f4e257d1f1bb248e6706e175b4a29d54f97efd688462a837`
- `thumbnail-ctest-portable.log`: `e62a176256290411ade1c0359c0830d2ee5e7a9e33fc749237c21be2018d1a9c`
- `thumbnail-ci-cleanup-tests.log`: `7bdbd889974268a02ae09054a9338c37bc81b17c01e3feafbc46ef13d73f99a2`
- `Tools/Build/editor-ci-cleanup.cjs`: `6714ee7a3999b2d84ef781f892db96ddff589d4b05006e1f503066f30eae5f0f`
- `Tools/Build/tests/editor-ci-cleanup.test.cjs`: `807f4d7564ba5742c2d179ef38c2e13eb82934cd24a7c267cffd49bda6e65994`

## Acceptance boundary

The [ownership/budget contract](../../../Engine/Editor/README.md#bounded-asynchronous-mesh-thumbnail-snapshots)
requires serialized non-worker owner calls, Content and the running pool to outlive the owner,
Clear on project replacement (including generation reuse), and cancellation/drain before shutdown.
Returned immutable images can outlive cache shutdown. This supports future UI rendering and grants
no source mutation or project access authority.

Graphical Content image uploads/draws, other asset types, representative production-project UI and
performance/DPI, physical devices and full M1-01/ED-M1 acceptance are unverified here. Editor stays
0/8 accepted. Root README remains unchanged because overall milestones do not advance.
