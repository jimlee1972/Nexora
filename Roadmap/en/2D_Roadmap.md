# Nexora 2D Engine Roadmap

> Version: v1.2 | Status: cloud delivery accepted; PC host gates open | Updated: 2026-10-14

> **Progress: 82% capability acceptance** (9/11, rounded; P0 6/8). M0–M5 and M8–M10
> have portable/Linux functional evidence. M6 physical minimum-PC 60 FPS and M7 clean Windows
> runtime/Shipping remain **NOT RUN**. This is not full Windows PC product acceptance.

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
| 2D-M6 | P0 | Profiling/stress acceptance | CPU/GPU frame time, memory, draw counts, allocation spikes and transparent overlap | Reproducible mixed engine workload meets declared 60 FPS/frame-time budgets on the specified minimum PC; retain traces/configuration | Cloud implementation verified; physical Windows gate NOT RUN |
| 2D-M7 | P0 | Windows runtime/Shipping acceptance | Input, UI/text, audio, async loading, serialization/save, cooked assets and standalone packaging | Clean Windows machine runs cooked 2D validation app without Editor/developer dependencies; input, UI/text rendering, audio playback, async-load completion/failure, reload, save/reopen and shutdown pass | Cloud implementation verified; physical Windows gate NOT RUN |
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
corruption rejection and no Editor/development engine library dependency. Windows clean-machine
input/controller, UI/text, actual audio, async failure/reload/save, native DX12/Vulkan, display/DPI
and shutdown are **NOT RUN**. Windows backend compilation in CI is not that acceptance.

Retained [Linux acceptance record](../../Engine/TwoD/evidence/Linux-2026-10-14/acceptance.md)
contains commands, platform scope, captures and workload CSV. See [module contracts](../../Engine/TwoD/README.md)
and [tools](../../Tools/TwoD/README.md). Generated reports remain under `build/<preset>/artifacts/`.
The cloud executable work is complete; Beads keeps physical target-host follow-ups open.


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

[Retained Zig/Linux acceptance](../../Engine/TwoD/evidence/Zig-Linux-2026-10-14/acceptance.md)
records the complete suite, initial GUI failures/retry and Shipping scope.

- [Roadmap index](../ReadMe.md)
- [V1 Complete Plan](Cross-platform_3D_Engine_V1_Complete_Plan_v1_2.md)
- [V1 AI Implementation Plan](Cross-platform_3D_Engine_V1_AI_Implementation_Technology_and_System_Plan_v1_2.md)
- [Graphical Editor Roadmap](Editor_Roadmap.md)
- [Focused Roadmaps AI Plan](Focused_Roadmaps_AI_Implementation_Plan.md)
- [Renderer](../../Engine/Renderer/README.md), [Runtime](../../Engine/Runtime/README.md) and
  [Editor contracts](../../Engine/Editor/README.md)
- [Traditional Chinese edition](../zh-TW/2D_Roadmap.md)
