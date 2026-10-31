# Nexora Zig Showcase and Engine-owned Entry Point Roadmap

> Version: v1.0 | Status: planning baseline | Updated: 2026-10-01

> **Progress: 100%** (as of 2026-10-01; weighted acceptance checklist across the six milestones
> in section 4; completed items are marked with ✅ and the result is rounded down to 10%.)

**Implementation status (2026-10-01):** ZS-M0 through ZS-M5 are complete. ABI V3 now defines
explicit result and capability values, separate create/start/stop/destroy phases, fallible variable/fixed updates, and
transactional state migration in the C++ host. The in-tree C++ fake module exercises this contract.
The Zig module now owns independently allocated state obtained and released through paired V3 host
allocator callbacks, including reload candidates. The fake and Zig module now consume one vector
set; allocation tags, negative descriptor/callback coverage, and Linux sanitizer presets complete
ZS-M0. Runtime dynamic discovery and generation lifecycle are implemented; the Showcase builds and
selects a Zig Development shared library, while Shipping retains the same statically linked ABI.

**Editor workflow extension (2026-11-01, acceptance pending):** ED-WF-04 adds paired typed
Zig save/load data, an actual registered-work barrier and strict embedding-owned World/resource
transactions. A scoped Windows fixture passed 32 real Zig library replacements and 224 rejected
candidates; complete Cloud/CI/Editor integration is still pending. This extends the historical
module-state contract above without accepting Editor background reload or animation/pool workflows.
See [the work-order evidence](../../Tools/Build/evidence/EditorWorkflow/ED-WF-04.md).

**Local implementation status (2026-09-23):** Deterministic headless/static and
Development-dynamic ZS-M1 verification is now available as `NexoraShowcase.exe`. C++ owns `main`, engine lifecycle, a small
`GameWorld`, fixed/update scheduling, offscreen scene rendering, reload, and shutdown; the Zig
consumer mutates the primary entity Transform through the public ABI and the executable emits a
JSON evidence report. Dynamic module discovery is exercised by CTest. Native Win32/DX12 presentation
is implemented through the Window & Presentation contracts, but still requires target-host Windows acceptance.

## 1. Architectural decision

The Showcase `main`, platform window, engine lifecycle, render loop, and shutdown are owned by the C++ Host/Engine. Zig is a loaded gameplay module, never the process owner. It calls the stable C ABI to create content, consume tick/input, manipulate entities/components, and update presentation state. This makes Zig more than an ABI smoke while retaining that smoke gate.

```text
OS -> C++ NexoraShowcase main
   -> engine bootstrap/window/RHI/runtime
   -> load Zig gameplay module
   -> nx_game_query_api / nx_game_create
   -> on_start -> fixed_update/update -> on_stop
   -> engine render/present and final shutdown
```

## 2. Module contract

The Zig dynamic/static module exports fixed C symbols and a descriptor containing ABI version, structure size, capabilities, and callbacks. The engine supplies a versioned `NxEngineApi` table and opaque context. It owns worlds, assets, window, GPU, thread pool, and callback invocation. Zig owns and destroys its gameplay state.

No exception crosses the ABI; callbacks return `NxResult`. API pointers live only for the module lifetime and borrowed frame data cannot be cached. Reload first quiesces jobs, then migrates a versioned state blob transactionally and rolls back on failure. Zig submits high-level draw requests, never native device pointers. Shipping may statically link while preserving the same contract; Development favors dynamic reload.

## 3. Showcase content

The first playable slice is a small 3D gallery. Zig spawns a camera, rotating cubes, lights, and materials; consumes input snapshots; raycasts to select objects; and displays transform/entity/asset/frame diagnostics. Bounded physics/character and large-world areas follow. Rendering and swapchain creation remain engine responsibilities.

| Room | Public APIs called by Zig | Visible evidence |
| --- | --- | --- |
| Math Lab | vectors, quaternion, transform, ray/AABB | gizmos, values, property-test state |
| Scene Lab | entity/component, scene, asset | spawn/despawn, reload, generation |
| Gameplay Lab | input, fixed tick, physics/navigation | controllable actor and query trace |
| Presentation Lab | animation/audio/VFX facade | counters and backend/fallback badge |
| Streaming Lab | cell requests and residency queries | cell/HLOD overlay |

