# Project startup scene, multi-scene build and runtime transitions

Status: scoped implementation and delivery accepted; **✅ 5/5 implementation phases accepted**.
Updated: 2026-10-26. Related milestones: ED-M1, ED-M3 and ED-M6; none is accepted by this plan.
The durable work/dependency record is Beads, not the phase table below.

✅ Phase 1 settings/core and graphical Build Settings are accepted for their scoped automated
contract. PR #716 head `6877b09d` passed all 38 exact-head CI checks and was squash merged as
`72c82cc7`. Full hosted Linux Development (454 registered, zero failures, existing font skip),
Shipping, sanitizer/TSan, native display and platform gates passed. Windows local core/1x/2x UI
and startup smoke passed. Windows symlink creation remained unavailable on this developer PC;
the full verbose Linux core gate exercised the alias branch without an unavailable diagnostic.
Native settings-to-game integration and final delivery remain phase 4/5 work.
[Evidence and closure](../../Tools/Build/evidence/EditorProjectBuildSettings-2026-10-25.md).

✅ Phase 2 cooker/package is accepted for its scoped data/producer contract. PR #717 head
`c66cecd0` passed all 38 exact-head checks and was squash merged as `7968bf62`. Hosted Linux
Development registered 457 tests, Windows 397 and macOS 376, all zero failures with the existing
font skip retained. Linux Shipping, sanitizer/TSan, feature-off and native/platform gates passed.
The first macOS temporary-path fixture failure is retained; resolving the physical fixture root
fixed it without changing production alias rejection. See the [package evidence](../../Tools/Build/evidence/EditorGameProjectPackage-2026-10-26.md),
[package contract](../../Engine/Runtime/GameProjectPackage.md) and [producer contract](../../Engine/Editor/GameProjectExport.md).

✅ Phase 3 adds an owner-thread queued Runtime session and optional C/Zig scene service,
with candidate World/presentation preparation, remapped IDs, persistent retention and retired
binding cleanup. Twelve Windows scoped tests pass, including the actual SDK-built Zig fixture
requesting Level01 → Title → Level01. PR #718 head `8726bb7f` passed all 38 exact-head checks and
was squash merged as `b45d880e`. Linux Development registered 461 tests, Windows 401 and macOS
380, with zero failures and the existing font skip retained. Shipping, sanitizer/TSan, feature-off
and native/platform gates passed. See the [session contract](../../Engine/Runtime/GameSceneSession.md) and
[scoped evidence](../../Tools/Build/evidence/GameSceneSession-Windows-2026-10-26.md).

✅ Phases 4 and 5 are accepted for this scoped delivery. PR #721 source `f327bd86`
passed all 41 exact-head checks and squash merged as `e0036e73`. Full hosted Linux Development
registered 469 tests, Windows 407 and macOS 386, with zero failures and the existing font skip.
Shipping, feature-off, sanitizers/TSan and platform gates passed. Linux actual Editor pixels,
graphical script Build and standalone native pixels passed; Windows developer-PC execution,
source-hidden zero-argument games and actual ZIP extraction also passed.

Editor Play uses the saved enabled scenes and common Scene V1 session in an isolated game World.
Dirty authoring offers Save/Use Saved/Cancel; remapped IDs cannot apply back to authoring.
Graphical Build and CLI share the guarded producer and installed runtime/tools. First-configure
compiler discovery and POSIX split-target profile regressions are fixed and covered; original
failed attempts remain in the historical host/build evidence.

The final Windows Editor-only ZIP is `NexoraEditor-Windows-x64-f327bd86260a.zip` (102,785,740 bytes),
SHA-256 `a5e97b52f0fd80801ff0310dcd5ba6cca6aa3e499a07b1208902fcbee5bc3cc7`.
All 18,501 inventory hashes match. Actual Unicode extraction passes zero-argument startup,
normal bounded close and 1,200-frame/three-commit Play with saved sources unchanged. Private
Runtime/Tools resources and licenses are included; no Nexora/game source checkout, examples or
extra 2D applications are included. Keep the whole extracted directory together.

