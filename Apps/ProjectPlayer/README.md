# Nexora Project Player

Enable `NEXORA_BUILD_PROJECT_PLAYER=ON` together with the asset pipeline to build the Runtime-only
`NexoraProjectPlayer`. The current executable verifies a **StaticView cooked package**:

```sh
NexoraProjectPlayer --verify-package '/path/to/project package.nxproject'
```

It loads the actual World snapshot, resolves all cooked mesh/scalar PBR bindings and validates
bounded dependency/integrity/identity contracts. Success prints JSON with
`status: VERIFIED_STATIC_VIEW`, project/scene UUIDs, entity/asset/resolved-renderer/inactive-component
counts and at most 64 render item summaries. IDs remain decimal strings to preserve every uint64
bit. Up to 64 inactive-component summaries identify entity/type/name and payload size; names are
lossless hexadecimal so preserved non-UTF8 names cannot invalidate JSON. Unreported counts are
explicit. These are diagnostic caps, not loading/validation limits: all bounded scene entities,
bindings and opaque records are validated. Invalid arguments exit 2; invalid data/input exits 1.
Failures do not print a success report.
The Windows console entry uses CRT `wmain` and native UTF-16 filesystem paths; POSIX `main`
interprets path arguments as UTF-8. The test fixture writer uses the same platform path rules.
MSVC/clang-cl support the wide console entry directly; MinGW uses its `-municode` link option,
without adding Shell32 or a command-line reparser. Linux path/CLI tests have run; Windows and
macOS execution have not been performed in this cloud environment.

Unknown opaque records remain present but inactive. Scalar material bindings override StaticView
appearance explicitly while preserving full legacy shader IDs; an unimplemented nonzero legacy
shader without such a binding rejects. No project source directory is required to verify an
already-produced package. The player does not compile/export an Editor project, load gameplay,
deploy an application. Verification itself opens no native window. Optional native StaticView
rendering is described below; gameplay/build/deploy workflows remain open.

See [the Runtime package contract](../../Engine/Runtime/ProjectPackage.md) for the wire, limits,
resource collision policy and ownership. Linux cloud CLI acceptance covers real cooked artifacts,
corruption/truncation/missing-file and alias failures; physical display and other platform runtime
acceptance are separate.

## Optional native StaticView

Enable NEXORA_ENABLE_PROJECT_PLAYER_NATIVE=ON with NEXORA_BUILD_PROJECT_PLAYER=ON, Window
Presentation and scene rendering. The default remains verification-only; disabling native support
also permits a verifier build with Window Presentation and scene rendering disabled.

~~~sh
NexoraProjectPlayer --run-package '/path/to/project package.nxproject' --backend=vulkan
NexoraProjectPlayer --run-package '/path/to/project package.nxproject' --frames=4
~~~

Without a frame limit, the application runs until a real native close request. Optional positive
frame limits are at most 1,000,000 successful presents; a bounded run fails if presentation stalls
for 30 seconds. Backend selection is automatic, vulkan, dx12 or metal; unavailable selections fail
with an actual backend reason. Invalid/duplicate options exit 2. A verification-only executable
reports native rendering unavailable rather than pretending to draw.

The adapter consumes only the public owning Runtime package and public Presentation contracts.
It has no Editor, SDK, source directory, Showcase or gameplay dependency. Its CPU candidate owns
converted geometry, tangents, exact affine instances, scalar PBR materials and diagnostic IDs.
Input storage is borrowed only during preparation. Draw descriptors borrow those immutable vectors
for one serialized owner-thread DrawScene call; the owning loaded World supplies the camera view.
Resize recomputes the projection from the current native aspect ratio.

Native admission is stricter than data verification: 1..4096 instances, at most 65,535 shared vertices
and 1,048,576 shared triangle indices, and 63 distinct scalar materials plus reserved neutral slot
zero. Shared mesh/material objects are deduplicated without discarding full package identity.
Finite float affine conversion, exact invertibility/normal packing, PBR tangent orthogonality and
public material/batch validation must succeed. A conservative aggregate CPU upload estimate is capped
at 8 MiB; backend allocation/alignment failures still return actual failure. No partial scene or
substituted proxy is drawn when admission fails. Package verification keeps its wider data bounds.

The lowest-ID authored camera is selected deterministically and must produce a valid native view;
there is no invented fallback camera. The lowest-ID authored LightComponent supplies neutral-white
intensity in [0,65504] and its scale-independent world orientation. With no light, the documented
Presentation default directional light is used. Runtime currently stores intensity only; this
does not invent authored light colors or evaluate all production lighting types.

StaticView preserves full legacy shader IDs and explicitly uses the resolved scalar override or
neutral material; absent-plugin opaque components remain inactive and are counted. It runs no plugin
or gameplay code and writes no package/source data. Native window acquire/draw/present and teardown
use RenderSurface. Surface/resize recovery remains adapter-owned; device loss is a visible failure.
The GPU surface drains before its window/resources are released, including failed draws.

Success prints RENDERED_STATIC_VIEW JSON only after actual drawn presentation and successful drain.
The report owns camera/light IDs as decimal strings, actual draw/present/resize counts, inactive
component count, software-rasterizer observation and the explicitly requested backend. Requested
automatic selection is not claimed as an observed backend identity. Native failures exit 1 without
a success report. This is static native viewing, not a compiled/deployed application.

CPU tests use real AssetCooker/codec/package data and exercise exact budgets, mirror/shear ownership
and resized projection. Linux Xvfb tests inspect actual red/green scalar material pixels, resize,
WM_DELETE_WINDOW, drain, exactly four successful presents and unchanged package bytes. Hosted
desktop CI compiles this optional path on Windows/macOS/Linux; physical displays, hardware calibration
and other-platform native pixel acceptance remain separate.
