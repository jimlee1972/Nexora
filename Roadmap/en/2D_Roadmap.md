# Nexora 2D Engine Roadmap

> Version: v1.7 | Status: cloud delivery accepted; PC host gates open | Updated: 2026-10-19

> **Progress: 82% capability acceptance** (9/11, rounded; P0 6/8). M0–M5 and M8–M10
> have portable/Linux functional evidence. M6 now requires 5,000-object integrated acceptance
> on the current PC: NOT RUN. Retained 10,000-object stress screens are 23/24, integrated 3/4
> and 30-minute soaks 1/2; those historical verdicts remain unchanged. M6 remains open.
> M7 clean Windows runtime/Shipping remains **PARTIAL / NOT ACCEPTED**. The rebuilt current-main VM
> package passes CPU, native software DX12, Zig instancing and human keyboard movement. The current
> physical developer PC passes the scoped Runtime checks, including PS4 USB, hardware DX12/Vulkan,
> human Chinese UI/audio and 100/150/175% DPI; independent clean-host acceptance remains open.

## 1. Scope and baseline

Deliver the engine capabilities for sprite-heavy, top-down single-player Windows PC applications
on the V1 foundation. Orthographic cameras, Unlit materials, instancing, VFX sprites, UI/text,
input, audio, asynchronous assets, serialization and packaging have existing plans or slices.
The optional TwoD module supplies scene Sprite extraction and lightweight 2D queries, reusing
World, assets, Core VFS, native Presentation and EditorImGuiHost. Physical PC performance and
clean Windows integration remain separate. No Jolt replacement or gameplay is introduced.

Scope: reusable rendering, assets, queries, runtime lifecycle, profiling, packaging and authoring.
Gameplay rules, combat, spawning logic, progression and game content are outside this roadmap.

## 2. Capability milestones

P0 establishes the usable runtime and evidence. P1 improves production workflows; Tilemap is
conditional. Every row remains unaccepted until its complete condition passes; only then use `✅`.

| ID | Priority | Capability | Required scope | Acceptance condition | Current assessment |
| --- | --- | --- | --- | --- | --- |
| ✅ 2D-M0 | P0 | Scene Sprite Renderer | Atlas regions, pivot, flip, tint/alpha, sorting layers and stable Y ordering; existing transforms/orthographic cameras | Native captures verify UVs, pivot, flips, transparency and overlapping order; save/reopen retains fields | ✅ Portable/Linux capability acceptance; evidence below |
| ✅ 2D-M1 | P0 | Frame animation | Frame duration/rate, looping, switching and animation events, separate from skeletal animation | Frame/event traces verify loop boundaries, switching, pause/resume and large time steps without duplicate events | ✅ Portable/Linux capability acceptance; evidence below |
| ✅ 2D-M2 | P0 | 2D asset pipeline | Slicing, deterministic Atlas packing, import/sampling settings, transparent-edge handling and stable references | Identical inputs/settings/tool versions reproduce Atlas layout, regions and output hashes; import/cook/reimport retains references; native sampling has no edge bleed; invalid input preserves published assets | ✅ Portable/Linux capability acceptance; evidence below |
| ✅ 2D-M3 | P0 | Lightweight 2D collision/queries | Circle/AABB overlap, filters, range queries, spatial index and swept checks for fast movement | Compare indexed results with an independent brute-force oracle; cover boundaries, filtering, fast motion and insert/remove | ✅ Portable/Linux capability acceptance; evidence below |
| ✅ 2D-M4 | P0 | Bulk lifecycle/updates | Batch create/recycle, generational identity, allocation control, data-oriented updates and fixed-step scheduling | Repeated cycles reject stale handles, preserve safe teardown and report allocation/frame spikes; varied frame deltas verify fixed-step count, ordering, accumulated remainder and equivalent state for the same simulated time | ✅ Portable/Linux capability acceptance; evidence below |
| ✅ 2D-M5 | P0 | High-volume Sprite rendering | Atlas batching/instancing, visibility culling, transparency ordering and overdraw control via existing RHI | Native captures preserve order across batches; sparse/overlapping workloads retain draw counts and CPU/GPU timings | ✅ Portable/Linux capability acceptance; evidence below |
| 2D-M6 | P0 | Profiling/stress acceptance | CPU/GPU frame time, memory, draw counts, allocation spikes and transparent overlap | Reproducible 5,000-object integrated workload meets declared 30 FPS/frame-time and resource budgets on the specified PC; retain 10,000-object stress results and traces/configuration | 5,000-object integrated gate NOT RUN; historical 10k stress retained; M6 NOT ACCEPTED |
| 2D-M7 | P0 | Windows runtime/Shipping acceptance | Input (including any physical game controller), UI/text, audio, async loading, serialization/save, cooked assets and standalone packaging | Clean Windows machine runs cooked 2D validation app without Editor/developer dependencies; keyboard/mouse and a physical game controller, UI/text rendering, audio playback, async-load completion/failure, reload, save/reopen and shutdown pass | VM CPU/software DX12/Zig plus scoped physical-host DX12/Vulkan UI, human keyboard/audio and 100/150/175% DPI passed; PS4 USB/current-host Runtime slice passed; clean-host gate open; M7 NOT ACCEPTED |
| ✅ 2D-M8 | P1 | 2D scene authoring | Sprite placement, pivot/order editing, collision visualization and orthographic Editor preview | Edit-preview-Undo/Redo-save-reopen preserves authored scene; isolated Play does not change authoring World | ✅ Portable/Linux capability acceptance; evidence below |
| ✅ 2D-M9 | P1 | Atlas/animation tools | Slice/Atlas preview, packing diagnostics, frame editing and playback/event preview | Cooked output matches runtime rendering; Undo/Redo, invalid data and reimport are verified | ✅ Portable/Linux capability acceptance; evidence below |
| ✅ 2D-M10 | P1, conditional | Tilemap | Layers, painting, collision metadata, chunk loading/culling | Implemented painting, layers/collision metadata and chunk loading/culling pass Undo/save/reopen and chunk-boundary render/query tests; a decision to defer leaves this milestone unaccepted | ✅ Portable/Linux capability acceptance; evidence below |

### M7 physical game controller acceptance

Any physical game controller may be used for M7 input acceptance, including DUALSHOCK, DualSense,
Xbox controllers or other joysticks; no brand or XInput requirement applies. The game must actually
receive and verify the buttons, axes/deadzones and UI navigation/confirm/cancel required by the
product, plus disconnect/reconnect, keyboard/mouse switching and focus reset. Retain the model,
connection type, input backend and observed results. A pass covers that model and connection;
it does not establish compatibility with every controller.

