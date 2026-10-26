# Nexora Project Player

`--verify-game-package PATH` verifies the separate bounded multi-scene scene-catalog format,
reporting the configured startup UUID, scene/asset counts and native_rendering=false /
gameplay_loaded=false. It needs no authored `.scene` or source tree. This is CPU data verification;
native execution is a separate mode below; built games use executable-relative Game.launch metadata.
See [the catalog contract](../../Engine/Runtime/GameProjectPackage.md).

## Native packaged game candidate

With native Player support enabled, `--run-game-package PATH` uses the verified catalog's explicit
startup UUID and the same native options as `--run-package`. An optional explicit
`--gameplay-module=PATH` attaches app-scoped trusted code after the first startup frame presents.
The module may negotiate the scene service and queue Replace/Additive/unload. It stays attached
across transitions. The first presented frame initializes the game; subsequent presented frames
run gameplay callbacks, so a bounded N-frame run reports N-1 updates when a module is attached.

The host processes requests after callbacks and GameWorld EndFrame. It captures remapped live
render items, reuses the StaticView geometry/material admission authority, validates the candidate
camera and records the candidate native draw before no-throw World/palette publication. CPU upload
spans are borrowed only for DrawScene; the surface owns copied, fence-protected upload/residency
resources through retirement/drain. Geometry residency is requested only when supported and its
copied counters are reported independently from CPU asset ownership. Resource preparation or
camera admission failure before native submission leaves the previous scene active and rendering;
the module can inspect Failed and retry. Native Unsupported/InvalidDescriptor admission rejection
also attempts the retained view in the same frame; it continues only if that draw succeeds. Actual
backend/device failure terminates with a diagnostic,
retaining the old World until teardown rather than claiming recovery from a lost device.

The primary scene owns the lowest-ID authored camera/light. Additive scenes retain that primary.
A camera-less game scene uses a separate host default camera at (0,0,5), facing -Z with default
CameraComponent projection; the live/authored World is not modified. Empty game scenes present
the acquired surface's clear background. Existing StaticView still requires a camera and nonempty
native mesh data. Legacy opaque-component and CLI behavior remain independently guarded.

`RENDERED_GAME_PACKAGE` reports startup/active UUIDs, camera, actual frames/draws, script updates,
scene commits/rejections, retained scene/entity counts, geometry residency counters, software-device
observation and successful drain. Native allocation bytes are backend allocation requirements,
not a physical VRAM measurement. A successful report requires successful native presentation,
drain, stdout flush and exit 0. Stop cancels pending transitions before module Stop/Destroy; the
borrowed session/services remain alive until module teardown finishes.

This is a native integration candidate. Linux real-pixel sequence, full exact-head CI, Editor
Play/common build, game relocation and final Editor ZIP remain independent acceptance gates in
the [delivery plan](../../Roadmap/en/Project_Scene_Delivery_Plan.md). Windows GUI capture also
remains pending when Computer Use app access has not been granted.

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
Failures do not print a complete success report. Verification explicitly flushes stdout and checks
its stream state before returning 0; a failed report write returns 1 with a stderr diagnostic.
Consumers must require both a successful exit and a complete valid report.
The Windows console entry uses CRT `wmain` and native UTF-16 filesystem paths; POSIX `main`
interprets path arguments as UTF-8. The test fixture writer uses the same platform path rules.
MSVC/clang-cl support the wide console entry directly; MinGW uses its `-municode` link option,
without adding Shell32 or a command-line reparser. Linux path/CLI tests have run; Windows and
macOS execution have not been performed in this cloud environment.

Unknown opaque records remain present but inactive. Scalar material bindings override StaticView
appearance explicitly while preserving full legacy shader IDs; an unimplemented nonzero legacy
shader without such a binding rejects. No project source directory is required to verify an
already-produced package. Verification does not load gameplay. The player does not compile/export
an Editor project or deploy an application. Verification itself opens no native window. Optional native StaticView
rendering and explicit gameplay-module attachment are described below; build/deploy workflows remain open.

See [the Runtime package contract](../../Engine/Runtime/ProjectPackage.md) for the wire, limits,
resource collision policy and ownership. Linux cloud CLI acceptance covers real cooked artifacts,
corruption/truncation/missing-file and alias failures; physical display and other platform runtime
acceptance are separate.

## Editor export CLI output and failure diagnostics