UI distinguishes `IMPLEMENTED`, `CONTRACT ONLY`, and `UNAVAILABLE`; placeholders never impersonate backends.

## 4. Milestones

- **✅ ZS-M0 Contract:** host-owned lifecycle, function table, errors/memory/threads; one suite for a C++ fake and Zig smoke.
  - ✅ ABI V3 lifecycle, result/capability values, paired allocator, and transactional state migration.
  - ✅ C++ fake-module lifecycle and reload contract coverage.
  - ✅ One shared conformance-vector set for the C++ fake and Zig consumer.
  - ✅ Owner tags, missing-symbol/version/structure-size/callback failures, and Linux sanitizer gates.
- **✅ ZS-M1 Bootstrap:** C++ `NexoraShowcase` owns CLI, window/headless mode, discovery; Zig start/update/stop executes.
  - ✅ C++-owned `main`, Engine/World lifetime, fixed/update scheduling, and ordered shutdown.
  - ✅ Deterministic headless validation backend, JSON evidence, and static Zig-object consumer.
  - ✅ Runtime dynamic-library discovery, generation ownership, job quiescence, and real library unload/rollback.
  - ✅ Build and select the Zig Development shared-library artifact from `NexoraShowcase`.
  - ✅ Consume native window/input/swapchain through the Window & Presentation boundary.
- **✅ ZS-M2 API scene:** build scene, camera, mesh, input, and diagnostics solely through public C/Zig bindings.
  - ✅ Zig creates the scene, camera, light, and cubes through the public host table; C++ retains engine/world/render ownership.
  - ✅ Append-only spawn/despawn, scene, input snapshot, opaque asset handle, raycast, high-level debug draw, and diagnostics callbacks.
  - ✅ C header, ABI manifest/baseline, Zig binding, ownership/thread/error contract, C++ ABI gates, Zig smoke, and deterministic headless evidence are synchronized.
- **✅ ZS-M3 Feature gallery:** bounded physics, presentation, and streaming rooms with capability fallbacks.
  - ✅ Scriptable room selection and deterministic capability/fallback evidence for the Math, Scene,
    Gameplay, Presentation, and Streaming rooms.
  - ✅ Native interaction adds WASD camera movement while the public Zig scene callback performs selection raycasts and high-level query traces.
  - ✅ The 2026-10-02 Linux/Xvfb Rendering Room keyboard gate now verifies 600 native Vulkan GPU frames and camera movement from `Window::Key` events; the previous ASCII comparison is fixed. See the [Phase B evidence](../../Apps/Showcase/evidence/V1-Phase-B-Linux-Vulkan-2026-10-02/acceptance.md).
  - ✅ Physics/navigation, animation/audio/VFX, and cell/HLOD room overlays derive their state from compiled runtime capabilities.
  - ✅ Every room exposes `IMPLEMENTED` / `CONTRACT ONLY` / `UNAVAILABLE`, and the minimal-capability CTest locks the fallback matrix.
- **✅ ZS-M4 Reload/failure:** transactional reload, state migration, bad ABI rejection, and rollback.
  - ✅ Job drain, restore failure, old-generation rollback, shutdown-during-reload, and repeated-reload stress for real dynamic generations.
  - ✅ Dynamic library replacement waits for stable size/write-time samples before loading. The host records generation-scoped update/fixed-update failures, and Presentation maps device loss to explicit device recreation rather than surface retry.
