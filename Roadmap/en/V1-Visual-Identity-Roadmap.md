# Nexora V1 Visual Identity Showcase Roadmap

> Version: v0.2
>
> Date: 2026-10-04
>
> Status: ✅ VIS-M0–M2 and VIS-M4–M5 accepted (5/7); VIS-M3 art and VIS-M6 target-hardware acceptance remain open.
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

### Confirmed art direction

✅ The user approved the following generated preview in this conversation on 2026-10-04. Use it as the art-direction reference for asset selection, composition, materials, and lighting.

![Approved stylized ruins courtyard concept; AI-generated, not an engine screenshot](../art/V1-Visual-Identity-Concept.png)

- **Mood and palette:** a calm, mysterious sanctuary; golden late-afternoon sunlight, warm ochre sandstone, cool blue shadows, muted green foliage, and one turquoise rune accent.
- **Focal point:** a weathered stone ring with aged-bronze fittings around a floating faceted crystal, raised on a carved pedestal. The central rune mechanism defines the scene's identity.
- **Architecture and composition:** modular broken arches, thick columns, worn paving, and ceramic props; a readable foreground, central device in the midground, and ruins behind it.
- **Material treatment:** stylized PBR with intentional silhouettes, stone surface detail, varied roughness, restrained metal highlights, and matte ceramics. Free assets must be adapted to this look rather than dictate a different style.
- **Motion and effects:** gentle foliage wind, vegetation backlighting, rune activation, sparse floating particles, and controlled emissive bloom.
- **V1 scope:** start with one compact courtyard and reduced architecture/foliage density. The concept's reflective puddles, distant waterfall, extensive background, and camera blur are not initial acceptance requirements; water/reflections remain later extensions.

The preview was created with the image-generation tool and is a visual target, not a rendered Nexora scene, production model, texture map, or performance result. The original PNG is retained unchanged; SHA-256: `ad4cd12a0331e9c30b4a2e54d2773ee0718a63bdf2791a65743f38e1bd7a9b3b`.

The user also requested free online models and textures. Prioritize freely downloadable assets with redistribution permission, preferably CC0; avoid paid packs. See the bilingual [free-asset shortlist and source checks](../art/Free-Asset-Sourcing.md). Art-direction confirmation alone did not complete VIS-M0; adopted-asset and baseline acceptance is now recorded in section 11.

## 4. Milestones

✅ VIS-M0 is accepted within its baseline scope; the remaining milestones are planned. Mark milestones complete only after their acceptance gates pass and evidence is retained.

| ID | Work | Visible outcome | Acceptance |
| --- | --- | --- | --- |
| ✅ VIS-M0 | Art direction, asset inventory, greybox, fixed shots, initial performance capture | Complete composition and tour route | Three shots work; a representative asset loads through Import → Cook → Bundle → Runtime; retain baseline screenshots |
| ✅ VIS-M1 | Shared PBR shader integration, multiple materials, normal/ORM/emission, tangents, IBL, linear color, HDR output | Material close-up | Materials differ under identical light; orbit preserves correct normals/reflections; missing-map fallbacks work; no duplicate gamma conversion |
| ✅ VIS-M2 | Directional shadow map, PCF, bias controls, stylized tonal separation and shadow tint | Lighting wide shot | Moving objects update shadows; the fixed route has no obvious flicker, shadow acne, or floating shadows; retain lighting comparisons |
| VIS-M3 | Central ruins, ground and surrounding content; exposure, tone mapping, color, restrained bloom | First finished hero image | Composition survives hidden UI; close shots have detail and wide shots a clear subject; target-hardware screenshots pass visual review |
| ✅ VIS-M4 | Vegetation wind, alpha cutout, back-light transmission, rune particles, device activation | Living courtyard | Continuous wind; correct edges and occlusion; pause/replay works; particle and transparency costs are observable |
| ✅ VIS-M5 | 90–120-second visual tour, free camera, feature comparisons, screenshot mode | Complete viewing and interaction | Replayable tour; clear feature differences; default view has only necessary controls; screenshots omit diagnostic overlays |
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

## 10. VIS-M0 first implementation slice (2026-10-05)

The native Showcase now accepts `--scene=courtyard` / `9`, with an original engineering blockout
for paving, broken architecture, the central ring device, and ceramic/vegetation placeholders.
`B` cycles three deterministic shot framings; `F4` hides all diagnostic UI while preserving the
Offscreen → Main → UI → Present schedule. The crystal placeholder uses the existing active
cooked/bundled mesh, with a procedural fallback when the asset pipeline is compiled out.
The report retains shot/UI state and representative asset provenance.

