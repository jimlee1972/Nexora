# HD-2D Lighting, Soft Shadows and Selective Bloom Plan

> Baseline: Nexora `8c4aa989`; Beads epic `nexora-m6m`. Engineering **5/7 (71%)**; physical acceptance **NOT_RUN**.

## Goal and boundaries

Deliver dynamic local lights, dynamic soft occlusion and selective bloom through the generic 2D Player on DX12, Vulkan and Metal. Gameplay and diagnostics remain Zig; generic engine code may use existing C++. V1 uses flat lighting; normal maps, 3D height shadows, HDR displays and full Editor authoring are excluded.

## Work items and progress

Beads is the task source of truth; this table is a review snapshot. Engineering progress counts accepted children / 7, excluding physical acceptance. Partial implementation earns no completion credit. Only evidenced accepted items receive ✅. Keep the epic open until PC acceptance passes.

| ID | Beads | Delivery | Depends on | Status / evidence |
| --- | --- | --- | --- | --- |
| ✅ HD2D-01 | `nexora-m6m.1` | Public contract and Zig bindings | — | Contract/snapshots delivered; [Linux evidence](../../Engine/TwoD/evidence/HD2D-01-Linux-2026-10-31.md) |
| ✅ HD2D-02 | `nexora-m6m.2` | Linear RGBA16F pipeline on three backends | 01 | Accepted [PR #735](https://github.com/jimlee1972/Nexora/pull/735), squash `ed0e51e4`; 41 CI checks and three-backend native evidence |
| ✅ HD2D-03 | `nexora-m6m.3` | Dynamic local point lights | 02 | Accepted PR #736, squash `40c96072`; current-head native DX12/Metal and Linux gates passed |
| ✅ HD2D-04 | `nexora-m6m.4` | Dynamic occlusion and soft shadows | 03 | Accepted PR #737 / `0845c078`; final-head native DX12/Metal and full Linux CI passed |
| ✅ HD2D-05 | `nexora-m6m.5` | Selective emission and bloom | 02 | Accepted PR #738 / `a28f1001`; all34 final-head checks and native53-frame DX12/Vulkan/Metal CPU readbacks passed |
| HD2D-06 | `nexora-m6m.6` | Zig diagnostic and native validation | 03, 04, 05 | IN_PROGRESS — real Zig four-size native diagnostic, GPU timestamps and map view; full/native CI pending |
| HD2D-07 | `nexora-m6m.7` | SDK 0.2.0 and game integration | 06 | IN_PROGRESS — isolated0.2.0 candidate/consumer and same-game Zig preparation; immutable release/game lock pending |
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

HD2D-04 local preparation: GPU angular shadow maps and independent CPU oracle; [evidence](../../Engine/TwoD/evidence/HD2D-04-Linux-2026-10-31.md). IN_PROGRESS; no acceptance credit before dependent PR/native CI.

HD2D-05: [local evidence](../../Engine/TwoD/evidence/HD2D-05-Linux-2026-10-31.md); IN_PROGRESS.

HD2D-03 accepted: PR #736 / `40c96072`; current-head Windows job `124365092041` and macOS job `124365092312` passed actual native pixels and full desktop CTest. HD2D-04 PR #737 and this dependent stage still require their own native CI. Engineering 3/7; physical PC NOT_RUN.
HD2D-05 follow-up fixes pipeline-off retirement: completed-slot legacy frames release offscreen owners; appended HUD stays protected and re-enable recreates targets. Local/native gate evidence remains linked; no acceptance credit before CI.

HD2D-04 accepted: PR #737 / `0845c078`, tested `dd256588`. Windows52 native frames/425 CTests, Metal52/400, Linux483; all34 final-head checks completed (10 legitimate documentation-push skips). HD2D-05 preliminary head9e2323ce passed41 checks and native53-frame DX12/Metal readbacks. Its dependency-only rebase preserves the runtime tree; final rebased-head CI remains required. Engineering4/7; physical PC NOT_RUN.

HD2D-06: real reusable Zig scene, native GPU map view, completed-slot per-pass timestamps, four-size exported Player runs and explicit unavailable observations. [Local evidence](../../Engine/TwoD/evidence/HD2D-06-Linux-2026-10-31.md); IN_PROGRESS, no additional acceptance credit.

HD2D-03 acceptance receipt: PR #736 / `40c96072`; current-head Windows job `124365092041` (425 CTests) and macOS job `124365092312` (400 CTests) passed actual native pixel readbacks. Physical PC remains NOT_RUN.

HD2D-06 input evidence follow-up replaces fixed-time synthetic click injection with actual Zig-sampled neutral/down acknowledgements. A retained dependent full-gate failure motivated the change; three complete four-size native repetitions passed, with the original exactly-one activation/camera isolation assertions intact. Acceptance still requires final-head CI.

HD2D-06 aligns moving Sprite poses with the last successfully submitted light/caster snapshot, including zero-size restoration; UI reload defers to the next update start. Full Linux297/289pass/eight skips/zero failures111.82s and Shipping passed. No ABI/host semantics change or acceptance credit before CI.

HD2D-06 four-size aggregate watchdog now covers four unchanged90s case bounds plus export/teardown (420s), justified by a real Windows144-frame consumer taking about53s. Pixel/resource/input assertions and product performance gates are unchanged. Full Linux297/289pass/eight skips/zero failures105.54s and Shipping passed; final-head CI still required.

HD2D-06 macOS job124420121268 retained a configuration failure: the three Python diagnostic tests had an empty interpreter because the older discovery block excludes Apple. Explicitly require the interpreter at the diagnostic registration boundary. Metal native55-frame CPU pixel readback itself passed; three diagnostic tests remain unaccepted until retry, with no assertions/skips weakened.

HD2D-06 interpreter-scope repair passed full Linux Development297/289pass/eight skips/zero failures112.54s with actual four-size Zig Vulkan and synchronization validation, plus Shipping. macOS actual diagnostic retry remains required.

HD2D-06 second macOS diagnostic failure retained from dependent job124428232886: Python/asset generation now run, but ExportGame rejects dylib and supports only Windows/Linux. Add explicit Development-only macOS dyld graph staging, inherited RPATH resolution, copied import relocation/ad-hoc signing and complete inventory. Six portable graph/failure fixtures pass; actual Metal Zig startup remains a required retry. No macOS Shipping distribution or weaker assertions claimed.

HD2D-06 Development-only macOS export passed full Linux298/290pass/eight skips/zero failures109.56s and Shipping, including six portable dyld fixtures. Actual Mac binary rewrite/signing/native Zig retry remains pending.

HD2D-06 retains dependent macOS mimalloc job124435568972 fixture failure: temporary /var resolves to /private/var, but mocked import keys used the unresolved root. A real Linux symlinked TMPDIR reproduced all six KeyErrors; canonicalizing only the fixture root passes all six unchanged tests. Exporter/runtime and assertions are unchanged; actual Mac retry remains required.

HD2D-06 fixture-root canonicalization passed full Linux298/290pass/eight skips/zero failures106.22s and Shipping; actual Mac retry still required.

HD2D-05 accepted PR #738 / `a28f1001`, tested0cd1f27a with identical squash tree: Windows native53/full426 tests, Metal53/401, Linux53/484; all34 final-head checks completed successfully (10 legitimate documentation-push skips). HD2D-06 dependency rebase preserves runtime source; current-head native/full CI and Mac fixture retry still required. Engineering5/7; physical PC NOT_RUN.

HD2D-07 preparation packages explicit-mask tools and licensed reusable Zig diagnostics into a versioned 0.2.0 candidate. The independent source-free consumer builds/runs real headless/native DX12 diagnostics and offline rebuilds. Game default NX00 hash is unchanged; local same-product Vulkan two-cycle F9 evidence passed. Native candidate CI, dependency acceptance, actual release hash and final game lock remain required; no acceptance credit or publication claimed.

HD2D-07 release provenance now binds the independent consumer receipt to the actual candidate ZIP SHA-256 and source commit, native DX12 diagnostic and offline rebuild. CI cleanup is scoped to this delivery’s four exact owner branches, only obsolete Build runs; current-head push/PR, main, foreign actor and similarly named branches are preserved. All 47 cleanup guard tests passed.

HD2D-07 [local preparation evidence](../../Tools/SDK/evidence/HD2D-07-Linux-2026-10-31.md): final full Linux gate 297 registered / 289 pass / eight skips / zero failures, Shipping passed. Candidate Windows consumer/publication/game lock remain pending; engineering stays 3/7.