This accepts the native mesh/scalar-PBR scene delivery contract, not all custom 2D/physics/plugin
formats or any complete Editor milestone. Full graphical milestones remain 0/8. Windows captured
pixels and independent clean-host acceptance are not claimed. See the [final acceptance record](../../Tools/Build/evidence/ProjectSceneDelivery-Acceptance-2026-10-26.md)
for commands, counts, original failures, artifact/exit/hash evidence and exact boundaries.

## 1. Product behavior

- Project Build Settings selects the game's startup scene and scenes included in a build.
- Opening a project restores the Editor's last scene; this never changes the game's startup scene.
- A packaged game starts the configured scene, including a scene whose name is not Main.
- Gameplay can replace the current scene, load an additional scene and unload an additional scene.
- The main Editor continues to display one editable Scene, with Save/Discard/Cancel before replacement.
- The project directory supplies its root. No root input, scene tabs or Preview Project step is added.
- Editor and command-line builds use the same engine-owned settings, cooker and build entrypoint.
- The final Editor ZIP contains the executable, required private Runtime/Tools resources and licenses;
  it does not include a Nexora/game source checkout, TwoDPlayer, TwoDStudio or TwoDAssetCompiler.
  Compiler standard libraries are required tool data. A built game's runtime is a separate output.

## 2. Verified baseline

At main `d1ec642bbf8330df21d9c3f3e6b87510dcbfd42d`, `ProjectDescriptor` contains identity/name/schema,
not a game startup scene or build list. `SceneFileSession` remembers the Editor scene in
`.nexora/scene-session.ini`; the application falls back to `.nexora/scenes/Main.scene`.
The graphical export coordinator captures only the current document. StaticView schema 1 accepts
exactly one scene and its mesh/scalar-PBR dependency closure, rejecting additional scenes.
`World` supports scene snapshots, activation and deferred unload. Gameplay V3 `load_scene` creates
a logical scene; it does not resolve an authored scene from a game package. The native StaticView
palette is immutable. These are prerequisites, not integrated scene-delivery acceptance.

Authorities: [workspace](../../Engine/Editor/include/Nexora/Editor/EditorWorkspace.h),
[Editor host](../../Apps/Editor/main.cpp), [scene files](../../Engine/Editor/src/SceneFiles.cpp),
[package contract](../../Engine/Runtime/ProjectPackage.md),
[native player](../../Apps/ProjectPlayer/README.md),
[SDK build adapter](../../CMake/Modules/NexoraGameProject.cmake).

## 3. Project settings and identity

Use a versioned, project-owned UTF-8 `project.build.json` next to `project.nexora`.
Keep the existing identity descriptor and Zig script manifest readable; do not duplicate module
build/ABI glue in authored projects. Settings are shared by Editor/CLI and belong in version control.
Editor session metadata remains private and separate.

Proposed schema 1:

```json
{
  "schema": 1,
  "start_scene": "11111111-1111-4111-8111-111111111111",
  "scenes": [
    {"id": "11111111-1111-4111-8111-111111111111", "path": "Content/Scenes/Title.scene", "enabled": true},
    {"id": "22222222-2222-4222-8222-222222222222", "path": "Content/Scenes/Level01.scene", "enabled": true}
  ]
}
```

UUID is runtime/build identity; path is a canonical project-relative authoring locator. The
existing catalog owns identity where available. Existing project-root and `.nexora/scenes` files
need explicit safe identity enrollment; do not copy or silently move them into Content. Renaming
through Editor updates the locator while preserving identity. An external rename must be repaired
explicitly if identity cannot be recovered; no same-name fallback. Duplicate IDs/paths, conflicting
catalog identities, invalid UTF-8, aliases, traversal/outside-root paths, excessive counts and
unknown schemas reject with a diagnostic. Settings are capped at 256 KiB, 256 scene records and
1024 UTF-8 bytes per relative path; the phase-1 parser/tests enforce those bounds before allocation.