The native Windows adapter retains XInput and adds DirectInput joysticks with DUALSHOCK 4
button/axis mapping. Other models expose rebindable controls; compatibility is limited to tested
models and connections. OS detection of a DUALSHOCK or another HID
device alone does not establish game input. See the current implementation scope in the
[input contract](../../Engine/Runtime/GameplayInput.md#native-gamepad-adapter-and-evidence).

### M7 Runtime/Shipping acceptance scope (revised 2026-10-19)

M7 accepts one standalone cooked 2D Shipping validation application; a finished production game
is not a prerequisite. Required checks cover keyboard/mouse and any physical joystick actions/
deadzones, UI navigation/confirm/cancel, disconnect/reconnect and focus reset; Chinese UI,
human-confirmed audio and available output-device recovery; asynchronous load success/failure,
cooked corruption rejection, reload, cross-process engine save restoration/isolation/error
protection, and repeated startup/normal shutdown with resource release.
Any physical joystick may pass; no brand, XInput or additional controller purchase is required.

The current single-monitor DPI acceptance set is **100/150/175%**, using the retained human
display/interaction evidence. **200% is neither required nor pending**. Cross-monitor cases apply
only to a declared multi-monitor configuration and do not block this single-monitor gate.
Audio acceptance covers audible playback and available unplug/switch recovery in the tested
output configuration, reusing the user's confirmed earlier checks instead of repeating passes.
Untriggered endpoint-loss/reopen paths limit compatibility claims; speaker-cable reconnection
does not establish USB endpoint failure, and an unobserved local fault is not an additional exit prerequisite.

Move these conditions out of M7: minimum hardware, 30 FPS/spikes, integrated UI/VFX, allocator/VRAM and
soaks belong to [M6](#2-capability-milestones); main Editor Game View acquisition and Play/Stop
belong to the [Editor Roadmap](Editor_Roadmap.md); production gameplay/content and save schema/
migration belong to individual game projects. M7 uses a fixed fixture schema to test the engine
storage contract, without requiring production content completion.

Record current developer-host acceptance separately from the **independent clean Windows host
gate**. The latter still requires the same package to start without Editor/Zig/Python/MSVC
development tools, with complete dependencies, basic interaction and normal shutdown.
Developer-PC PATH isolation, VM software rendering and CI do not replace that gate.
M7 remains incomplete until the clean-host gate passes.

✅ The cloud exporter now requires an explicitly supplied `--vulkan-loader` when a Windows binary imports
`vulkan-1.dll`, stages it with recursive dependency checks and inventories its hash. This fixes
the implicit OS-DLL distribution assumption; missing loaders fail before publication. Cloud
mocked-import regressions do not execute Windows. Old frozen packages remain unchanged and a
newly hashed package still needs missing-loader-host startup and clean-host acceptance.
[Cloud validation evidence](../../Tools/Build/evidence/TwoDCloudAcceptancePreparation-Linux-2026-10-20.md).

✅ 2026-10-19 physical PS4 USB slice: the Shipping Player reads Sony DUALSHOCK 4 (054c:09cc)
through DirectInput. DX12/Vulkan each exit normally after 120 frames; initial acquisition no longer
submits half-pressed triggers. Two human DX12 sessions total 5,578 frames and confirm left-stick
movement, both triggers, right-stick RX/RY -1..1, D-pad/confirm/cancel, focus, keyboard switching
and USB disconnect/reconnect. Normal teardown, cross-process state restoration and Shipping
corrupt-save rejection pass; full local MSVC Development/Shipping tool builds and 27/27 related
regressions pass. The user additionally confirms all is normal and was tested previously; retained
Chinese UI/audio/supported-DPI evidence is reused without another manual run.
[Raw counters, human replies, source/binary hashes and acceptance boundaries](../../Engine/TwoD/evidence/Windows-PS4-M7-2026-10-19/acceptance.md)
are retained separately. This slice does not establish every controller or untriggered audio
endpoint-loss/reopen behavior. Available current-host acceptance is complete; the clean-host gate
remains open. M7 NOT ACCEPTED, 9/11 (82%), P0 6/8 are unchanged.

## 3. Dependencies and implementation order

1. Inventory Runtime, Renderer, asset/cook, queries, Editor and public C ABI support. Record reused
   contracts, gaps and platform evidence before introducing modules/APIs.
2. Define M0/M2 Sprite/Atlas data, then rendering, import/cook and M1 animation. Synthetic fixtures
   can verify the runtime before full authoring tools exist.
3. Complete M3 queries and M4 lifecycle/update integration without duplicating World, scheduler,
   asset manager or 3D physics services.
4. Integrate M5 batching/culling with M0/M2/M4; instrument performance early. Accept M6 only after
   the combined rendering/query/update path is present.
5. Complete M7 cooked Windows packaging against the other accepted P0 capabilities.
6. Deliver M8/M9 through existing Editor shell, asset and scene transactions; evaluate M10 after
   choosing the map workflow. Editor milestone acceptance remains separate.

## 4. Integration boundaries

- Renderer extracts Sprite data and owns batching/order; RHI keeps backend-neutral resources and
  commands. Do not fork a private UI/VFX scene renderer.
- Runtime owns component state, fixed-step update and identities. Atlas/clip assets reuse existing
  import/cook, stable references, generation and retirement rules.
- Queries return generic overlaps/hits; applications interpret them. Full 2D rigid-body simulation
  is deferred; this plan selects no new physics dependency.
- Editor owns authoring metadata/transactions. Shipping must not depend on its UI or tools.
- Future public C++/C ABI, serialization and optional-module changes need contract review; module
  dependencies belong in `Config/Modules/modules.json` with feature options. TwoD is feature-gated with Runtime/Presentation dependencies; Editor
  conditionally depends on it. Public C++ additions require consumer rebuilds; the original V3 C/Zig prefix is preserved with an optional 2D extension.

## 5. Performance and platform evidence

M6 targets 30 FPS on the specified minimum PC. Before acceptance, fix minimum CPU/GPU/RAM, Windows version, backend/driver,
resolution, build and frame pacing. Fix visible/updated/colliding counts, Atlas/material count,
Sprite pixel coverage/overlap, animation rate, VFX/UI load and loading churn. Product minimum hardware/workload
budgets remain **TBD**; dated current-host fixture plans below declare their own fixed loads and
resource limits. Object count alone is not a performance claim.

Use deterministic synthetic objects, animations, moving query shapes, VFX and UI, without game
mechanics. Include baseline, high-count, transparent-overlap and allocation/loading spikes.
Retain warm-up/duration, average/p95/p99/max frame time, separate CPU/GPU intervals when available,
memory peaks and dropped samples. A 30 FPS target has an approximately 33.33 ms frame budget; define allowed
spikes before acceptance. Software rasterizers establish correctness only.

On 2026-10-18, the user revised the required M6 target from 60 FPS to 30 FPS. Existing
60 FPS/16.67 ms screening records retain their original budgets and conclusions. This target
change alone does not establish physical minimum-PC acceptance.

### M6 current-PC workload revision (2026-10-19)

At the user's request, the required current-PC integrated acceptance load is **5,000 ordinary
objects**. Retain 10,000 objects as additional stress coverage; its load-specific timing failures
do not alone block the 5,000-object gate. Keep their original results and unresolved correctness,
resource or submit/present issues; reducing the target does not prove those issues fixed.

| Ordinary objects | Purpose | Acceptance role |
| --- | --- | --- |
| 1,000 | Functional smoke | Quick correctness checks; does not accept M6 |
| 5,000 | Daily integrated performance and sustained validation | Required current-PC M6 load |
| 10,000 | Additional stress and regression comparison | Retain every result; not the required performance load |

The current test PC is AMD FX-6100, 16 GiB RAM and GTX 960 2 GiB, Windows 10 x64 build 19045.
Use native DX12 and Vulkan separately, Monolithic Shipping, 640x480 and VSync; pin the exact
driver, build and executable identity before measuring. This defines this host's fixture target,
not a universal production-game minimum specification.

All 5,000 objects must still receive updates, transforms and query-index participation every frame,
with moving queries, animation, extraction/culling/sorting, pool/atlas/codec reload and loading
allocation work. Retain the extra 256 translucent VFX, two Sprite atlas batches and 120 actual
GameplayUi quads/commands. Keep mixed and sparse phases; no update skipping or render-only substitution.

Keep full/cycle and per-phase p95/p99 <= 1000/30 ms and maximum <= 2000/30 ms. The planned suite
retains two 600-measured-frame screens per backend after four warm-up frames, reload every 300
frames, followed by 1,800 measured seconds per backend after 1,200 warm-up frames and reload
every 600 frames. Keep the existing sampled resource caps/growth limits, correctness oracles,
complete frame/completed-GPU sample accounting and teardown checks from the
[fixed plan](../../Engine/TwoD/evidence/Windows-M6-IntegratedSoak-2026-10-18/plan.json).

**5,000-object physical integrated acceptance: NOT RUN.** ✅ Cloud benchmark/runner support accepts
explicit `--objects 5000`, retaining 1,000 functional smoke and the default 10,000 stress load.
Every selected object is fully updated/queried; UI/VFX/reload and existing budgets are unchanged.
All original six-case correctness and timing verdicts remain retained; only cases at or below
the selected load gate performance. Cloud checks can validate workload/report contracts; the
physical PC/30-minute gates still require execution. Existing 5,000-object single-case screens
and 10,000-object integration/soak data cannot substitute for fresh integrated evidence. Keep every historical verdict under its original
load and budget. Mark M6 `✅` and close its acceptance task only after all revised required checks
have retained passing evidence; until then M6 stays NOT ACCEPTED and 9/11 (82%), P0 6/8 is unchanged.

Windows DX12 is primary; record Windows Vulkan separately when offered as supported. Evidence
does not transfer between backends. Linux can validate portable contracts and Vulkan correctness.
Physical Windows PC and clean-machine evidence must come from those hosts; missing gates are
`NOT RUN`, never `PASS`.

## 6. Status, deferred scope and related documents

Count these 11 milestones equally for this roadmap only; report actual accepted P0 as x/8
(currently 6/8), reaching 8/8 only when every P0 milestone has acceptance evidence.
Deferred Tilemap stays in the denominator until a reviewed scope revision changes it. Planning
completion does not change V1 foundation, Editor or focused AI-plan percentages. Retain detailed
acceptance beside the module/tool or in evidence directories; Beads remains the durable task tracker.

Deferred: full 2D rigid-body physics, multiplayer, additional platforms and advanced authoring
beyond the listed P1 workflows.

## 7. Cloud delivery and retained acceptance

✅ M0/M5: affine orthographic extraction, pivot/UV/flips, color/alpha, stable layer/order/Y sorting,
adjacent Atlas batching, culling, shared native DrawSprites/HUD, nearest/linear sampling, immutable
residency and GPU-safe retirement. `twod.native_pixels` verifies actual Linux Vulkan pixels,
including cross-atlas translucent overlap, gutters and chunk boundaries.
✅ M1/M2: deterministic PNG rectangle/grid slicing, canonical pack/import/Cook/reimport, UUID
references and codecs; loop/switch/pause/large-step/event traces and invalid publication rejection.
`twod.asset_tool`, `twod.runtime_contracts` and cooked native cloud acceptance retain evidence.
✅ M3/M4: independent brute-force collision oracle, circles/AABBs/masks and exact point/disc sweep;
transactional rebuild, generational batch create/recycle, stale/foreign/reentry rejection and
fixed-step partition equivalence. Mixed stress performs updates, queries, animation, recycle and reload.
✅ M8/M9/M10: existing Editor World/UndoStack/ImGui host, isolated 2D document and Studio tabs,
Sprite/collider/orthographic preview, Atlas/clip frame/event controls and layered Tilemap painting.
Save/reopen, Undo/Redo, read-only/invalid-edit rejection, isolated Play and negative chunk render/query
boundaries pass. Full main Editor milestone acceptance and project-lease integration remain separate.

M6 cloud slice: ✅ CPU/GPU timing, load/reload spikes, process RSS, batches, AABB coverage, sparse
and overlap scenarios are reproducible. Hardware/minimum-spec/frame-spike budgets remain TBD;
software Vulkan results do not accept the physical 30 FPS gate.
M7 cloud slice: ✅ Linux Monolithic Shipping staged with cooked hashes, Core asynchronous loading,
corruption rejection and no Editor/development engine library dependency. Section 10 records
partial Windows VM prerequisite/CPU checks; native DX12 failed and Vulkan was unavailable.
Input/controller, UI/text, actual audio, save/reopen, display/DPI and full product shutdown remain
unverified. **M7 is NOT ACCEPTED**; Windows backend compilation in CI is not that acceptance.

Retained [Linux acceptance record](../../Engine/TwoD/evidence/Linux-2026-10-14/acceptance.md)
contains commands, platform scope, captures and workload CSV. See [module contracts](../../Engine/TwoD/README.md)
and [tools](../../Tools/TwoD/README.md). Generated reports remain under `build/<preset>/artifacts/`.
The accepted cloud baseline above is complete. Beads also tracks the remaining product/device
integration slices and their separate physical target-host gates.


## 8. Zig engine-control extension (2026-10-14)

✅ An append-only, capability/size/version-gated C ABI now exposes Sprite/collider/camera, clip
Play/Pause/Advance with ordered event buffers, cached overlap/sweep queries, Tilemap painting/chunk
state and atomic generational pool batches to Zig. Runtime does not depend on TwoD; disabled
hosts expose no capability. Original V3 consumers retain their prefix.
✅ The actual Zig 0.14 consumer drives the existing World and 2D extraction in static/dynamic
modules, migrates IDs/handles without duplicate content, and passes C/C++/Zig wire and bridge
negative contracts. Linux/Xvfb/Vulkan input acceptance presents 600 frames and verifies normalized
D-key movement through Zig. See [usage](../../Gameplay/Zig/README.md) and
[contracts](../../Engine/TwoD/README.md).

This adds the engine interface for application gameplay; game rules remain excluded. It does
not increase the 9/11 milestone count or accept physical Windows M6/M7. Main Editor project/Play
attachment and game production content are separate integrations.

✅ The optional Editor now provides an owning `TwoDPlayContext` prerequisite: isolated document
World/Sprite/collider state, frozen Atlas/Clip, copied Tilemap/chunks and a fresh bounded pool
with the canonical 2D bridge. Real module callbacks, 32 session lifetimes, owning frames and
source/history preservation are verified; Linux native pixels distinguish frozen red, animated
and retained green, and new-session blue under the same Atlas UUID. Main project/cooked-asset
admission and bounded Game preview is described below; seamless reload remains open; milestone counts are unchanged.
[Evidence](../../Tools/Build/evidence/EditorEDM3-TwoDPlayContext-Linux-2026-10-15.md).

✅ `TwoDPlayAssets` now freezes a bounded cooked Atlas/Clip closure from an explicit Runtime
asset generation, validates dependencies and frame regions, and retains owning payloads across
replacement, collection and store destruction. Temporary pins/residency balance on success and
failure. This prerequisite does not establish project/source provenance or attach main Editor
Play/Game preview; those integrations remain open and milestone counts are unchanged.
[Evidence](../../Tools/Build/evidence/EditorEDM3-TwoDPlayAssets-Linux-2026-10-15.md).

✅ Optional project indexing and synchronous/queued reimport now retain typed immutable
Atlas/Clip payloads. Initial embedded UUIDs, authoritative sidecars, capacity budgets and atomic
Clip dependencies preserve prior model/history on invalid, stale or cancelled publication.
This is a project asset prerequisite; main host API attachment is described below, while bounded Game preview is described below and seamless reload remains open.
Scoped cook/freeze admission is supplied by the following capture owner. Milestone counts are unchanged. [Evidence](../../Tools/Build/evidence/EditorEDM1-TypedTwoDContent-Linux-2026-10-16.md).

✅ `ProjectTwoDPlayAssets` now cooks and freezes the selected current project Atlas/Clip closure
with project UUID/root, Content generation/revision, writer/access/recovery, exact source/sidecar
and dependency admission. Owning observations revalidate before activation and retain frozen data
after project destruction. Main host API attachment is described below; bounded Game preview is described below; seamless reload remains open;
ED/2D milestone counts are unchanged. See the [capture contract](../../Engine/Editor/README.md#project-scoped-cooked-2d-play-capture)
and [evidence](../../Tools/Build/evidence/EditorEDM3-ProjectTwoDPlayAssets-Linux-2026-10-16.md).

✅ The main Editor Play start path now captures/revalidates project Atlas/Clip Content and binds
optional 2D gameplay services to its actual cloned World and selected active scene. Fresh Sprite/
collider metadata and a bounded instance pool survive through module Stop/Destroy; generic subtree
despawn forgets every destroyed ID. Full module replacement and failed load cleanup are covered
by 32 CPU clone lifetimes alongside the existing 3D host. Bounded Game View 2D rendering is described below; authored Sprite
persistence, Tilemap binding, seamless hot reload and physical acceptance remain open. No ED/2D
milestone count changes. [Contract](../../Apps/Editor/README.md#optional-2d-services-in-project-play). [Evidence](../../Tools/Build/evidence/EditorEDM3-MainPlayTwoDServices-Linux-2026-10-16.md).

✅ The main Game panel now offers explicit 3D/2D selection and bounded native 2D preview from the
actual Play clone, frozen multi-Atlas assets and pool. Canonical ordering, 4096-input admission,
GUI texture generations/budgets, DPI/clipping, concurrent/overlapping Scene behavior and Stop
conservation are covered. New sessions/Play owners reset to 3D; full native image resends include
the font. The actual Linux Editor passes 32 native cycles with blended pixels,
Pause/Step/input, unchanged authoring files and concurrent 3D source pixels, and zero Vulkan
validation errors. Authored Sprite persistence, Tilemap binding, seamless DLL reload and physical
acceptance remain open; ED/2D milestone counts are unchanged. [Contract](../../Apps/Editor/README.md#bounded-2d-game-view)
and [evidence](../../Tools/Build/evidence/EditorEDM3-GameTwoDPreview-Linux-2026-10-16.md).


[Retained Zig/Linux acceptance](../../Engine/TwoD/evidence/Zig-Linux-2026-10-14/acceptance.md)
records the complete suite, initial GUI failures/retry and Shipping scope.

## 9. Zig-controlled GPU instancing (2026-10-15)

✅ `SpriteFrame::Draw` now submits compact 44-byte affine/UV/tint records through native
Vulkan/DX12/Metal instance-rate vertex pipelines. Six Quad corners are generated by vertex ID.
The existing Zig V1 create/read/write/destroy batches require no ABI change. CPU projection,
culling, sorting and updates remain; CPU triangle snapshots still serve Editor preview/inspection.
Only adjacent Atlas-compatible Sprites after transparent sorting share a draw; background/HUD,
immutable Atlas generations, nearest/linear sampling and in-flight buffer ownership are preserved.
Malformed/non-affine packets and overflowing instance ranges reject before native submission.

✅ Linux Vulkan pixel tests exercise a 256-instance draw, cross-Atlas transparency, HUD,
UV flips, linear sampling and Tilemap chunk changes. The real Zig input/reload test presents
600 frames and checks native instanced draw/instance/upload-byte counters.
See [usage](../../Gameplay/Zig/README.md#gpu-instanced-2d-rendering-from-zig),
[Presentation contract](../../Engine/Presentation/README.md) and
[acceptance](../../Engine/TwoD/evidence/GPU-Instancing-Linux-2026-10-15/acceptance.md).

This completes the native instancing addition to accepted M5, without increasing 9/11 or
P0 6/8. Physical Windows M6/M7 remain unaccepted; the older VM package slice below does not
validate this instancing addition. Compute simulation/culling, indirect draws,
script-owned GPU buffers and general Mesh/Material instancing are deferred beyond this 2D slice.

## 10. Windows VirtualBox partial acceptance (2026-10-15)

✅ A frozen Monolithic Shipping package has matching EXE/Atlas/Clip hashes in the Windows 10
VirtualBox guest. After explicit installation of Microsoft-signed VC++ Runtime 14.51.36247.0,
two configurations each pass six CPU workloads, cooked asynchronous load/reload and corrupt-Atlas
rejection. The original baseline launch failed with `0xC0000135` due to missing Runtime DLLs.
[Acceptance and raw evidence](../../Engine/TwoD/evidence/Windows-VirtualBox-2026-10-15/acceptance.md)
retain exact runtime provenance, commands, hashes, exits and limitations.

**M7 remains PARTIAL / NOT ACCEPTED.** With 3D off, the software DX12 path fails at `stress draw`;
with 3D on, the VirtualBox adapter rejects DX12 device creation (`0x887A0004`) and the application
fails at `stress acquire`. Vulkan loader/ICD were absent and the tested package had Vulkan disabled.
Physical input/controller, audio, UI/text visuals, VM Studio save/reopen, display/DPI and successful
native presentation remain unverified. The frozen executable predates the latest instancing code;
this slice does not validate current main. M6 stays open; progress remains **9/11, P0 6/8 (82%)**.

The VM was shut down normally and restored to 3D off/64 MiB VRAM, retaining the installed Runtime.
Beads parent `nexora-j4q` remains open; execution is `nexora-j4q.4`. Work stops here at the user's request.

## 11. Local Windows developer-PC slice (2026-10-14–15)

✅ The physical AMD FX-6100 / GTX 960 / Windows 10 host ran the complete MSVC Development
suite, native DX12 Studio document lifecycle, a 600-frame Monolithic Shipping Zig consumer,
and an isolated Cooked Shipping package with async-load/hash/corruption checks. Six mixed
native Shipping workloads retain CPU/GPU/frame-time distributions and process memory samples.
[Windows evidence](../../Engine/TwoD/evidence/Windows-2026-10-14/acceptance.md) records exact
commands, device/driver, pre-run screening budgets, raw results and the MSVC fixture repair.

M6 remains **NOT ACCEPTED**: all cases exceed the predeclared screening budget; 10,000 normal/
sparse objects have whole-frame p95 53.68/61.00 ms, with GPU p95 below 0.8 ms. This screening
does not declare product minimum hardware or validate sustained integrated UI/VFX/allocator budgets. This developer-PC run did not execute the M7 clean-host gate (the separate VM slice above is partial/not accepted);
actual keyboard/controller, audible playback, clean-host redistributables, display/DPI transitions
and separate Windows Vulkan execution remain open. Computer Use initialization failed, so no
physical-input or screenshot acceptance is inferred. Native Zig movement was synthetic input.
The user explicitly limited this run to available local-host checks. Progress stays **9/11,
P0 6/8 (82%)**; Beads parent `nexora-j4q` stays open, with this slice in `nexora-j4q.1`
and the CPU/pacing follow-up in `nexora-j4q.2`.

The frozen executable used for this physical run predates the GPU instancing addition above; these measurements do not validate current-main rendering performance.


## 12. Rebuilt Windows VirtualBox M7 slice (2026-10-15–16)

✅ Source `350c40ee` was rebuilt as Monolithic Shipping and admitted by four guest SHA-256 checks.
Six CPU cases (48 measured frames each), six native DX12 cases (eight each), Cooked asynchronous
load/reload and corrupt-Atlas rejection pass. The Microsoft Basic Render Driver software path
presents 120-frame and 1,800-frame Zig GPU-instancing runs; another 1,800-frame run completes on
the visible guest desktop. The older binary's `stress draw` failure is not reproduced by this package;
no root-cause fix is inferred. A human confirms D/W movement and Escape exit; its independent report
retains 353 frames, 48 movement frames, positive entity X, exit 0 and balanced allocations.
It exits before reload, which remains covered by the separate scripted runs.

✅ Separate Monolithic Development Studio passes synthetic save/reopen and independent read-only
reopen. A human confirms all four tabs/text/preview and Add sprite / Save / Open; the changed file's
hash is preserved on independent reopen. The original close button exits 1; a two-line tool fix
rechecks close after native event pumping. The rebuilt binary then exits 0 on the user's close,
leaves no Studio process and preserves the document. Linux native close regression is added to
`twod.cloud_native`; full PR CI validates it. This is supporting authoring evidence, not full M7.

[Current-package VM evidence](../../Engine/TwoD/evidence/Windows-VirtualBox-Current-2026-10-16/acceptance.md)
retains exact builds, hashes, raw results, screenshot and remaining scope. This guest retains the
previously installed VC++ Runtime, with no developer tools on PATH. M7 remains **PARTIAL / NOT ACCEPTED**:
controller, audible platform audio, integrated product/UI/text/save/shutdown and display/DPI gates
remain open. Existing Benchmark/Zig programs do not produce device audio; Runtime's AudioEngine
performs no device I/O. `nexora-j4q.7` tracks the missing integrated acceptance flow.
M6 physical performance and 9/11, P0 6/8 (82%) stay unchanged.

- [Roadmap index](../ReadMe.md)
- [V1 Complete Plan](Cross-platform_3D_Engine_V1_Complete_Plan_v1_2.md)
- [V1 AI Implementation Plan](Cross-platform_3D_Engine_V1_AI_Implementation_Technology_and_System_Plan_v1_2.md)
- [Graphical Editor Roadmap](Editor_Roadmap.md)
- [Focused Roadmaps AI Plan](Focused_Roadmaps_AI_Implementation_Plan.md)
- [Renderer](../../Engine/Renderer/README.md), [Runtime](../../Engine/Runtime/README.md) and
  [Editor contracts](../../Engine/Editor/README.md)
- [Traditional Chinese edition](../zh-TW/2D_Roadmap.md)

## Zig runtime service integration (2026-10-16)

✅ The optional Zig service slice now reaches rebindable keyboard/mouse/gamepad action snapshots,
focus/disconnect resets, pointer position/delta/wheel, native UI packets with supplied UTF-8 glyph
atlases, PCM/WAV music/SFX controls, schema-owned durable saves and existing cooked Atlas/Clip
registration. The 2D module host feeds native events, reserves rebindable UI actions, renders its
UI overlay, and resolves registered content; the startup fixture is no longer its only content path.
ProjectPlayer can explicitly load V3 gameplay for its existing StaticView adapter; it advertises
no unsupported UI/2D/content table. See [Zig contracts](../../Gameplay/Zig/README.md#optional-zig-gameplay-services)
and [Linux acceptance](../../Tools/Build/evidence/Zig-Gameplay-Services-Linux-2026-10-16.md).

This supports game-author Zig code; it adds no combat rules or formal artwork. M6/M7 and the
82% capability summary remain unchanged: physical Windows performance, device audio/controllers,
font appearance, full 2D project export and clean-host product acceptance remain separate gates.

## Windows gameplay audio prerequisite (2026-10-16)

`GameplayAudio` now has a synchronous Windows default-device waveOut adapter in addition to ALSA
and portable host sinks. A Runtime-only `NexoraTwoDAudioProbe` submits a quiet tone, closes/reopens
the device and reports output availability independently from human audibility. It is available
in Editor-free Monolithic Shipping. The existing Zig host's explicit `--audio` path uses this
adapter; modules still register/play their own WAV assets. No game rules are added.

✅ Linux full gate (167 passed, three unrelated Vulkan skips), actual ALSA null/missing-device
lifecycle, Windows cross-compilation/linking and isolated Monolithic Shipping checks passed.
[Retained evidence](../../Engine/TwoD/evidence/Device-Audio-Linux-2026-10-16/acceptance.md). Windows native playback/audibility,
controller, integrated product and display/DPI gates remain open; M6/M7 and **9/11 (82%)** are unchanged.
See the [audio contract](../../Engine/Runtime/GameplayAudio.md).

## Windows gamepad routing prerequisite (2026-10-16)

The native gameplay adapter now dynamically polls Windows XInput in addition to Linux raw
joysticks. Complete per-slot snapshots reuse the existing action provider for rebinding,
focus/disconnect release and unchanged-packet recovery. Interactive Zig 2D movement/pause now
uses the same action service (WASD/Space or the default left stick/A), preserving deterministic
noninteractive fixtures. Native Windows controller/device and full M7 acceptance remain open;
9/11 (82%) is unchanged.

✅ Full Linux Development gate passes 311/311 with no skips/failures; Shipping and Windows
cross-compilation pass. [Retained evidence](../../Engine/TwoD/evidence/Cooked-Services-Linux-2026-10-16/acceptance.md).
See the [input contract](../../Engine/Runtime/GameplayInput.md#native-gamepad-adapter-and-evidence).

## Integrated cooked service validation prerequisite (2026-10-16)

The existing real Zig 2D host now has explicit `--validation-package` mode: async hash/type/identity/
dependency-checked cooked Atlas/Clip, WAV decoding/output, a supplied readable ASCII fixture font,
UI SAVE/OPEN, action-based native keyboard/gamepad routing, cross-process position-save/resume,
immutable cooked/module reload and ordered callback/audio/storage/surface shutdown. Reproducible
staging refuses Editor dependencies and requires an isolated Monolithic Shipping executable for
normal use. Development test copies are labeled separately.

The fixture validates engine service integration without adding game rules, production artwork,
generic project export or main Editor project Play. Linux software Vulkan pixels/clicks/resize and
null audio do not establish Windows devices, physical audibility, DPI/font appearance or clean-host
acceptance. M6/M7 remain open and **9/11 (82%)** is unchanged.

✅ Retained evidence covers 311/311 Development tests, isolated Shipping, 900 native UI/Sprite frames,
mouse Save/Open, actual text pixels, resize and corrupt/missing-data rejection.
[Acceptance](../../Engine/TwoD/evidence/Cooked-Services-Linux-2026-10-16/acceptance.md); see the [tool contract](../../Tools/TwoD/README.md#integrated-cooked-service-validation).

✅ The Windows audio follow-up uses four owned PCM slots and waits on pending-slot reuse rather
than on a newly submitted block. The actual portable queue verifies copy lifetime, endpoints,
backpressure/timeout, no pending overwrite and reset. This removes the explicit per-block wait
feedback in the host loop; physical latency/FPS/audibility remain unaccepted. See the linked evidence.

✅ Final queued-audio integration gate: 313/313 Linux Development tests, zero failures/skips;
Shipping/isolated native package and strict Windows cross-compilation pass. Physical gates and 82% are unchanged.

✅ Native cooked-service input acknowledgement follow-up: the acceptance driver now requires
positive keyboard-driven position in a CRC-valid SAVE publication before OPEN, then compares the
restored position. Five native repetitions pass; a deliberately missed first D press recovers,
while missing every press still fails. The local Zig-enabled full gate passes 190/190 without
skips. This is a supporting harness correction; M6/M7 and 9/11 (82%) are unchanged.
See the linked cooked-service acceptance evidence for CI failures, scope and controlled experiments.

## General Zig project Player and Windows delivery tools (2026-10-16)

✅ `NexoraTwoDPlayer` and native ProjectPlayer's explicit 2D mode load a separately built trusted
V3 module and project-owned Atlas/Clip, WAV and font atlases without the fixed sample scene/counts.
Fresh-directory export, MSVC CRT staging, a launcher and hash inventory support Shipping delivery.
The Windows font baker checks a real selected font against a bounded UTF-8 BMP corpus; missing
glyphs fail. User-scoped permanent saves remain module-schema-owned and separate from migration.

✅ The main Editor composes scene/pool sprites plus gameplay UI in the selected 2D Game View using
the frozen project asset owner, without duplicating sprites in the HUD submission. Focus/acquisition-gated actions, native audio control/retry and project-scoped
storage remain valid through Stop/Destroy, and native texture retirement is host-owned. CPU and
Linux Vulkan pixels validate 32 Play lifetimes; real Zig Shipping export and independent module
restart/corruption checks validate the general Player path. See the [acceptance record](../../Tools/Build/evidence/Zig-Project-Player-Linux-2026-10-16.md)
and [Windows checklist](../../Tools/TwoD/WindowsAcceptance.md).

Formal art/fonts/game rules and physical Windows sound, physical game controllers, focus/DPI, clean-machine
runtime and minimum-PC acceptance require the actual project/target host. Authored Sprite
persistence, Editor Tilemap binding and seamless reload remain open. M6/M7 and **9/11 (82%)**
are unchanged. Hosted MSVC CI and software Vulkan are not physical product acceptance.

## Local MSVC Player acceptance slice (2026-10-17)

✅ The PR #620 Player and separate Zig module built as native x64 MSVC Monolithic Shipping.
Independent export/Unicode/save-restart/corruption contracts, Windows GDI font checks, three-frame
physical-host DX12 UI submission and Run.cmd passed. waveOut submitted 48,001 frames and reopened;
human audibility is still NOT VERIFIED. The Visual Studio multi-config output path is documented.
[Acceptance and raw evidence](../../Engine/TwoD/evidence/Windows-MSVC-PR620-2026-10-17/acceptance.md)
retain exact scope, inventories and commands. VM individual native cases and final hashes were
observed, but later Guest Control/RPC failures aborted the VM; a full VM lifecycle is not accepted.
Physical input/controller/audio recovery, Chinese/DPI appearance, clean hardware PC and minimum-PC
performance remain open. M6/M7 remain NOT ACCEPTED; **9/11 (82%), P0 6/8** are unchanged.

## Windows VM transport recovery slice (2026-10-17)

✅ The acceptance harness now serializes each VM, preserves raw VBoxManage byte streams, rejects
NUL/invalid UTF-8 inventories, flushes/hash-checks fresh staging, and records actual process exits.
Native cases wait for desktop level 3 and run in an InteractiveToken task; window close waits for
presented fixture pixels. RAM reclamation waits retain the reserve, and failed cleanup reports
unknown rather than reusing an earlier poweroff. Local regressions: 12 Python tests and 2/2 CTest.

✅ A full current-state recovery clone passed **two cold boots, 14/14 cases, save visits 0–4,
120 native Zig frames per cycle, zero residual Player and two normal poweroffs**.
[Raw reports and scope](../../Engine/TwoD/evidence/Windows-VM-Recovery-2026-10-17/acceptance.md)
preserve the old 806-NUL inventory and failed attempts. At that run's end the original guest's second-boot Windows
login stall remained unresolved and its RAM/disks remained saved; only the new clone's duplicated RAM
state was discarded. No old snapshot was restored or original save deleted. This is a recovered
VM/software correctness slice using frozen source `63bb5f7a`, not pristine/physical-PC acceptance.
Physical controller/audio/DPI, integrated product and minimum-hardware gates remain open;
**M6/M7 NOT ACCEPTED, 9/11 (82%), P0 6/8** stay unchanged.

✅ Original-VM recovery follow-up (`nexora-j4q.7.7`): after the user's restart restored the desktop,
the original UUID passed **two cold boots, 14/14 cases, visits 0–4, 120 native Zig frames per cycle,
zero residual Player and two normal poweroffs**. This is bounded recovery verification; the prior
Welcome stall's root cause is not proven. See the [original-VM record](../../Engine/TwoD/evidence/Windows-Original-VM-2026-10-17/acceptance.md)
for preservation checks, historical RAM-state limits and frozen binary identity. A third read-only
cold boot confirms 16 old save hashes, both snapshot UUIDs/current selection and older snapshot RAM
unchanged, followed by normal poweroff. Full M6/M7 and
**9/11 (82%), P0 6/8** remain unchanged.

## Physical Windows developer-host M6/M7 slice (2026-10-17)

✅ Current Shipping Player renders the independent public-C-ABI Chinese host fixture on GTX 960
with explicit DX12 and Vulkan selection. The human confirms audible probe, looped Music/SFX,
three bus gains, pause/resume/stop, external speaker reconnect, keyboard/rebinding, SAVE/OPEN,
focus/minimize/resize and readable Chinese at 100/150/175%. Three manual DX12 lifetimes exit
normally; the repeat records 87 wheel events and four UI cancels. Position and rebind state persist
across independent DX12/Vulkan processes. Raw host counters and human observations are retained
separately in the [acceptance record](../../Engine/TwoD/evidence/Windows-Host-M6M7-2026-10-17/acceptance.md).

PS4 USB HID is present, but the XInput backend used in that run received no input from it;
physical controller acceptance remains open. A subsequent run may use DUALSHOCK or another
joystick under the criteria above; a separate XInput device is not required. The
standard display menu stops at 175%; 200% and multi-monitor tests were NOT RUN at that time.
The 2026-10-19 scope removes 200% and makes cross-monitor testing inapplicable to this single-monitor run. Speaker cable
recovery caused no waveOut failure/reopen and does not prove USB endpoint-loss/default-switch
recovery. The UI-only fixture is separate from the sprite stress workload and a production game.
Those clean-host, minimum-PC/integrated-load/soak, Editor lifecycle and full-device gates were open
at that time; M6/Editor/game conditions are now tracked separately under the scope above and do not block M7;
**M6/M7 NOT ACCEPTED, 9/11 (82%), P0 6/8** remain unchanged.

✅ The real Zig cooked-service fixture additionally passes two 120-frame native processes per backend: 34 visible sprites, one cooked reload, 96,000 audio frames, save/resume and balanced teardown each. Automated fixture movement and audio submission do not add human controller/audibility acceptance.

Current-PC screening completes all six 600-frame cases on both hardware backends, but all 12 rows exceed the predeclared p95/p99/max budgets. 10k mixed frame p95: DX12 31.97 ms, Vulkan 30.99 ms; 10k sparse: 40.33/49.06 ms. External process peak working set is 90.9/148.0 MB; no allocator/VRAM or minimum-PC claim. Functional gates pass independently of this unaccepted M6 performance result.

These rows already use GPU instancing. A separate 60-frame DX12 diagnostic points to CPU extraction (10k mixed p95 16.48 ms) and spatial-index rebuilding (10k sparse p95 27.16 ms); a render-only baseline and optimization remain pending. The host fixture also exposed system gamepad input entering headless Linux runs; native polling is now confined to native mode, with zero-input storage/resume assertions. Linux headless fixture audio uses an explicit ALSA null configuration under sanitizers, independently of physical output. See the same acceptance record for failure evidence and diagnostic limits.

## Windows M6 30 FPS local revalidation (2026-10-18)

✅ Rebuilt x64 MSVC Monolithic Shipping from `9e3adfdd` against the revised 30 FPS target.
The physical FX-6100/GTX 960 host completed two repeats per DX12/Vulkan backend, each containing
six unchanged mixed workloads of 600 frames. All four processes exit 0 with hardware identity
and native lifecycle checks passing. Advance limits were p95/p99 ≤ 33.3333 ms and maximum
≤ 66.6667 ms; 10k mixed and 10k sparse exceed them in both repeats on both backends.
All 24 rows, raw CPU/GPU/frame metrics, process memory samples, advance plan and binary hashes
are retained in [the new evidence](../../Engine/TwoD/evidence/Windows-M6-30FPS-2026-10-18/acceptance.md).

This is a current developer-PC revalidation without engine changes or reduced mixed workloads.
The older 60 FPS screening retains its original verdict. Product minimum specification,
integrated UI/VFX, allocation/VRAM, sustained operation and per-frame traces remain open.
**M6 NOT ACCEPTED; 9/11 (82%), P0 6/8** are unchanged; M7 gates were not exercised in this run.

## Shared 2D path optimization and same-host revalidation (2026-10-18)

✅ The shared grid now uses contiguous cell references and reusable staging snapshots; extraction
uses contiguous ID validation and compact sort references, avoiding re-sorts for already ordered input.
Full per-call rebuild, query/sweep, transparent order, matrix snapshots, owning geometry and invalid-input
rejection remain intact. Public functions and C/Zig ABI are unchanged; private C++ layout changes require consumer rebuilds.

The full MSVC Development build, Shipping benchmark/compiler and 20/20 related local contracts pass.
The FX-6100/GTX 960 retains all six mixed workloads including both 10k cases: DX12 repeats pass
6/6 and 6/6, Vulkan repeats 4/6 and 5/6 under unchanged 30 FPS/spike limits; all four processes exit 0.
Visible/culled counts, batches, coverage, query hits and sample counts match the baseline for all 24 rows.
No reduced workload or relaxed threshold is used. [Raw evidence and comparison](../../Engine/TwoD/evidence/Windows-M6-SharedPath-2026-10-18/acceptance.md)
retain source patch/hashes, CPU/GPU/frame distributions and process memory. Full Linux/native pixel gates are tracked on the final PR CI.

This is a shared engine improvement and developer-PC screen. Product minimum specification,
integrated UI/VFX, allocator/VRAM, per-frame traces and sustained acceptance remain open.
**M6 NOT ACCEPTED; 9/11 (82%), P0 6/8 unchanged.**

## Vulkan FIFO pacing and same-host revalidation (2026-10-18)

✅ The shared Vulkan surface now admits FIFO CPU work after image readiness, using one reusable
fence and per-image presentation semaphores. Immediate acquisition stays asynchronous; two/three
upload slots follow the descriptor independently of image synchronization. Initial SUBOPTIMAL
recovery, partial allocation cleanup, zero extent, resource drain and abandoned frames have native
regression coverage. Default image count, VSync, resolution, benchmark source and all 10,000-object
update/query/extraction/reload work remain unchanged.

The physical FX-6100/GTX 960 screen passes **23/24**: Vulkan passes 6/6 in both repeats,
with four warm-up plus 600 measured frames per case at 640x480 and the original p95/p99 <= 1000/30 ms,
maximum <= 2000/30 ms limits. Visible/culled counts, batches, coverage, query hits and GPU samples
match the prior screen in all rows. Full local MSVC Development and Shipping builds pass;
31/31 related local contracts pass without skips. [Evidence](../../Engine/TwoD/evidence/Windows-M6-VulkanPacing-2026-10-18/acceptance.md) retains the complete
results, executable/source hashes, process memory and separate per-frame/render-only diagnostics.
Full Linux/native pixel and other hosted gates are tracked on the final PR CI.

DX12 passes 6/6 then 5/6. The second 10k sparse run has p99 21.55 ms but maximum 72.36 ms,
exceeding the unchanged 66.67 ms spike limit. This failure is retained and remains open;
Vulkan's three prior p99 failures are resolved by this screen. No all-backend acceptance is claimed.

This records the current developer-PC screen. Product minimum specification, integrated UI/VFX,
allocator/VRAM and sustained acceptance remain open. **M6 NOT ACCEPTED; 9/11 (82%), P0 6/8 unchanged.**

## DX12 VSync admission and same-frame screening (2026-10-18)

✅ Shared DX12 VSync now admits CPU frame work through one DXGI frame-latency handle, with
queue capacity equal to the existing two/three frame slots. Separate GPU fences retain upload,
allocator and retirement ownership. Immediate, buffer count, resize/recovery, zero extent and
teardown remain covered. A single-queued-frame trial regressed 10k p99 and was rejected; its
failed repeats remain in the evidence. The final policy preserves CPU/GPU overlap.

✅ Optional benchmark `--frame-trace` buffers every frame's CPU/full interval and eight contiguous
stages, with explicit warm-up flags and no in-frame file output. It adds measured clock cost;
all original animation, movement, complete query rebuild/sweep, extraction/order, instanced
draws and midpoint pool/Atlas reload work remain. Aggregate CSV schema and interval boundaries
stay intact. Portable regression recomputes p95/p99/max from the same samples and checks output errors.

The physical FX-6100/GTX 960 screen passes **24/24**: DX12 and Vulkan each pass 6/6 in both repeats,
at the original 640x480 VSync, 600 measured + four warm-up frames, p95/p99 <= 1000/30 ms and
maximum <= 2000/30 ms. All counts, coverage, queries, GPU samples and native identity match the
prior screen. Complete local MSVC Development and canonical Shipping builds pass; related CTest
passes 33/33 without skips. [Evidence](../../Engine/TwoD/evidence/Windows-M6-DX12Pacing-2026-10-18/acceptance.md) retains every formal frame, rejected trials,
exact source/binary identities and the original failed-screen reference. Final-head full hosted
Linux/Shipping and other platform gates are recorded on [PR #644](https://github.com/jimlee1972/Nexora/pull/644).

The earlier 72.3556 ms DX12 event was not reproduced in fixed diagnostic repeats; its unique
same-frame cause remains untraced, and current Windows policy prevents kernel CPU tracing.
This screen does not prove that historical cause eliminated. Product minimum specification,
integrated UI/VFX, allocator/VRAM and sustained acceptance remain open.
**M6 NOT ACCEPTED; 9/11 (82%), P0 6/8 unchanged.**

## Integrated 10k UI/VFX and physical 30-minute soaks (2026-10-18)

✅ An opt-in benchmark mode and fixed-plan runner preserve the original six-case `Measure` body,
all 10,000 CPU updates/query shapes and the original 30 FPS limits. The integrated native pass adds
256 animated translucent VFX, two Sprite atlases/batches and actual GameplayUi panels/buttons/text
(120 UI quads/commands). Full-pool/immutable-atlas recycling, codec reload and a touched/freed 8 MiB
scoped allocation repeat while mixed/sparse layouts alternate. Real-time simulation, production
art, asynchronous disk IO and physical input are not inferred from these deterministic fixtures.

✅ Both hardware backends complete 1,800 measured seconds after 1,200 warm-up frames, with zero
lost frame/completed GPU samples and balanced teardown. The formal suite retains 210,785 raw
frames and exact source/executable identities. Original workload non-timing outputs match the
preceding screen. Local Development/Shipping builds and 34/34 related tests pass without skips.

Strict performance is **NOT ACCEPTED**: original screens are 23/24 (DX12 11/12, Vulkan 12/12),
integrated short screens 3/4 and soaks 1/2. DX12 soak has 95,701 measured frames, cycle p99/max
28.4662/62.8658 ms. Vulkan has 95,772, p99 27.7955 ms but max **134.9232 ms**, exceeding the
unchanged 66.6667 ms maximum. DX12 original mixed-10k repeat 1 p99 is 39.3942 ms; Vulkan integrated
repeat 2 p95/p99 is 33.5469/49.2560 ms. No failed rows, cold measured frames or spikes are excluded.

Vulkan frame 24,415 has CPU 14.9771 ms, Draw 5.6583 ms and EndFrame **113.6250 ms**, with neither
reload nor resource polling. Matching submission 24,416 has GPU interval **0.448736 ms**, observed
two frames later. This isolates the broad submit/present boundary, not an individual API or OS/driver
cause. Native API/thread-cycle/scheduling tracing is tracked by `nexora-j4q.2.6`; the earlier DX12
72.3556 ms cause remains separately untraced under `nexora-j4q.2.4`.

✅ All six integrated resource screens meet fixed sampled caps/growth limits. Both soaks retain
two native buffer reallocations, 161 balanced scoped allocations/frees and zero rejected textures.
DX12 private-memory first/last-five-minute median growth is 0.3984 MiB; Vulkan is -0.0078 MiB;
local GPU usage median growth is zero for both. Post-frame RSS/commit/WDDM budget usage and the
explicit loading allocation do not establish full allocator accounting or physical VRAM residency.

[Complete plan, results, failed frames and compressed raw evidence](../../Engine/TwoD/evidence/Windows-M6-IntegratedSoak-2026-10-18/acceptance.md).
[Tool contract](../../Tools/TwoD/README.md#integrated-10k-load-and-sustained-native-validation).
This record supplements the earlier successful 24/24 screen without replacing its historical verdict.
Keep **M6 NOT ACCEPTED; 9/11 (82%), P0 6/8 unchanged**. Product minimum specification and broader
allocator/production/device/clean-host gates remain open. At the user's explicit request, the harness
and retained evidence were merged in [PR #646](https://github.com/jimlee1972/Nexora/pull/646); this
did not accept M6 or change any failed result. The revised 5,000-object gate above is NOT RUN.

## ALSA configuration lifetime repair (2026-10-20)

✅ `GameplayAudio` now opens Linux output with a private ALSA configuration tree/update
record and releases both after PCM close and before library unload, including failed opens.
It preserves system/user/`ALSA_CONFIG_PATH` selection without replacing or freeing another
client's global configuration. The real-adapter regression matrix covers 65 opens per process
for null, missing-device, malformed, absent-file and system configurations, both alone and
with an external live PCM/retained global reference. A second Nexora owner remains usable.
Local ASan/UBSan/LSan passes all ten cases (650 opens), without suppression; old-code leak
and external-reference failures are retained in the [evidence](../../Engine/Runtime/evidence/ALSA-Lifetime-Linux-2026-10-20/acceptance.md).
Hosted sanitizer CI retains the same matrix's stdout/stderr. Silent/null output does not add
physical audibility or device-recovery acceptance. M6/M7 and 9/11 (82%) remain unchanged.

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