- **✅ ZS-M5 Distribution:** dynamic Development and static/packaged Shipping profiles with license/build/API manifests.
  - ✅ Reproducible Development-dynamic and Shipping-static package targets emit license,
    build/API/content manifests, per-artifact SHA-256 digests, and `SHA256SUMS`.
  - ✅ An independently provisioned Windows 10 target passed the generated command from its package
    root after verifying all 16 `SHA256SUMS` entries; exit code 0 and `launch-report.json` status `PASS`
    are retained in [the acceptance record](../../Apps/Showcase/evidence/ZS-M5-Windows-CleanMachine-2026-10-01/acceptance.md).
  - ✅ Linux clean-package evidence verifies all checksums, stages a fresh isolated copy, launches
    the relocatable dynamic package from that copy, and retains its report/exit status. Other target
    hosts remain open.
  - ✅ On a Windows development workstation, isolated copies of the Development-dynamic and
    Shipping-monolithic/static packages passed all 13 and 5 checksums respectively, launched the
    recorded command, and emitted `PASS` reports. This workstation evidence is separate from the
    independently provisioned clean-machine acceptance. The CI workflow is configured to run the
    package evidence targets.

## 5. Evidence and completion

The engine must outlive the module. C++ and Zig consumers run identical ABI vectors. A headless scripted tour emits deterministic JSON; a native-window smoke proves real presentation, while screenshots are visual regression evidence rather than correctness oracles. Test missing symbols, version mismatch, callback errors, outstanding jobs, failed restore, device loss, and shutdown. Use host sanitizers, Zig safety checks, and allocation owner tags. Windows/DX12 is the first graphical gate; Linux headless remains the CI gate, and Vulkan/Metal advance only after execution on their hosts.

Done means `NexoraShowcase` starts from C++ `main` and Zig visibly drives the scene through public APIs, with headless, failure, reload, and Shipping evidence. Zig cannot directly access RHI/native windows or own editor UI. This document complements [`V1-Visual-Showcase-Long-Term-Plan.md`](V1-Visual-Showcase-Long-Term-Plan.md) by defining the Zig consumer and engine-owned entry point specifically.

## V1 Visual Showcase follow-through (2026-10-03)

✅ The Linux/Vulkan application now presents eight live Runtime room views, readable native GPU UI,
held keyboard/pointer controls and a 210-second guided tour under Xvfb/lavapipe. Full package tooling
includes content, deterministic ZIP/SHA-256 and isolated Linux shared-library resolution. These are
V1 Visual Showcase integration results; they do not replace this roadmap's existing platform gates
or establish the expanded Windows version's clean-machine graphical acceptance.

✅ Windows DX12 local Development/Full and Shipping/Full native execution is recorded in
[Windows-V1-DX12-Local-2026-10-03](../../Apps/Showcase/evidence/Windows-V1-DX12-Local-2026-10-03/acceptance.md).
The GTX 960 developer-machine run covers eight rooms, primitive/texture/instance rendering,
native offscreen graph/UI/present, input/edit/reload/animation captures, Lab asset/plugin rejection
and the complete 210-second guided tour. The explicit Development preset and stock PowerShell 5
default-path fix make the documented local workflow reproducible. Clean-host and physical-display
operator attestations remain pending; this does not complete V1's final acceptance.
✅ The Windows local verifier now requires fresh exports, successful edited-scene snapshot round trip,
reset tour progress and the exact sampled M5/M6 rejection inputs/outputs. Source/package hashes use
pinned LF bytes. Baseline e4a140139189 passes Linux Development 77/77 and Shipping package CI;
physical-display and clean-host final gates remain pending.


## Optional Zig 2D engine API (2026-10-14)

✅ The original V3 prefix now has an optional, size/capability/version-gated 2D extension.
Copied Sprite/collider/camera, animation event buffers, query snapshots, Tilemap and generational
pool batches reach the existing TwoD services. Actual Zig static/dynamic/reload and wire-layout
contracts pass; Linux native input acceptance presents 600 frames with Zig movement.
See [Zig usage](../../Gameplay/Zig/README.md) and [2D roadmap](2D_Roadmap.md). This additive
engine-control scope leaves the original roadmap percentage and physical Windows gates unchanged.

## 2D GPU instancing follow-through (2026-10-15)

✅ The unchanged Zig V1 instance batches now reach native CPU-driven GPU instancing via
`SpriteFrame::Draw`. Linux pixel acceptance verifies a 256-instance batch plus transparent
cross-Atlas/HUD ordering; a 600-frame real Zig input/reload run records native instance counters.
See [2D scope and platform limits](2D_Roadmap.md) and [usage](../../Gameplay/Zig/README.md).
This adds no new ABI or general Mesh/Material/Compute/Indirect API and accepts no physical Windows gate.