Inventory: [engineering sources and replacements](../../Apps/Showcase/content/Courtyard-Greybox.md).
This begins VIS-M0; final free-asset adoption, art review and target-hardware acceptance remain open.
Material/motion shot names describe intended future demonstrations, not delivered PBR or wind.

✅ First-slice Linux validation: 84/84 Development tests with no skips; native fixed-shot replay,
UI hiding/restoration and representative asset loading verified. [Baseline evidence](../../Apps/Showcase/evidence/VIS-M0-Linux-Greybox-2026-10-05/acceptance.md).

## 11. VIS-M0 accepted baseline (2026-10-05)

✅ Three CC0 KayKit architectural meshes and the palette atlas are adopted through the Runtime
asset generation, with original source/license/hash/replacement inventory retained and packaged.
Three native fixed shots, exact replay and diagnostic-free startup are verified. Performance
reports expose warm-up, average FPS, P95/P99, process CPU time and peak resident bytes; GPU timestamps
remain unavailable. Three idle-host lavapipe runs provide an initial software baseline, not a
GTX 960 performance promise. Full Linux Development passes 87/87 without skips; isolated
Development packaging passes. [Evidence](../../Apps/Showcase/evidence/VIS-M0-Linux-AdoptedAssets-2026-10-05/acceptance.md).

VIS-M0 acceptance does not accept final materials/art, VIS-M1–M6 or target-hardware visuals.

## 12. VIS-M1 material submission foundation (2026-10-05)

Per-batch opaque colors and texture generations are implemented for Vulkan, DX12 and Metal.
Courtyard geometry now selects six material slots, preserving adopted atlas UVs for architecture.
Native material pixel and rejection/recovery tests cover Vulkan and Metal source paths; execution
results are recorded separately. [Ownership and PBR/HDR direction](ADR-0004-Showcase-Materials-HDR.md).
This is an in-progress VIS-M1 slice. Shared PBR, tangents, normal/ORM/emission, IBL, linear color and
HDR acceptance remain open; milestone progress remains 1/7.

✅ Material binding slice: Linux 87/87 without skips and native material pixels passed.
[Evidence](../../Apps/Showcase/evidence/VIS-M1-Linux-MaterialBindings-2026-10-05/acceptance.md).

## 13. VIS-M1 shared direct-light PBR (2026-10-05)

The native entry now imports `Nexora.Common` and generates pinned Slang 2026.18 SPIR-V/HLSL/MSL
artifacts. Vulkan, DX12 and Metal bind base/normal/ORM/emission maps, camera and per-material factors.
Renderer generates unit tangents and mirrored UV handedness without changing Runtime mesh wire data.
Courtyard geometry caches owning prepared vertices and defaults to shared direct-light PBR;
`P` switches the same fixed scene to Lambert and back. Linear factors, single sRGB map decoding,
shared ACES and a single target transfer establish the color path. Targets remain RGBA8.

Native Vulkan cases check independent emission, normal-map lighting, ORM, missing-map fallbacks,
single sRGB decoding, mirrored model tangent handedness, frame reuse, direct/offscreen draws and
resize/re-upload. Equivalent Metal tests and Windows courtyard captures exercise the host paths in CI.
IBL resources, floating-point HDR composition and final material/reflection acceptance remain open.
The milestone remains 1/7; direct-light PBR alone does not complete VIS-M1.

✅ Linux direct-light PBR slice: 91/91 without skips, Monolithic Shipping build and isolated package launch passed.
[Evidence](../../Apps/Showcase/evidence/VIS-M1-Linux-SharedPBR-2026-10-05/acceptance.md).

## 14. VIS-M1 linear color filtering (2026-10-05)

PBR base/emission use hardware sRGB texture views with decoding before filtering; normal/ORM and
legacy Lambert retain linear UNORM sampling. Vulkan, DX12 and Metal own paired views under the
existing immutable-generation/fence lifetime. Shared shaders no longer decode sampled color a
second time. Native black/white midpoint tests compare base/emission against linear 0.5 factors
and verify that ORM and legacy sampling remain linear. A calibrated per-frame emission marker rejects
stale X11 presentation pixels. Vulkan candidate allocation completes before recording texture copies;
failed candidates release their storage before publication.