The startup UUID must identify an enabled, existing, readable scene. Disabled scenes are omitted;
shared dependencies of enabled scenes are still included. Removing/disabling the startup scene
requires selecting another startup scene before settings can be saved. New projects may initialize
the first successfully saved scene as startup once, visibly; merely opening another scene never
changes it. Old projects without settings still open normally. Build Settings offers explicit
initialization from the current saved scene; game builds require valid settings instead of guessing Main.

Build Settings supplies Add Current/Add Scene, Remove, Enable and Set Startup. Paths are relative,
the startup choice is visible, and invalid/missing entries are explained. Read-only/recovery/lease
guards apply to atomic settings publication. Export prompts to save affected dirty authoring state;
Cancel/failure leaves settings, documents and previous package intact. Builds consume saved snapshots.

## 4. Multi-scene package and build ownership

Add a separately versioned gameplay scene-package capability. Keep existing StaticView schema-1
bytes, validation, CLI behavior and immutable consumer unchanged; a new package must not masquerade
as StaticView. Specify magic/version, project UUID, startup UUID, scene catalog, sorted asset records,
hashes, size limits and supported component capabilities before implementing the codec. Initial
aggregate admission caps are 256 scenes, 4096 assets, 256 MiB serialized package, 64 MiB World
snapshot text, 100000 entities and 128 MiB decoded geometry. Existing stricter per-asset codec
limits still apply; these are admission bounds, not total process-memory or performance guarantees.

The producer captures every enabled scene, computes the union of dependency closures, and cooks
shared resources once by full UUID. Same UUID with different bytes/type/dependencies rejects.
Scene records preserve authoring identities; the runtime remaps instance-local entity references.
Reject missing dependencies, invalid startup/catalog references, unsupported content and budget
overflow before publication. Opaque bytes preserved as inactive by StaticView are not evidence
that gameplay behavior runs; gameplay builds must report unsupported required behavior explicitly.
Cross-scene serialized entity references are unsupported in the first version and reject.

Publish atomically after full verification; stale project/document/settings generations and
cancellation reject publication. The build manifest records source/settings identity, package hash,
startup and included scenes, dependency inventory, executable/module/DLL inventory and command.
Editor/CLI delegate to one engine-owned build service using the established CMake/Zig adapter.
Compilation/cache/ABI glue stays in the SDK/build directory. Source scripts are optional only for
a declared scene-only game; a configured gameplay script must build and load successfully.

The standalone game launches without user-supplied arguments or authored sources, locating its
own runtime, gameplay module and package relative to the executable, including from a foreign CWD.
It needs neither Editor nor compiler at runtime. This developer-PC relocation gate is distinct
from an independent clean-host/platform acceptance gate.

## 5. Runtime transition contract

Use an owning Runtime scene-package session, independent of Editor. Keep a verified immutable CPU
catalog and shared cooked assets for the package lifetime; create fresh runtime entity/scene IDs
per load and remap hierarchy/component references without changing authored bytes. Public gameplay
references use validated instance/generation handles; references to unloaded entities reject.
Do not resolve duplicate entity IDs by overwriting another scene or by loading into the live World first.

Scene requests execute serially at a safe owner-thread frame boundary after gameplay callbacks.
Allow one pending transition; another request returns Busy. Keep the last completed request status
until replaced or session destruction, with at most 256 loaded scene instances. Expose deterministic
not-found/unsupported/error results. Before
commit, stage World data, all required native resources and camera/render bindings. Any preparation
failure retains the old active world, camera and render state. Initial startup failure exits with
a useful log; it never silently substitutes Main.

- **Replace (default):** activate the fully prepared scene, then retire previous nonpersistent scenes.
- **Additive:** retain existing scenes; reject duplicate active scene asset requests initially.
- **Unload:** retire the specified nonpersistent instance; persistent scenes require explicit host policy.
- Define deterministic active-camera selection and fallback for camera-less scenes in the contract;
  additive loading must not accidentally steal camera ownership. Unloading the camera scene selects
  a valid retained camera or the documented fallback.
- Shared CPU assets survive scene retirement; GPU resources remain alive until submitted work drains.
  A failed request releases its temporary ownership. Repeated transitions must stabilize resource counts.
