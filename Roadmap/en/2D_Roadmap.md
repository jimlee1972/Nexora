# Nexora 2D Engine Roadmap

> Version: v1.0 | Status: planned, engine capabilities only | Updated: 2026-10-13

> **Progress: 0% acceptance** (0/11 capability milestones accepted, equal milestone count).
> No dedicated 2D acceptance evidence is recorded here. Existing foundation delivery does not
> complete a 2D milestone; this does not mean the engine has no reusable support.

## 1. Scope and baseline

Deliver the engine capabilities for sprite-heavy, top-down single-player Windows PC applications
on the V1 foundation. Orthographic cameras, Unlit materials, instancing, VFX sprites, UI/text,
input, audio, asynchronous assets, serialization and packaging have existing plans or slices.
Their complete 2D integration still needs verification. VFX/UI sprites do not establish a scene
Sprite Renderer, and Jolt 3D does not establish native 2D collision support. Inventory unconfirmed
support before implementing replacements. Full V2/V3 delivery is not a prerequisite.

Scope: reusable rendering, assets, queries, runtime lifecycle, profiling, packaging and authoring.
Gameplay rules, combat, spawning logic, progression and game content are outside this roadmap.

## 2. Capability milestones

P0 establishes the usable runtime and evidence. P1 improves production workflows; Tilemap is
conditional. Every row remains unaccepted until its complete condition passes; only then use `✅`.

| ID | Priority | Capability | Required scope | Acceptance condition | Current assessment |
| --- | --- | --- | --- | --- | --- |
| 2D-M0 | P0 | Scene Sprite Renderer | Atlas regions, pivot, flip, tint/alpha, sorting layers and stable Y ordering; existing transforms/orthographic cameras | Native captures verify UVs, pivot, flips, transparency and overlapping order; save/reopen retains fields | Complete dedicated specification not confirmed |
| 2D-M1 | P0 | Frame animation | Frame duration/rate, looping, switching and animation events, separate from skeletal animation | Frame/event traces verify loop boundaries, switching, pause/resume and large time steps without duplicate events | Dedicated sprite animation path not confirmed |
| 2D-M2 | P0 | 2D asset pipeline | Slicing, deterministic Atlas packing, import/sampling settings, transparent-edge handling and stable references | Import/cook/reimport retains references; native sampling has no edge bleed; invalid input preserves published assets | General foundation exists; 2D workflow needs inventory |
| 2D-M3 | P0 | Lightweight 2D collision/queries | Circle/AABB overlap, filters, range queries, spatial index and swept checks for fast movement | Compare indexed results with an independent brute-force oracle; cover boundaries, filtering, fast motion and insert/remove | Jolt 3D is not proof of this capability |
| 2D-M4 | P0 | Bulk lifecycle/updates | Batch create/recycle, generational identity, allocation control, data-oriented updates and fixed-step scheduling | Repeated cycles reject stale handles, preserve safe teardown and report allocation/frame spikes under load | Architecture has direction; capacity needs measurement |
| 2D-M5 | P0 | High-volume Sprite rendering | Atlas batching/instancing, visibility culling, transparency ordering and overdraw control via existing RHI | Native captures preserve order across batches; sparse/overlapping workloads retain draw counts and CPU/GPU timings | General rendering exists; 2D path needs verification |
| 2D-M6 | P0 | Profiling/stress acceptance | CPU/GPU frame time, memory, draw counts, allocation spikes and transparent overlap | Reproducible mixed engine workload meets declared 60 FPS/frame-time budgets on the specified minimum PC; retain traces/configuration | Actual capacity must be measured |
| 2D-M7 | P0 | Windows runtime/Shipping acceptance | Input, UI/text, audio, async loading, serialization/save, cooked assets and standalone packaging | Clean Windows machine runs cooked 2D validation app without Editor/developer dependencies; input, reload, save/reopen and shutdown pass | Existing subsystem slices need end-to-end 2D acceptance |
| 2D-M8 | P1 | 2D scene authoring | Sprite placement, pivot/order editing, collision visualization and orthographic Editor preview | Edit-preview-Undo/Redo-save-reopen preserves authored scene; isolated Play does not change authoring World | Complete graphical 2D workflow not confirmed |
| 2D-M9 | P1 | Atlas/animation tools | Slice/Atlas preview, packing diagnostics, frame editing and playback/event preview | Cooked output matches runtime rendering; Undo/Redo, invalid data and reimport are verified | Dedicated tools not confirmed |
| 2D-M10 | P1, conditional | Tilemap | Layers, painting, collision metadata, chunk loading/culling | If selected, paint-Undo/save/reopen and chunk-boundary render/query tests pass; otherwise record workflow decision and defer | Needed only for a tile-based map workflow |

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
  dependencies belong in `Config/Modules/modules.json` with feature options. This documentation
  change adds no modules and modifies no existing runtime contracts.

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

Count these 11 milestones equally for this roadmap only; report P0 as 8/8 separately if useful.
Deferred Tilemap stays in the denominator until a reviewed scope revision changes it. Planning
completion does not change V1 foundation, Editor or focused AI-plan percentages. Retain detailed
acceptance beside the module/tool or in evidence directories; Beads remains the durable task tracker.

Deferred: full 2D rigid-body physics, multiplayer, additional platforms and advanced authoring
beyond the listed P1 workflows.

- [Roadmap index](../ReadMe.md)
- [V1 Complete Plan](Cross-platform_3D_Engine_V1_Complete_Plan_v1_2.md)
- [V1 AI Implementation Plan](Cross-platform_3D_Engine_V1_AI_Implementation_Technology_and_System_Plan_v1_2.md)
- [Graphical Editor Roadmap](Editor_Roadmap.md)
- [Focused Roadmaps AI Plan](Focused_Roadmaps_AI_Implementation_Plan.md)
- [Renderer](../../Engine/Renderer/README.md), [Runtime](../../Engine/Runtime/README.md) and
  [Editor contracts](../../Engine/Editor/README.md)
- [Traditional Chinese edition](../zh-TW/2D_Roadmap.md)