✅ Linux color-filtering slice: 92/92 without skips, all nine rooms and fixed-camera/comparison replay
passed. Three retained software-renderer runs measured 47–48 FPS; hardware performance remains open.
[Evidence](../../Apps/Showcase/evidence/VIS-M1-Linux-LinearColor-2026-10-05/acceptance.md).
IBL and floating HDR remain open and the milestone stays 1/7.

## 15. VIS-M1 cooked IBL resources (2026-10-05)

The pinned CC0 Forest Slope HDRI produces bounded linear RGBA16F irradiance, seven GGX prefilter
levels and a split-sum BRDF LUT with deterministic 128-sample Hammersley integration. Source, license,
attribution, converter and derived hashes are retained. Each cooked float asset depends on cooked
source/license/conversion metadata in the same verified Runtime generation. Vulkan, DX12 and Metal
bind actual float environment resources through seven explicit sampled resources and an 80-byte
private material packet. Public C++ consumers rebuild; NXAB/stable C/Zig schemas remain compatible.

Native fixtures verify radiance above 1.0, diffuse/metal separation, roughness levels, reflection
rotation/view/seam, IBL disable and resize/re-upload; descriptors reject bad mip counts, half values,
missing resources and texture-type aliases. `O` compares IBL/direct light and restores the fixed shot.

✅ Linux IBL slice: 94/94 without skips, Monolithic Shipping build, isolated headless package launch,
nine-room interaction, all fixed shots and exact PBR/IBL comparison restoration pass. Three retained
lavapipe runs measured 35–37 FPS; physical performance remains open.
[Evidence](../../Apps/Showcase/evidence/VIS-M1-Linux-IBL-2026-10-05/acceptance.md).
Native Windows/Metal execution is required through PR CI. Targets remain RGBA8; floating HDR
composition and final VIS-M1 material acceptance remain open. Milestone progress stays 1/7.

## 16. VIS-M1 floating HDR composition (2026-10-05)

✅ Linux native shared PBR renders into frame-owned RGBA16F and Main applies exposure, shared ACES
and one display transfer before UI. Pixel oracles distinguish radiance 4/1, exposure 1/0.125/0.25,
UI color invariance, frame reuse and resize. Legacy direct/offscreen RGBA8 and Lambert comparisons
remain covered. E switches courtyard exposure with exact fixed-camera restoration. External graph
metadata identifies RGBA16F and ShaderRead without exposing resources or silently changing native
RHI triangle allocation. Public C++ consumers rebuild; stable C/Zig and NXAB remain compatible.

✅ Linux Development full gate: 95/95 without skips. Native Windows DX12 and Metal pixel oracles,
Windows Vulkan package replay and Shipping packaging require the exact PR head CI before milestone
acceptance. Evidence is retained under `Apps/Showcase/evidence/VIS-M1-Linux-HDR-2026-10-05`.
This slice does not accept bloom, final art, physical visuals or the GTX 960 budget. VIS-M1 remains
open until cross-platform acceptance; milestone progress is still 1/7.

## 17. VIS-M2 directional shadows and tonal separation (2026-10-05)

✅ Linux native PBR records a directional shadow prepass into frame-owned R32Float/D32Float
resources and applies shared four-tap PCF, normal/slope bias, stylized ramp and shadow/light tint.
The 30-submission pixel oracle covers horizontal/vertical caster movement, partial PCF edges,
map resolution/reuse, camera/resize changes, disabled shadows and styled/neutral light.
F6/G/brackets expose bounded comparisons; exact fixed-camera restoration is tested.
✅ Linux Development full gate passes 95/95 without skips. The private packet grows to 208 bytes
and main binds eight sampled maps; stable C/Zig and persistent content schemas remain unchanged.
Evidence: `Apps/Showcase/evidence/VIS-M2-Linux-Shadows-2026-10-05`. Windows DX12/Metal execution
and Windows Vulkan package replay require exact-head CI; fixed-route visual review and final
hardware budget remain open. This Linux slice alone does not accept VIS-M2.

## 18. VIS-M1 cross-platform acceptance

✅ Shared PBR/material maps, tangent/orbit normals, cooked IBL, hardware sRGB filtering and
linear RGBA16F composition are accepted. HDR PR #322 head
`79270136894a7ed00ebf7d48db5044d1ebb5ecc0` passed all 18 checks in Build 1430
(run 37278711512), including Windows DX12/Metal native pixel gates, exact generated shaders,
Windows DX12/Vulkan isolated packages and Linux full/native interaction gates. Merge:
`ef8c305f30e4f01e97ad2cbbd812bd5d0171c83c`. Prior pending notes in sections 12–16 describe
those intermediate slices; this acceptance supersedes them. Progress is 2/7.
Final art, physical target-hardware visuals and the GTX 960 performance budget remain later gates.

