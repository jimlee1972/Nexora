# Nexora 2D Engine Roadmap

> Version: v1.6 | Status: cloud delivery accepted; PC host gates open | Updated: 2026-10-16

> **Progress: 82% capability acceptance** (9/11, rounded; P0 6/8). M0–M5 and M8–M10
> have portable/Linux functional evidence. M6 local DX12 screening exceeded its budgets; physical minimum-PC acceptance remains open.
> M7 clean Windows runtime/Shipping remains **PARTIAL / NOT ACCEPTED**. The rebuilt current-main VM
> package passes CPU, native software DX12, Zig instancing and human keyboard movement; full device/product
> integration and Vulkan remain open. This is not full Windows PC product acceptance.

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
| 2D-M6 | P0 | Profiling/stress acceptance | CPU/GPU frame time, memory, draw counts, allocation spikes and transparent overlap | Reproducible mixed engine workload meets declared 60 FPS/frame-time budgets on the specified minimum PC; retain traces/configuration | Local DX12 screening recorded; minimum-PC gate NOT ACCEPTED |
| 2D-M7 | P0 | Windows runtime/Shipping acceptance | Input, UI/text, audio, async loading, serialization/save, cooked assets and standalone packaging | Clean Windows machine runs cooked 2D validation app without Editor/developer dependencies; input, UI/text rendering, audio playback, async-load completion/failure, reload, save/reopen and shutdown pass | Rebuilt VM CPU/software DX12/Zig and human keyboard slice passed with existing Runtime; full product/device gates and Vulkan open; M7 NOT ACCEPTED |
| ✅ 2D-M8 | P1 | 2D scene authoring | Sprite placement, pivot/order editing, collision visualization and orthographic Editor preview | Edit-preview-Undo/Redo-save-reopen preserves authored scene; isolated Play does not change authoring World | ✅ Portable/Linux capability acceptance; evidence below |
| ✅ 2D-M9 | P1 | Atlas/animation tools | Slice/Atlas preview, packing diagnostics, frame editing and playback/event preview | Cooked output matches runtime rendering; Undo/Redo, invalid data and reimport are verified | ✅ Portable/Linux capability acceptance; evidence below |
| ✅ 2D-M10 | P1, conditional | Tilemap | Layers, painting, collision metadata, chunk loading/culling | Implemented painting, layers/collision metadata and chunk loading/culling pass Undo/save/reopen and chunk-boundary render/query tests; a decision to defer leaves this milestone unaccepted | ✅ Portable/Linux capability acceptance; evidence below |

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

Before declaring 60 FPS support, fix minimum CPU/GPU/RAM, Windows version, backend/driver,
resolution, build and frame pacing. Fix visible/updated/colliding counts, Atlas/material count,
Sprite pixel coverage/overlap, animation rate, VFX/UI load and loading churn. Hardware/workload
budgets are currently **TBD**; object count alone is not a performance claim.

Use deterministic synthetic objects, animations, moving query shapes, VFX and UI, without game
mechanics. Include baseline, high-count, transparent-overlap and allocation/loading spikes.
Retain warm-up/duration, average/p95/p99/max frame time, separate CPU/GPU intervals when available,
memory peaks and dropped samples. A 60 FPS target has a 16.67 ms frame budget; define allowed
spikes before acceptance. Software rasterizers establish correctness only.

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
software Vulkan results do not accept the physical 60 FPS gate.
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
admission and integrated Game preview/reload remain open; milestone counts are unchanged.
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
This is a project asset prerequisite; main host API attachment is described below, while Game preview/reload remain open.
Scoped cook/freeze admission is supplied by the following capture owner. Milestone counts are unchanged. [Evidence](../../Tools/Build/evidence/EditorEDM1-TypedTwoDContent-Linux-2026-10-16.md).

✅ `ProjectTwoDPlayAssets` now cooks and freezes the selected current project Atlas/Clip closure
with project UUID/root, Content generation/revision, writer/access/recovery, exact source/sidecar
and dependency admission. Owning observations revalidate before activation and retain frozen data
after project destruction. Main host API attachment is described below; integrated Game preview/reload remain open;
ED/2D milestone counts are unchanged. See the [capture contract](../../Engine/Editor/README.md#project-scoped-cooked-2d-play-capture)
and [evidence](../../Tools/Build/evidence/EditorEDM3-ProjectTwoDPlayAssets-Linux-2026-10-16.md).

✅ The main Editor Play start path now captures/revalidates project Atlas/Clip Content and binds
optional 2D gameplay services to its actual cloned World and selected active scene. Fresh Sprite/
collider metadata and a bounded instance pool survive through module Stop/Destroy; generic subtree
despawn forgets every destroyed ID. Full module replacement and failed load cleanup are covered
by 32 CPU clone lifetimes alongside the existing 3D host. Game View 2D rendering, authored Sprite
persistence, Tilemap binding, seamless hot reload and physical acceptance remain open. No ED/2D
milestone count changes. [Contract](../../Apps/Editor/README.md#optional-2d-services-in-project-play). [Evidence](../../Tools/Build/evidence/EditorEDM3-MainPlayTwoDServices-Linux-2026-10-16.md).

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

✅ The main Editor composes scene/pool sprites plus gameplay UI in Game View using the frozen
project asset owner. Focus/acquisition-gated actions, native audio control/retry and project-scoped
storage remain valid through Stop/Destroy, and native texture retirement is host-owned. CPU and
Linux Vulkan pixels validate 32 Play lifetimes; real Zig Shipping export and independent module
restart/corruption checks validate the general Player path. See the [acceptance record](../../Tools/Build/evidence/Zig-Project-Player-Linux-2026-10-16.md)
and [Windows checklist](../../Tools/TwoD/WindowsAcceptance.md).

Formal art/fonts/game rules and physical Windows sound, XInput devices, focus/DPI, clean-machine
runtime and minimum-PC acceptance require the actual project/target host. Authored Sprite
persistence, Editor Tilemap binding and seamless reload remain open. M6/M7 and **9/11 (82%)**
are unchanged. Hosted MSVC CI and software Vulkan are not physical product acceptance.
