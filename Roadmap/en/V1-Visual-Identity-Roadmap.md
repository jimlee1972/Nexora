# Nexora V1 Visual Identity Showcase Roadmap

> Version: v0.1
>
> Date: 2026-10-04
>
> Status: planning draft; VIS-M0 through VIS-M6 have not been implemented or accepted (0/7).
>
> Theme: a stylized ruins courtyard.
>
> Traditional Chinese edition: [V1 Visual Identity Showcase Roadmap](../zh-TW/V1-Visual-Identity-Roadmap.md).

## 1. Goal

Build an explorable stylized ruins courtyard that establishes Nexora's visual identity through materials, lighting, and environmental motion. With all text and counters hidden, one screenshot or ten seconds of footage should still communicate the engine's visual capabilities.

This plan follows the [V1 Visual Showcase Demo Long-Term Plan](V1-Visual-Showcase-Long-Term-Plan.md), reusing NexoraShowcase, its asset pipeline, tours, and packaging foundations. VIS milestones are tracked independently: they do not change existing V1 portable-contract completion or establish final V1 platform acceptance.

### Three visual strengths

| Strength | Visual treatment | How viewers inspect it |
| --- | --- | --- |
| Material quality | Stone, metal, ceramics, emissive runes | Compare under the same lighting; orbit to inspect reflection, roughness, and normal detail |
| Lighting style | Warm sunlight, cool shadows, controlled tonal separation | View spatial depth in a wide shot; compare basic and stylized lighting |
| Living environment | Wind-driven vegetation, activating rune device, moving particles | Play, pause, and replay the motion sequence |

## 2. Existing foundations and integration gaps

These are planning inputs, not acceptance evidence for new VIS milestones.

- Native presentation already supports indexed geometry, directional light, depth testing, basic textures, hardware instancing, and an Offscreen → Main → UI → Present RenderGraph.
- The shared shader library includes PBR/IBL, Stylized, Anime, vegetation wind/transmission, shadow, and post-process helpers, with compilation contracts for representative shaders.
- Showcase already visualizes procedural terrain, vegetation geometry, animation blending, CPU deformation, and particle positions.
- The native showcase path still uses basic materials. Shared helpers need real scene integration and visual acceptance. Multiple materials, normal/ORM maps, IBL, HDR, and real shadows are the main engineering gaps.
- Existing scene batches share lighting, texture, and base material. Multiple-material integration requires a new scene/material binding boundary.
- Recorded GTX 960 frame times are observations from simple scenes, not performance commitments for this plan.

Sources: [Shader library](../../Shaders/README.md), [Renderer](../../Engine/Renderer/README.md), [Presentation](../../Engine/Presentation/README.md), [Showcase](../../Apps/Showcase/README.md).

## 3. Scene and shots

Use a courtyard with a central statue/rune device, surrounded by stone walls, metal fixtures, ceramic ornaments, and vegetation. The palette combines warm sunlight, cool shadows, and one rune accent color.

| Shot | Content | Capability demonstrated |
| --- | --- | --- |
| Material close-up | Stone carving, metal, ceramics, rune surfaces | Normals, roughness, reflections, emission, and detail density |
| Courtyard wide shot | Angled sunlight, architectural shadows, vegetation, foreground/midground/background | Lighting style, real shadows, spatial depth, and composition |
| Motion finale | Camera approaches the device; vegetation moves, runes activate, particles flow | Environmental motion, effect timing, and replayability |

VIS-M0 records asset authors, sources, licenses, redistribution conditions, and production cost. Track engineering models separately from final art assets.

## 4. Milestones

All milestones are planned. Mark them complete only after their acceptance gates pass and evidence is retained.

| ID | Work | Visible outcome | Acceptance |
| --- | --- | --- | --- |
| VIS-M0 | Art direction, asset inventory, greybox, fixed shots, initial performance capture | Complete composition and tour route | Three shots work; a representative asset loads through Import → Cook → Bundle → Runtime; retain baseline screenshots |
| VIS-M1 | Shared PBR shader integration, multiple materials, normal/ORM/emission, tangents, IBL, linear color, HDR output | Material close-up | Materials differ under identical light; orbit preserves correct normals/reflections; missing-map fallbacks work; no duplicate gamma conversion |
| VIS-M2 | Directional shadow map, PCF, bias controls, stylized tonal separation and shadow tint | Lighting wide shot | Moving objects update shadows; the fixed route has no obvious flicker, shadow acne, or floating shadows; retain lighting comparisons |
| VIS-M3 | Central ruins, ground and surrounding content; exposure, tone mapping, color, restrained bloom | First finished hero image | Composition survives hidden UI; close shots have detail and wide shots a clear subject; target-hardware screenshots pass visual review |
| VIS-M4 | Vegetation wind, alpha cutout, back-light transmission, rune particles, device activation | Living courtyard | Continuous wind; correct edges and occlusion; pause/replay works; particle and transparency costs are observable |
| VIS-M5 | 90–120-second visual tour, free camera, feature comparisons, screenshot mode | Complete viewing and interaction | Replayable tour; clear feature differences; default view has only necessary controls; screenshots omit diagnostic overlays |
| VIS-M6 | Optimization, quality tiers, DX12/Vulkan validation, packaging, video, performance report | Shareable V1 Visual Showcase | Fixed-route performance meets the confirmed budget; both backends pass visual acceptance; isolated package launches; video/screenshots/report match one version |