## 19. VIS-M2 cross-platform acceptance

✅ Shadow PR #324 head `a66f568af38954b665b16abc7fd15814bfc9ab7a` passed all 18 checks
in Build 1439 (run 37280731307). DX12/Metal native movement/PCF/bias/reuse/tint pixels and
Windows DX12/Vulkan comparison/package replay passed. Native fixed captures retain exact
restoration; Linux wide and shadow-off images were reviewed. Merge:
`dcd44ad1bfed81ce60ee59108ca80461003c79de`. Earlier pending slice notes are superseded;
progress is 3/7. This does not accept final VIS-M3 art or target-hardware performance.

## 20. VIS-M3 hero art, bloom and color implementation

Original Runtime-loaded crystal/detail maps, a broken stone/bronze rune device, round plinths,
ceramic profiles, sky and reframed fixed shots are implemented. GPU tone composition adds bounded
thresholded neighborhood bloom and shared color grade before UI; K and G retain comparisons.
Native pixel cases verify halo spread, threshold rejection, disable, grayscale and UI invariance.
✅ Linux Development passes 96/96 without skips; Shipping packaging and native 34-frame pixels pass.
Evidence is retained under `Apps/Showcase/evidence/VIS-M3-Linux-Hero-2026-10-05`.
Cross-platform execution and final target-hardware hero/close-up visual approval remain open.
Original maps remain distinct from the adopted online CC0 resources. VIS-M3 is not yet accepted.

## 21. VIS-M4 living courtyard implementation

Shared GPU wind, main/shadow alpha cutout and thin-leaf transmission are implemented across native
adapters. Original cooked leaf/mote masks replace vegetation placeholders; activation adds 48
emissive cutout motes and a pulsing crystal/rune device. Space pauses explicitly, R replays time
and fixed camera, N/M compare wind/transmission. F4 remains a UI-only control. Native fixtures
cover cutout shadow agreement, wind movement, exact replay and back-light color. Cross-platform
CI and native motion/interaction evidence remain required before VIS-M4 acceptance.
✅ Linux Development passes 96/96 without skips; Shipping, native 42-frame pixels and pause/replay/comparison interaction pass. Evidence: `Apps/Showcase/evidence/VIS-M4-Linux-Living-2026-10-05`.

## 22. VIS-M5 tour and exploration implementation

The native default entry is the courtyard with a compact control strip. `--tour=visual` renders a
100-second five-segment camera route (wide, material approach, orbit and activation finale),
pauses camera/effects together and supports deterministic replay. The existing 210-second
engineering tour remains separate. C enables actual eye translation and mouse look; B/R restore
fixed framing. H exposes named comparisons; F1–F3 opt into diagnostics and F4 removes all UI.
A full headless CLI timeline gate is separate from native tour recording/interaction evidence.
Native execution/video and exact-head cross-platform CI remain required before VIS-M5 acceptance.

✅ Linux Development: 97/97; Shipping package verification and native free-camera/fixed-shot restoration pass. Evidence: `Apps/Showcase/evidence/VIS-M5-Linux-Tour-2026-10-05`. PR #327 living effects passed all 18 checks (run 37288122626) and merged. Target-hardware final acceptance remains open.

## 23. VIS-M6 quality and optimization implementation

Actual Basic / Standard / High quality selects 512/1024/2048 shadows, 32/64/128 foliage
quads and 24/48/96 active motes. Basic disables IBL/bloom; Standard/High preserve bounded
post-processing. Q changes real geometry and GPU settings; reports retain effective settings.
One original Runtime-loaded gradient replaces 24 sky batches. Emission-only sky/motes bypass
lighting and shadow casting; native fixtures verify bright non-casters and lit receivers.
Sequential repeatable measurements, matching-version packages/video and exact-head platform CI
are in progress. Physical DX12/Vulkan art approval and confirmed performance budget remain open.

## 24. Living/tour acceptance and matching-version release evidence

✅ VIS-M4: PR #327 head 9040f0c7644db448f5c0e819dc7d1fc15f315f3d passed all 18 checks
(run 37288122626); the retained actual native tour plus 25-second living finale and exact
pause/replay/comparison captures supply continuous motion evidence.
✅ VIS-M5: PR #328 head 4c944fca96bee9863b652940cc41f00a408ae406 passed all 18 checks
(Build 1471, run 37292103114), including native DX12/Vulkan isolated packages and Metal pixels.
Merge f016aa2a51b04475ae47657a6af042a0282b02ee. Stable native baseline acquisition preserves all
exact comparison assertions. Earlier pending notes in sections 21–22 are superseded.