The Editor fixture-to-player acceptance test retains escaped, bounded stdout and stderr alongside
its stage and exit code when fixture generation/verification fails or a zero-exit verification
returns malformed JSON. Each stream is limited to 2,048 characters plus its original length;
control characters cannot create additional log lines. Only this test's generated package is
consumed. Reports are still parsed as complete JSON, all package/schema/Unicode/determinism
assertions remain mandatory, and no child is automatically retried. Normal and optimized Python
regressions exercise successful reports, empty/non-JSON zero exits, nonzero valid-JSON exits and
bounded control-character output. Both complete fixture workflows require failed writes through
a real read-only stdout handle to return 1 with unchanged sink/package bytes. This supplies failure evidence, not a repair of the intermittent
hosted Windows failure. Successful JSON and package ownership stay unchanged; output write
failure now returns 1 explicitly instead of allowing a zero exit with no report.

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
It has no Editor, SDK, source directory or Showcase dependency. Its CPU candidate owns
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
neutral material; absent-plugin opaque components remain inactive and are counted. Without an explicit module option it runs no plugin
or gameplay code. It writes no package/source data. Native window acquire/draw/present and teardown
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

## Explicit gameplay module (Zig V3 ABI)

✅ Native runs can explicitly attach a separately built Zig gameplay dynamic library:

```sh
NexoraProjectPlayer --run-package project.nxproject --gameplay-module=/absolute/path/to/game.so
NexoraProjectPlayer --run-package project.nxproject --gameplay-module=/absolute/path/to/game.so --save-directory=/absolute/path/to/saves
```

The module path is trusted executable code selected by the caller. Package verification never
loads modules, and neither packages nor their opaque components trigger module discovery.
`--gameplay-module` and `--save-directory` are native-run options; duplicate/empty options reject.
A save directory requires an explicit module. No save directory or save file is invented by default.
Storage slots use the package project UUID as their scope; the caller owns the directory and must
provide a dedicated directory per project and avoid simultaneous writers to it.

The player clones the verified scene into an owning Play `GameWorld`, preserving authored entity
IDs. The V3 host supplies copied component access, logical entity spawn/despawn, scene creation and
activation, allocation, native input snapshots and diagnostics. The legacy V3 snapshot maps held WASD to
movement axes and Space to button bit zero; focus loss clears held controls and reserved stays zero. Authored mesh transforms, camera and
light poses are refreshed from this World before drawing. Invalid or destroyed admitted render
objects fail the run visibly. The immutable StaticView mesh/material palette does not admit new
rendered entities; rendered/physics spawn flags, mesh-resource replacement, asset resolution,
raycast, debug drawing, 2D and UI return unsupported or have no advertised extension. This is a
module launch path for the existing static package renderer, not a complete 2D project exporter.

The optional services extension exposes rebindable input actions from native key/mouse/focus events
and the Linux gamepad adapter. The input service also exposes native pointer coordinates,
per-frame motion, mouse buttons and horizontal/vertical wheel values through `read_pointer`.
Repeated reads preserve the frame snapshot; focus loss clears held buttons and motion. Audio control uses the real output service; unavailable devices are
reported and playback returns unsupported. This package format contains no audio assets, so no
sound content is auto-registered. UI is absent. Persistent storage is exposed only with the explicit
save directory and stays separate from hot-reload `save_state`.

Callbacks run serially on the native owner thread: fixed updates at 60 Hz with a 250 ms frame clamp,
then update and draw. Suspended acquisition does not invoke updates; elapsed time is clamped when
rendering resumes. Callback/load failures produce no success JSON. Stop/destroy/unload complete
before native surface teardown and before the copied World and services are released, including
exception unwinding. Successful reports include actual gameplay update/fixed-update counts.

Acceptance: the Linux native test loads a real fixture dynamic library, exercises logical spawn,
component read/write, authored-pose changes, input/services negotiation, explicit storage roundtrip,
fixed/update callbacks, despawn, stop/destroy and real presentation. It also rejects missing/nonmodule
libraries, verifier module options and duplicate/empty CLI options, and checks unchanged package bytes.
The fixture follows the same V3 C ABI consumed by Zig; this does not claim additional 2D assets or
other-platform execution acceptance.

## Independent 2D game projects

With native support and `NEXORA_ENABLE_2D=ON`, this executable also accepts
`--run-2d-project game.nx2game` and delegates to the common engine-owned 2D Player host. That explicit
mode loads the selected trusted V3 library, project-owned cooked assets, UI/audio/input and durable
slots. It does not reinterpret StaticView packages or change verification-only behavior. See the
[2D Player guide](../TwoDPlayer/README.md) for export, Windows/MSVC CRT staging, font baking, CLI
options, limits and remaining physical acceptance.