### Technical decisions before VIS-M1 expansion

- Multiple-material binding, mesh/material identities, and GPU resource lifetimes.
- Vertex tangents, normal-map conventions, ORM channel packing, and texture color spaces.
- IBL environment sources, irradiance/prefilter/BRDF LUT generation and packaging.
- HDR render targets, exposure, tone mapping, and UI output composition order.
- Resource retirement on resize, scene changes, asset reload, and shutdown.

Record decisions in the relevant contract or ADR before expanding scene content. Successful helper compilation does not replace native pixel tests or target-hardware visual acceptance.

## 5. Execution order and ownership

Main sequence: VIS-M0 → VIS-M1 → VIS-M2 → VIS-M3 → VIS-M4 → VIS-M5 → VIS-M6.

Performance capture begins at VIS-M0 and continues with each effect; VIS-M6 provides final convergence. Art preparation can start after VIS-M0, but material appearance is confirmed after VIS-M1 stabilizes color and shading.

| Area | Primary responsibility |
| --- | --- |
| Renderer/shared shader library | Materials, lighting, shadows, post-processing, RenderGraph, backend-neutral descriptions |
| RHI/native adapters | GPU resources, pipelines, bindings, barriers, backend operations |
| Presentation | Windows, swapchains, output composition, resize, surface lifecycle |
| Runtime/asset pipeline | Scenes, material resources, import, cook, packaging, version lifetimes |
| Showcase/content | Courtyard, cameras, tour, interaction, feature comparisons, display UI |

New visual features should reuse shared rendering capabilities rather than duplicate shading in Showcase or each Presentation backend. Actual module/public-interface changes still require architecture and compatibility review.

## 6. Three deliveries

| Delivery | Scope | Outputs |
| --- | --- | --- |
| Look Preview | VIS-M0–VIS-M3 | Finished fixed hero shot, material close-up, free orbit, baseline screenshots |
| Living Scene | Add VIS-M4 | 20–30-second motion demonstration and replayable environmental effects |
| V1 Visual Showcase | Finish VIS-M5–VIS-M6 | Complete tour, interactive executable, highlight video, screenshots, performance report |

Estimate the schedule after VIS-M0 inventories assets and VIS-M1 engineering gaps are confirmed. This draft makes no calendar-date commitment.

## 7. Performance and platform acceptance

### Candidate performance baseline

- GTX 960 at 1280×720 and 60 FPS (16.7 ms per frame) is a candidate baseline, to confirm or revise after VIS-M1/VIS-M2 measurements.
- Record CPU, GPU, driver, backend, build ID, resolution, quality, VSync, and display refresh rate.
- Benchmarks use fixed cameras, random seeds, and effect timelines, with warm-up and repeated runs. Disable VSync/FPS caps for performance capture; presentation mode may use VSync.
- Report average FPS, P95/P99 frame time, valid separate CPU/GPU timings, and memory use. Mark GPU timing unavailable until GPU timestamps exist; do not substitute overall frame time.
- Basic/Standard/High tiers control shadows, IBL resources, vegetation density, particles, and post-processing. Hold other conditions constant in feature comparisons.

### Acceptance scope

- Windows DX12 is the first complete visual-delivery target.
- Windows Vulkan independently validates the same scene, reviewing color, materials, shadows, and effects for backend differences.
- Linux Vulkan automation can validate contracts, native pixels, interaction, and lifecycle. Software-rasterizer results do not establish GTX 960 performance.
- Full physical Metal visuals/interaction remain pending and deferred under existing instructions. VIS-M6 acceptance is explicitly limited to the DX12/Vulkan showcase; it does not establish final cross-platform V1 acceptance.

Retain fixed-shot images, effect comparisons, build provenance, and test results at every stage. Implementation changes run the repository's full Linux Development gate; linkage-boundary changes also validate Shipping. Target-platform native visuals have separate acceptance.

## 8. Later extensions

Water, SSR, volumetric fog, complete characters, GPU skinning, large worlds, and high-object-count stress scenes are later candidates. Finish materials, lighting, and environmental motion first, then select extensions based on visual benefit and measured cost.

## 9. Final completion criteria

- Hiding diagnostics preserves composition and a consistent art style.
- Three shots separately demonstrate materials, lighting, and environmental motion.
- Tour, free exploration, comparisons, and screenshot mode work.
- Visuals and performance come from the delivered native executable version.
- The package launches in isolation and includes asset sources and licensing information.
- Every completed milestone has traceable acceptance evidence; unaccepted platforms and effects retain explicit status.
