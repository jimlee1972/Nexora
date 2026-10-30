# HD-2D Lighting, Soft Shadows and Selective Bloom Plan

> Baseline: Nexora `8c4aa989`; Beads epic `nexora-m6m`. Engineering **1/7 (14%)**; physical acceptance **NOT_RUN**.

## Goal and boundaries

Deliver dynamic local lights, dynamic soft occlusion and selective bloom through the generic 2D Player on DX12, Vulkan and Metal. Gameplay and diagnostics remain Zig; generic engine code may use existing C++. V1 uses flat lighting; normal maps, 3D height shadows, HDR displays and full Editor authoring are excluded.

## Work items and progress

Beads is the task source of truth; this table is a review snapshot. Engineering progress counts accepted children / 7, excluding physical acceptance. Partial implementation earns no completion credit. Only evidenced accepted items receive ✅. Keep the epic open until PC acceptance passes.

| ID | Beads | Delivery | Depends on | Status / evidence |
| --- | --- | --- | --- | --- |
| ✅ HD2D-01 | `nexora-m6m.1` | Public contract and Zig bindings | — | Contract/snapshots delivered; [Linux evidence](../../Engine/TwoD/evidence/HD2D-01-Linux-2026-10-31.md) |
| HD2D-02 | `nexora-m6m.2` | Linear RGBA16F pipeline on three backends | 01 | IN_PROGRESS — three native implementations; Linux software Vulkan pixel evidence, Windows/Metal CI pending |
| HD2D-03 | `nexora-m6m.3` | Dynamic local point lights | 02 | IN_PROGRESS — native point-light preparation, Zig identities and Linux oracle; depends on 02 acceptance |
| HD2D-04 | `nexora-m6m.4` | Dynamic occlusion and soft shadows | 03 | NOT_RUN |
| HD2D-05 | `nexora-m6m.5` | Selective emission and bloom | 02 | NOT_RUN |
| HD2D-06 | `nexora-m6m.6` | Zig diagnostic and native validation | 03, 04, 05 | NOT_RUN |
| HD2D-07 | `nexora-m6m.7` | SDK 0.2.0 and game integration | 06 | NOT_RUN |
| HD2D-PC | `nexora-m6m.8` | Physical Windows acceptance | 07 | NOT_RUN |

## Implementation contracts

- **01 — API:** add independently negotiated `NexoraGameplayTwoDEffectsApiV1` and an appended services getter, preserving old prefixes and Sprite ABI. Expose capabilities/limits/state and atomic copied submissions for ambient, lights, polygons, Sprite effects, bloom and strength. Owner thread only; publish at next frame boundary. Use entity/pool identity, never sorted indices. Reject invalid/over-limit batches without replacement; old hosts report UNSUPPORTED.
- **02 — Pipeline:** default off, separate 2D RGBA16F scene/emission, stable Y order and straight alpha. Bloom precedes fixed SDR mapping and HUD; encode sRGB once. Preserve viewport, resize/zero-size handling and GPU-completion retirement. Do not substitute 3D DrawScene. Report unsupported capabilities explicitly; Zig may select the existing fallback.
- **03 — Lights:** circular point position/linear RGB/intensity/radius/smooth attenuation, ambient, receiving flags and masks; viewport/radius culling. Up to 32 lights, 8 shadowed; reject excess atomically. Limits are not a 60FPS promise.
- **04 — Shadows:** closed simple polygons, <=256 polygons, <=16 vertices each, <=2048 total edges; reject intersections/degeneracy/nonfinite data. 1024 angular nearest-distance samples per shadowed light, bounded filtering, caster/receiver groups and bias. Shared backend contract and CPU oracle cover seams, contact and self-shadow. No physically accurate area-light claim.
- **05 — Emission:** optional linear companion mask/color/intensity associated with existing Atlas/region through separate metadata. Match slicing/padding/repacking without changing old Atlas/Clip wires. Preserve ordered transparent occlusion; half-resolution start and <=5 bloom levels. Include export/inventory/deferred residency/retirement. HUD excluded. 100/75/50% reduces decoration, retaining independent hazard outlines; off uses legacy rendering.
- **06 — Diagnostic:** real Zig module in generic Player, moving lantern, overlapping colored lights, moving occluder/soft shadow, selective emitter and white HUD. Toggle effects/strength/outlines/shadow maps. Explicit DEV ONLY assets.
- **07 — SDK:** immutable 0.2.0 retaining 0.1.0; promote the independent-consumer-tested candidate ZIP bytes. Update game lock/hash, preserve project Zig and one-click/offline builds. Add diagnostic mode to the same product, preserving default NX-00. No engine checkout/MSVC consumer requirement.

## Validation and delivery

Retain commands, versions, outcomes and failed retries for each stage. Run full `cmake --preset linux-development`, `cmake --build --preset linux-development`, `ctest --preset linux-development`; public/linkage changes also run `linux-shipping`. Require shader, module graph, ABI and old/new host regression checks.

Native DX12/Vulkan/Metal readbacks compare a CPU oracle with fixed documented tolerances. Cover attenuation/groups/occlusion/soft edges/seams, non-emissive whites and HUD exclusion, transparency/flips/camera rotation/viewport; 1080p, 21:9, 4:3 and odd sizes, DPI/resize/minimize/restore. Verify replacement/unload/transitions/teardown and record real GPU pass timing, draw calls and memory peaks. Missing timing is unavailable, never fabricated zero.

Windows/macOS native evidence comes from their CI hosts; unexecuted checks are not PASS. Engineering acceptance is not formal product acceptance. The separate PC gate requires i5-8400/GTX 1050 Ti/8GB, formal art/input and sustained frame timing, currently NOT_RUN. Existing SP-01–07 and M6/M7 remain unchanged.

Merge this plan first, then dependent stage PRs. Squash merge only after applicable local and current-head CI passes; update both roadmaps, Beads and evidence each time. Preserve source/commit/PR/test traceability and never publish private hardware identifiers.

References: [2D roadmap](2D_Roadmap.md), [color and existing fallback](../../Engine/TwoD/ColorAndFallback.md).

HD2D-01 delivers negotiation, Zig wrappers and owning snapshots only. Native capability defaults to zero and Player registration is pending; no rendered effect is claimed. Applicable CI success gates this PR merge.

HD2D-01: [PR #734](https://github.com/jimlee1972/Nexora/pull/734), squash `3628a62e`, all 41 current-head checks passed. One existing Windows VM timeout was retained and passed on retry without assertion changes. HD2D-02 does not increase accepted progress until native backend gates pass.

HD2D-02: [Linux evidence](../../Engine/TwoD/evidence/HD2D-02-Linux-2026-10-31.md).

HD2D-03: [local evidence](../../Engine/TwoD/evidence/HD2D-03-Linux-2026-10-31.md); IN_PROGRESS.