- App-scoped gameplay modules remain attached across transitions. Complete callbacks before unloading
  scene-owned state; copied tables/handles cannot outlive their owner. Document event ordering and
  stale-handle behavior. Stop/teardown cancels pending work before destroying its package/services.

Add an optional, size/version-negotiated scene service to the existing C gameplay service mechanism,
with Zig helpers for request/status. Do not change V3 `load_scene` semantics or existing ABI offsets.
Old modules retain their negotiated prefix; unsupported hosts report Unsupported. Editor Play and
standalone Player register the same service and use the same session, preventing divergent semantics.
Current custom 2D content/export remains a distinct capability until an explicit adapter is tested;
do not claim this work accepts every 2D/physics/plugin format or the full Editor milestones.

## 6. Ordered delivery phases

| Phase | Scope | Required result | Acceptance |
| --- | --- | --- | --- |
| 1 | Settings/core and graphical Build Settings | Independent startup/build list, migration and guarded persistence | ✅ Scoped settings/UI contract accepted; PR #716 |
| 2 | Multi-scene cooker/package | Deterministic catalog and union closure; legacy StaticView unchanged | ✅ Scoped cooker/package contract accepted; PR #717 |
| 3 | Runtime session and C/Zig scene service | Transactional Replace/Additive/unload, identity and lifetime checks | ✅ Scoped session/API contract accepted; PR #718 |
| 4 | Shared build, Editor Play and native Player | Same authored scenes, startup and transitions through all entrypoints | ✅ Accepted; PR #721 |
| 5 | Integrated validation and Editor delivery | Evidence, CI, merged source and runnable Windows Editor-only ZIP | ✅ Accepted; final acceptance record |

Implement in this dependency order on one feature branch; record phase evidence in the relevant
contracts/roadmaps and Beads. A planning PR can establish the contract without accepting a phase.
Preserve unrelated claims and historical additive-editor/StaticView acceptance records.

## 7. Acceptance fixture and gates

Create an isolated project with Title, Level01 and an excluded third scene. Title and Level01 have
visibly distinct geometry/cameras plus a shared asset. Their authored local entity IDs deliberately
overlap. Select Level01 as startup while Title is the last edited scene. Add a small Zig gameplay
script that invokes the scene service, with deterministic frame/status logging and native observations.

| Gate | Required observation |
| --- | --- |
| Settings/UI | Persist/reopen startup, include/exclude, rename, read-only/recovery, dirty Save/Discard/Cancel |
| Determinism/integrity | Equal input bytes yield equal package; shared asset once; excluded scene absent; corrupt/missing/conflicting entries reject |
| Startup | Zero-argument game starts Level01, independent of Main, last Editor scene and working directory |
| Transition | Level01 → Title → Level01 changes actual native pixels/camera; source files remain unchanged |
| Additive/unload | Overlapping authored IDs are remapped; persistent retention, duplicate load and stale handles follow contract |
| Failure conservation | Missing scene, corrupt resource and native preparation failure leave previous scene playable/visible |
| Lifetime | At least 100 transitions plus module Stop/teardown; no residual process; stable resource/handle counters and sanitizer gates |
| Compatibility | Old project, StaticView package, V3 module prefix and disabled optional-feature builds remain valid |
| Relocation | Run copied game after source removal from a foreign CWD; retain hashes, dependencies, exit/stdout/stderr and observations |
| Editor ZIP | Relocated Editor starts with no arguments; Build Settings and authored fixture work using shipped dependencies |

Implementation acceptance requires the complete commands below on Linux, with logs/counts/skips:

```bash
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping
```

Also run relevant C/Zig ABI, optional-feature stripping, native display and Windows tests. Retain
exact source/tool identities, package hashes, commands, logs and the original failed attempts.
Hosted Linux/Windows, this developer PC, VM and independent clean host remain distinct evidence.
Create/update the agent's PR and merge only after applicable exact-head CI succeeds. Green checks
mark only evidence-backed completed scope; these requirements alone mark no implementation complete.