The VIS-M6 release code 3047d5c68de939c6ead3bfceb8a47c5d6ccf3524 passes Linux 97/97,
Shipping/Full isolated native acceptance and 43 native pixel cases. One packaged executable
produces the 100.7-second video, five screenshots and nine quality measurements. Software
Vulkan averages: Basic 27.45–28.20 FPS; Standard 17.56–17.86; High 12.99–13.11. P95/P99,
CPU and memory are retained; GPU timestamps are unavailable. Evidence and physical instructions:
`Apps/Showcase/evidence/VIS-M6-Linux-Release-2026-10-05`. Compiled ZIP remains a workspace
deliverable; hashes/manifests are tracked. The production sources are unchanged by later evidence
commits. Progress is 5/7: physical hero/material art review and the confirmed DX12/Vulkan hardware
budget remain open. Earlier physical GTX 960 reports do not accept the new effects.

## 25. Golden-hour background, HDR and reference parity

The user expanded the approved concept target to include background scenery, a skybox,
matching light direction, visible HDR bloom/glow, wind and animation. The earlier reduced
background scope no longer bounds this work. Actual native geometry now includes mountain
ridges, upper ruins, arcades, cypresses, rooted hanging vines, rippling reflective puddles and
animated falls. A six-face camera-centred skybox and HDR solar disc share golden-hour authoring
with the key light and linear IBL. Bevelled/fluted original stone and 256x256 detail maps replace
the simpler shapes; emissive rune rails, crystal hover/rotation and orbiting splinters are live.

The shared HDR compositor now supports bounded depth-aware focus before bloom/ACES, preserving
sharp post-composite UI. Native tests require visible focus/bloom differences, exact restoration
and deterministic animation pause/replay. Shipping and exact-head DX12/Vulkan/Metal CI remain
required before merging. This is implementation progress, not concept-image parity acceptance.
The procedural art is not identical to the reference; crystal refraction and richer
asset detail remain unresolved. Keep VIS-M3/M6 open and progress at 5/7 until reference art and
target hardware/budget acceptance are supported by matching-version evidence.

✅ Linux Development passes 97/97 (54.23s, no skips), including 45 native PBR frames with
depth-aware focus/UI checks and exact focus/bloom/wind/pause/replay input captures.

✅ Golden-hour source `97b20cdeb1a6e6ea035d639d2752fc9daa8c4144` passes full Linux 97/97
(53.10s), Shipping/Full and Minimal, isolated native launch, 45 PBR frames and a real 100.27s
movie with nine same-executable quality runs. Evidence: `Apps/Showcase/evidence/VIS-Background-HDR-Linux-2026-10-05`.
Software FPS: Basic 11.94–12.16; Standard 7.31–7.59; High 6.47–6.78. These are neither GPU
timestamps nor physical target acceptance. The user goal/reference parity remains open.

### Reference material iteration

The follow-up art source adds retained image-assisted sandstone/ivy authoring with deterministic
cooking, original foreground paving, chamfered ring wedges, larger reference framing, wind-bent
teal pennants and decorated ceramic vessels. Linux development validation passes 97/97 tests;
matching-source release captures are retained. This remains VIS-M3 refinement, with final scene
reflection refinement, crystal optics and final reference parity open; the milestone count remains 5/7.


### Planar mirror iteration

The courtyard now submits bounded horizontal mirror instances for the focal device, crystal,
vessels, foliage, pennants and sky. Shared Slang clipping removes receiver top/underside faces
inside two water regions; source-space wind, light and shadows remain coherent. V toggles the
reflection and Basic retains the cheaper environment-only water path. Four additional native
PBR fixtures check floor occlusion, source-driven movement and exact restoration (49 total).
Full release and cross-platform evidence must precede merge. Crystal refraction, shoreline
refinement and concept-image parity remain open; this does not accept VIS-M3 or VIS-M6.


### Tinted crystal transparency iteration

The crystal and floating splinters use tinted linear HDR surface blending, depth-testing against
opaque geometry and retaining nearest-layer camera distance for focus. U switches transparency
for native comparisons. Six extra PBR fixtures check opaque/half/zero coverage, restoration,
focus and colored transmission (55 total). Full Shipping and cross-platform gates must pass
before merge. Refraction and final visual fidelity remain open; VIS acceptance remains 5/7.