## SP-01 standalone display integration (2026-10-23)

✅ Portable implementation and regression checks: 252 passed, 16 native-prerequisite skips, 0 failed; Monolithic Shipping consumer passed.

The standalone 2D Player now has optional project window settings, an append-only independently
negotiated C/Zig display table, frame-stable pixel/logical/DPI snapshots, native window mode
requests, game-selected aspect bounds, safe-frame extraction/scissors and DPI-correct UI/input.
Historical service consumers and descriptors retain their prefixes/defaults. World camera span,
HUD layout and UI scale remain game-owned. This does not add Editor Play display wiring.

See [the display contract](../../Engine/TwoD/GameplayDisplay.md),
[Linux evidence](../../Tools/Build/evidence/SP01-GameplayDisplay-Linux-2026-10-23.md) and
[Windows procedure](../../Tools/TwoD/WindowsDisplayAcceptance.md).
Windows physical DPI/click/mode acceptance is NOT RUN / NOT ACCEPTED; 2D M6/M7 and existing
milestone counts remain unchanged. Portable implementation checks do not close those gates.

✅ Native X11 close-race regression: destroyed-before-pump metrics and cleanup pass on Xvfb; the preserved pre-fix DSO fails the same probe.

## SP-02 image UI and Zig composition (2026-10-23)

✅ Portable image/nine-slice UI, style-free interaction, glyph metrics, reusable font handles and
snapshot-aware texture retirement are implemented with independently negotiated C/Zig wires.
Original and SP-01 services prefixes remain compatible. Zig diagnostic screens cover HUD slots,
choices, loadout, wrapped subtitles and settings without game rules in the host.

A strict portable NXFONT1 baker reports complete supplied-corpus coverage, input/font hashes
and atlas budgets. Actual planning-document coverage is not final-script acceptance; diagnostic
icons are not final art. See [the UI contract](../../Engine/TwoD/GameplayUiDraw.md) and
[cloud evidence](../../Tools/Build/evidence/SP02-ImageUi-Cloud-2026-10-23.md). Final artwork,
Windows typography/input, physical gamepad and the existing milestone gates remain open.

## SP-03 deferred asset residency (2026-10-23)

✅ Bounded catalog IO, copied C/Zig request states, explicit asset removal, dependency-ordered
unloading and snapshot-aware native texture retirement are implemented. Original/SP-01/SP-02
services prefixes and legacy preload defaults remain compatible. Capacity is explicit rather
than an unconditional 64 MiB increase; Zig retains transition and gameplay/event policy.

Portable verification covers five simultaneous 2048² Atlases (>80 MiB) across eight unload/reload
rounds, retained-generation backpressure and real process RSS/working-set observations. A separate
Zig module is exported from a >80 MiB package and exercises 15 transitions, 1800 pooled instances
and 30 Animator events, with corrupt-file rejection. Native pixel coverage checks fresh 2048² GPU
generations and held-frame retirement; executed platform evidence must be distinguished from
compilation. See [the contract](../../Engine/TwoD/GameplayAssets.md) and
[cloud evidence](../../Tools/Build/evidence/SP03-AssetResidency-Cloud-2026-10-23.md).

Final game art/content, complete pickup/combat transitions and physical target-PC RAM/VRAM/frame
budgets remain unaccepted. Audio streaming/compression, color and existing M6/M7 gates are unchanged.

## SP-04 explicit authoring color and 2D fallback (2026-10-23)

✅ Opt-in sRGB PNG import now decodes RGB once into the existing linear RGBA8 Atlas contract,
preserving straight alpha, historical defaults, Atlas wire format and services ABI. Conflicting
profiles/ICC reject before publication. Zig raw-image/tint helpers share the transfer contract.
✅ The native 2D Player requires hardware sRGB UI/Sprite output on Vulkan/DX12/Metal; legacy
mixed 3D surface defaults remain unchanged. Native pixel tests require encoded gray, not UNORM.
✅ A separately exported Zig diagnostic composes top-down layers, Y crossing, foreground fading,
baked glow/shadow removal and an always-retained danger outline. These are procedural fixtures
and ordinary alpha layers, not dynamic local lighting, occlusion shadows or selective bloom.

See [color/fallback contract](../../Engine/TwoD/ColorAndFallback.md) and
[cloud evidence](../../Tools/Build/evidence/SP04-ColorFallback-Cloud-2026-10-23.md).
Final first-level art, dark-gradient quantization, transparent filtering/white edges, readability
and physical Windows target output/performance remain open (`nexora-ofe.1`). A native test must
actually run before pixel acceptance is claimed; local compilation does not suffice. Existing
M6/M7 gates and unrelated SP gaps are unchanged.

## SP-05 bounded offline voice playback (2026-10-23)

✅ Decoded PCM now has explicit active/live/count budgets with exact accounting for samples
retained by removed/replaced voices. An optional AudioState service adds copied usage and an
independent Voice bus, preserving Audio V1 and every original/SP-01/SP-02/SP-03 services prefix.
✅ A caller-owned Zig dialogue controller reuses deferred local blobs for WAV registration,
play/pause/skip/replay, independent voice gain/mute and subtitle fallback. The Player exports
explicit audio capacity separately from asset budgets. No complete voice pack is startup-preloaded.
✅ A standard-library inventory tool reports declared WAV bytes/duration, subtitle-ID gaps and
concurrent PCM sets; metadata/fixture results do not establish the final recording corpus.

✅ API-M6 pins AudioState C/Zig field order and all four callback metadata contracts; mutation
guards reject their removal/reorder. Zig remembers pause requested during loading and applies it
before mixing, including replay until explicit resume.

Portable verification uses real PCM samples, failure/reopen, cancellation and legacy canaries;
an independent Zig module/Player exercises twelve 87-second procedural lines in a >96 MiB offline
package with 16 MiB peak PCM and zero final residency. Captured/null sinks and sampled RSS are
diagnostic observations, not physical audibility or production RAM/frame-time acceptance. See
[voice contracts](../../Engine/Runtime/GameplayVoice.md) and
[cloud evidence](../../Tools/Build/evidence/SP05-OfflineVoice-Cloud-2026-10-23.md).

Final recordings/subtitle corpus, full offline installation, worst-case concurrency and physical
Windows playback/unplug/reopen/long-soak gates remain open in `nexora-6d6.1`. Compressed decoding,
audio disk streaming and realtime mixing are not implemented by this per-line PCM path.
Other SP assessments and M6/M7 acceptance remain unchanged.

## SP-06 gameplay lifecycle, close handshake and rumble (2026-10-23)

✅ Optional Lifecycle V1 preserves every previous services prefix (including SP-05's 88 bytes),
with enforced C/Zig field order and callback metadata. Retained focus/minimize/power/disconnect
sequences are visible before fixed updates; an explicit guard routes native close through a token
that Zig can cancel or accept after a real successful save. Legacy close behavior is preserved.
✅ Windows translates suspend/resume and noncoalesced minimize/restore notifications. Bounded
XInput rumble reports support/native errors, expires on owner ticks, and requests zero on blur,
minimize, suspend, close and Stop. DirectInput/DS4/Linux rumble remains unsupported.
✅ A real Zig V3 diagnostic retains a pending marker, pauses before fixed simulation, retries failed
atomic snapshots, reopens paused, and requires explicit confirmation plus a three-second preparation.
Portable tests cover native-output failure retry, token/prefix/thread guards and actual Storage
failure preservation; the independent exported Player exercises negotiation/UI/ownership.

See [lifecycle contract](../../Engine/Runtime/GameplayLifecycle.md) and
[cloud evidence](../../Tools/Build/evidence/SP06-Lifecycle-Cloud-2026-10-23.md).
Final game card/transaction integration and physical Windows focus/minimize/controller/sleep/rumble
acceptance remain open (`nexora-4j7.1`). Message injection is not actual OS sleep or motor sensation.
This does not complete SP-06 product acceptance, M6/M7, or unrelated SP gaps.

Fresh post-foreground movement is retained; UI confirmation still waits for neutral rearming.
