# Editor script workflow and Sprite integration delivery

[繁體中文](../zh-TW/Editor_Workflow_Delivery_Plan.md) · [Epic #741](https://github.com/jimlee1972/Nexora/issues/741)

Updated: 2026-11-12. Baseline: `8c4aa989ee7443cc0b2129d54f0f0f77c73d2b49`.
Initial documentation publication base: `67db45d23e631b0e05b42c23716245a34ae996d5`.
ED-WF-04 integration base: main `a21c1ca2293e4200de8dcaa4b892ffef34687c9c`, including the appearance update.
Acceptance snapshot: **5/9 work orders (55.6%)**; ED-WF-01 merged in [PR #751](https://github.com/jimlee1972/Nexora/pull/751), ED-WF-02 in [PR #754](https://github.com/jimlee1972/Nexora/pull/754), ED-WF-03 in [PR #756](https://github.com/jimlee1972/Nexora/pull/756), ED-WF-04 in [PR #758](https://github.com/jimlee1972/Nexora/pull/758); ED-WF-06 is accepted through [PR #772](https://github.com/jimlee1972/Nexora/pull/772) and its prerequisite slices.
The Epic records subsequent accepted work and its merged PR/evidence. Complete graphical Editor
acceptance remains **0/8**; this plan does not accept physical-device or clean-host gates.

## Existing foundations and remaining work

The Editor already exposes a script checkbox/manifest, Build and Cancel through the shared
producer. Runtime already owns V3 module generations and optional save/load callbacks. At the plan
baseline, the typed Zig game adapter did not publish migration callbacks. ED-WF-04 ✅ supplies bounded typed state, actual registered-work barriers and strict reload
transactions after PR #758 acceptance. The macOS panic TLS and deprecated section-API failures were
repaired and revalidated; original unload assertions remain. Sprite sources already have immutable
metadata, source-aware C++ extraction and NX2S schema 3 scene persistence. These are foundations,
not evidence of the complete workflow below. Existing CLI and `.bat` development remains usable.

The common CMake/Zig SDK owns generated entry/ABI wiring. Authored projects retain scenes,
assets, scripts, manifests and settings; users must not maintain a second SDK or build pipeline.
SortingGroup, nine-slice/Tiled rendering, masks and advanced animation state machines are excluded.

## Work orders

| ID | GitHub | Depends on | Required acceptance | State |
| --- | --- | --- | --- | --- |
| ED-WF-01 | [#742](https://github.com/jimlee1972/Nexora/issues/742) | None | Bilingual plan, capability baseline, issue graph and validation/evidence mapping published | ✅ PR #751 |
| ED-WF-02 | [#743](https://github.com/jimlee1972/Nexora/issues/743) | 01 | Persistent script Build settings, real GUI Build/Build & Run, cancellation/progress/diagnostics | ✅ PR #754 |
| ED-WF-03 | [#744](https://github.com/jimlee1972/Nexora/issues/744) | 01 | Source-free runtime output; development receipts and publication inventory kept outside it | ✅ PR #756 |
| ED-WF-04 | [#745](https://github.com/jimlee1972/Nexora/issues/745) | 01 | Transactional Runtime/Zig migration; failed replacement retains the old module and World | ✅ PR #758 |
| ED-WF-05 | [#746](https://github.com/jimlee1972/Nexora/issues/746) | 02, 04 | Background source compilation and stateful Play reload without Stop/Play | In progress; integration unaccepted |
| ED-WF-06 | [#747](https://github.com/jimlee1972/Nexora/issues/747) | 01 | Compatible C/Zig source APIs and cooked Player dependency closure | ✅ PRs #767, #769, #770, #772; [acceptance](../../Tools/Build/evidence/EditorWorkflow/ED-WF-06.md) |
| ED-WF-07 | [#748](https://github.com/jimlee1972/Nexora/issues/748) | 06 | Source indexing/Inspector/Undo/save/reopen/Play/Build/reimport in the real Editor | In progress; source index, Inspector/persistence, module Play and Build support |
| ED-WF-08 | [#749](https://github.com/jimlee1972/Nexora/issues/749) | 04, 06, 07 | Source animation anchors, live pool instances and valid handles survive replacement | Pending |
| ED-WF-09 | [#750](https://github.com/jimlee1972/Nexora/issues/750) | 02, 03, 05, 07, 08 | Integrated native acceptance and a verified, extracted Windows Editor ZIP | Pending |

✅ The independent ED-WF-06 registry slice (`nexora-whr.6.1`) adds a separately versioned C/Zig
Sprite source registration table and immutable C++ resolver with Atlas dependency validation and
bounded retirement. Its [Linux evidence](../../Tools/Build/evidence/EditorWorkflow/ED-WF-06-SourceRegistry-Linux-2026-11-12.md)
records the executed registry gates and limits.

✅ The source API slice (`nexora-whr.6.2`) adds size/version-checked C/Zig source Sprite/pool
descriptors, compatible optional table negotiation and canonical Player registry/extraction. A real
SDK-built Zig module runs after export, relocation and source deletion, with native pixels checking
negative-scale pool geometry and source/Atlas replacement. See [API/Player evidence](../../Tools/Build/evidence/EditorWorkflow/ED-WF-06-SourceAPI-Linux-2026-11-12.md).
Editor source workflow and source-animation reload remain open.
The combined [ED-WF-06 acceptance](../../Tools/Build/evidence/EditorWorkflow/ED-WF-06.md)
includes the catalog and native-repack gates below. Complete Editor milestones remain **0/8**.

✅ The independent catalog slice (`nexora-whr.6.3`) adds cooked Sprite sources as catalog kind 4,
preloaded/deferred Player admission and canonical source export. Existing opaque kind 3, C/Zig
table layouts, explicit Atlas-first loading and immutable ownership remain unchanged. The
[Linux evidence](../../Tools/Build/evidence/EditorWorkflow/ED-WF-06-SpriteCatalog-Linux-2026-11-12.md)
records real relocated-package admission and corruption/closure rejection. A real SDK-built Zig
file-catalog game integrates the source API accepted in PR #769 and verifies scene/pool native
pixels after relocation, source deletion and replacement. Editor authoring remains open in ED-WF-07; the combined ED-WF-06 acceptance below
raises workflow completion to **5/9**, with graphical Editor still **0/8**.

Each work order records exact commands/results and limitations in
`Tools/Build/evidence/EditorWorkflow/ED-WF-XX.md`. The existing
[ED-WF-01 baseline](../../Tools/Build/evidence/EditorWorkflow/ED-WF-01.md) distinguishes planning,
implementation and accepted delivery. Later evidence links are added only when those files exist.
The [ED-WF-05 technical baseline](../../Tools/Build/evidence/EditorWorkflow/ED-WF-05.md) records
verified SDK lifecycle, compiler ownership and resource-isolation gaps. The first SDK lifecycle
slice is in progress with scoped Windows evidence; compiler coordination, Editor resource isolation
and complete acceptance remain pending. It does not change the accepted count.
PR #762's first Linux CI candidate is unaccepted after the Ubuntu 26/GNU 15.2 runner change:
existing Runtime, Prefab, scene-session/save and reflected/comparison GUI fixtures trigger
dangling-pointer warnings. Named owning-fixture repairs preserve all assertions, payloads and
warnings. Failure-only full compiler diagnostics retain the original failed gate; repaired-head
full Cloud/CI is still required. Editor resource isolation and integrated acceptance remain open.
The subsequent native build clears those compiler failures, but the `201d73e1` hosted display
gate remains unaccepted (231/247 pass,16 fail): injected F5 also carries Alt and Play never starts.
The [native input repair](../../Tools/Build/evidence/EditorWorkflow/ED-WF-05-Input-CI-Failure.md)
uses validated physical X11 function-key codes and retains all original runtime/pixel/deadline
checks. Repaired-head native/full gates remain required; no accepted-count change.
The [ED-WF-02 evidence](../../Tools/Build/evidence/EditorWorkflow/ED-WF-02.md) and [Cloud receipt](../../Tools/Build/evidence/EditorWorkflow/ED-WF-02-Cloud.json) record accepted PR #754: 42 successful CI checks, 490/490 Cloud tests and Shipping build, with initial environment/local-capture failures retained. Beads synchronization recovered by adopting existing history; Epic `nexora-whr` and children `.1` through `.9` map to the linked GitHub work orders.
The accepted [ED-WF-03 evidence](../../Tools/Build/evidence/EditorWorkflow/ED-WF-03.md), [Cloud receipt](../../Tools/Build/evidence/EditorWorkflow/ED-WF-03-Cloud.json) and [CI inventory](../../Tools/Build/evidence/EditorWorkflow/ED-WF-03-CI.json) record PR #756: native GUI/core passes, 490/490 Cloud tests and Shipping, all 42 CI checks and unchanged merged tree. ED-WF-04 is accepted in PR #758.
The accepted [ED-WF-04 evidence](../../Tools/Build/evidence/EditorWorkflow/ED-WF-04.md),
[Cloud receipt](../../Tools/Build/evidence/EditorWorkflow/ED-WF-04-Cloud.json) and
[CI inventory](../../Tools/Build/evidence/EditorWorkflow/ED-WF-04-CI.json) record 495/495 Cloud tests,
Shipping builds, native Windows/macOS proof, all 42 successful CI checks and merged tree equality.
Real Zig retains 32 accepted reloads and 224 expected rejections. Editor coordination, Sprite
authoring/animation and final delivery remain ED-WF-05, ED-WF-07, ED-WF-08 and ED-WF-09.
Accepted count is the number of children with acceptance evidence and a merged delivery PR,
divided by nine. Accepted rows use `✅`; pending or blocked work stays unmarked.

## Implementation contracts

- Build uses the current shared producer/publisher and trusted SDK tools. Persist compatible
  mode/manifest settings; legacy settings retain SceneOnly. Save/Discard/Cancel remains explicit.
  Cancellation, stale captures and failed tools preserve the previous successful output.
- `Build.commands`, `Build.sources`, `Build.tools` and publication-only inventory move to engine
  user data, outside read-only authored projects and runtime distributions. Expose an owning build
  receipt to callers. Preserve verified replacement, foreign-file/alias protection and rollback;
  verify old inventory-bearing output before migration. Runtime-needed metadata and licenses stay.
- Keep V3 `save_state`/`load_state`; add paired typed `saveState`/`loadState` support in the SDK.
  State is bounded/versioned data, never raw native pointers. Missing safe migration keeps the old
  generation running and reports restart required instead of silently resetting it.
- Before state capture/switch, stop new dispatch and wait for actual registered job/callback
  completion. Isolate candidate host/World/resource side effects and roll back on rejection.
  Retiring the old module must not delete transferred Play-owned state. Final Stop still drains
  and destroys its complete session; no callback/function pointer outlives its module generation.
- Reuse watcher debounce and compile in the background while old gameplay continues. Use unique
  DLL generation copies and accept only the current project/source/Play generation. Rapid edits,
  Stop, close and project changes cancel/drain work. Shipping contains no compiler/autoreload path.
- Preserve legacy C/Zig descriptor layouts; source APIs are capability-gated and size-checked.
  Keep source-free formats and NX2S schema 3. Editor and Player resolve the same immutable
  Sprite/Atlas closure without authored source paths or current-working-directory dependencies.
  Reimport/repack publishes a new generation while retained frames keep previous resources alive.
- Complete source animation and pool publication without moving Pivot/world-sort anchors or
  invalidating live session-owned handles. Cross-session stale handles remain rejected.

## Validation and rollout

Real GUI Build covers Unicode/space paths, saved settings, dirty-scene decisions, cancel, compiler
failure, stale source and repeated publication. Reopen and relocate the source-free output, launch
with no arguments and ordinary process flags, require one responsive game window without console
or child launcher, and close normally. Do not substitute a hidden-console test for GUI entry.

Use real Zig libraries for at least 32 successive reloads, checking counter/entity/scene/animation/
pool continuity, ABI and migration/start failure, pending jobs/callbacks, cancellation and old-code
retirement. Sprite tests cover trim, Pivot, Flip, negative scale, repacking, missing/corrupt closure,
legacy C/Zig consumers and feature-OFF builds. Compare actual native Editor Play and relocated
Player pixels; helper/CPU tests alone do not accept those paths.

For every implementation batch, run the full gate and retain its real exit codes and test inventory:

```sh
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development
```

ABI/linkage/lifetime changes additionally run `cmake --preset linux-shipping` and
`cmake --build --preset linux-shipping`. Windows/macOS native evidence is separate from Linux;
unavailable physical clean-host/device checks stay open. Failed runs and conditional skips remain
in the evidence, not replaced by an unqualified green summary.

Start each batch from current remote main, retain one working branch and preserve unrelated work.
Inspect the diff/results, commit/push and create an English conventional PR; merge with squash only
after applicable validation and all required CI succeed. Never bypass checks/protection or merge
another contributor's PR. Update the Epic, issue and bilingual roadmap with verified progress.
Keep the repository README reader-facing and unchanged unless an overall milestone changes.

Follow the Beads skill with pinned CLI and external task paths. On synchronization failure preserve
data and record the exact blocker, without initializing/replacing/forcing shared history. GitHub
work orders remain visible; failed Beads synchronization is never reported as successful.

## ✅ Editor Sprite source index and Play snapshots (2026-11-12)

The independent ED-WF-07 foundation (`nexora-whr.7.1`) admits immutable `.nx2sprite` payloads in
project indexing and synchronous/queued reimport, retains stable sidecars/importer identity, and
captures exact source/Atlas cooked/project closure. Main Editor Play resolves captured sources for
scene/pool instances through the existing geometry implementation. Real composed Editor pixels
verify Pivot replacement, negative scale and retained old snapshots after source owners are gone;
see [cloud evidence](../../Tools/Build/evidence/EditorWorkflow/ED-WF-07-SourceSnapshots-Linux-2026-11-12.md).
The independent catalog/package slice (`nexora-whr.6.3`) is merged in PR #770. Inspector source
assignment, authoring save/reopen, Build and live animation/reload remain open. This supporting
slice does not accept ED-WF-07; workflow **5/9**, Editor **0/8**, 2D **9/11** remain unchanged.

## ✅ Authored Sprite preview without a gameplay module (2026-11-13)

The independent `nexora-whr.7.6` consumer freezes only source UUIDs referenced by the main
scene and their Atlas closure, then binds an owning preview to the actual isolated Play clone
without loading a library. The existing 2D Game selector, Pause/Step/Resume and Stop retain
immutable source geometry and retire registrations before World release. Source-free 3D Play
ignores unrelated unavailable 2D assets. Real Editor native acceptance repeats 32 sessions with
exact red/blue masks and unchanged authoring pixels/source bytes; CPU coverage also verifies
callback-free ownership, rejection and replay after source/project teardown.
[Contract](../../Apps/Editor/README.md#module-free-main-scene-sprite-play) and
[evidence](../../Tools/Build/evidence/EditorWorkflow/ED-WF-07-ModuleFreePlay-Linux-2026-11-13.md).
Configured package Play/Build, Scene View and animation/reload remain open; the independently merged
Inspector assignment is preserved. This does not accept ED-WF-07 or change workflow **5/9**,
graphical Editor **0/8**, 2D **9/11** or physical-host gates.

## ✅ Authored Sprite references in main module Play (2026-11-13)

The independent ED-WF-07 host slice (`nexora-whr.7.4`) freezes main scene source UUID references
with their exact Runtime/opaque capture and project asset closure. It binds default source Sprites
to the actual isolated Play scene before module `Create`, rechecks authored drift before activation
and rejects missing/unsupported/stale bindings before callbacks. Existing gameplay-created sources
and negative-scale pools remain supported; mixed legacy/authored/legacy native snapshots verify
Pivot reimport and retained owners. [Contract](../../Apps/Editor/README.md#authored-source-references-in-module-backed-play)
and [evidence](../../Tools/Build/evidence/EditorWorkflow/ED-WF-07-PlayReferences-Linux-2026-11-13.md).
Scene View, configured package Play/Build and animation/reload remain open.
The independently merged ND2D/Studio slice is preserved. This supporting slice does not accept ED-WF-07;
workflow **5/9**, Editor **0/8**, 2D **9/11** remain unchanged.

## ✅ Main Editor Sprite source reference model (2026-11-13)

The independent ED-WF-07 model slice (`nexora-whr.7.3`) adds an owning source/Atlas catalog and
versioned UUID references in the main Editor scene wrapper. Complete-selection assignment/clear
uses one metadata Undo/Redo, preserves unrelated/unsupported components and round-trips wrapped
scene Save/Reload. Exact project generation plus Content revision rejects stale source or Atlas
reimport actions; retained snapshots keep old immutable owners. Region indexes avoid repeated full
Atlas validation per source. See the [contract](../../Engine/Editor/README.md#main-editor-sprite-source-reference-transactions)
and [Linux evidence](../../Tools/Build/evidence/EditorWorkflow/ED-WF-07-SourceReferences-Linux-2026-11-13.md).
Main Scene View/configured package Play conversion and Build export remain open. The separately
claimed authored ND2D/Studio slice is not included. This prerequisite does not accept ED-WF-07;
workflow **5/9**, Editor **0/8**, 2D **9/11** remain unchanged.

## ✅ ED-WF-06 source API and Player acceptance

The native ED-WF-06 proof (`nexora-whr.6.4`) exercises actual SDK/C ABI source flips and Atlas
repacks with asymmetric native pixels in a relocated catalog game. Scene and negative-scale pool
handles, authored transforms and source identity survive Atlas-only replacement. Development and
strict Shipping native checks pass; [evidence](../../Tools/Build/evidence/EditorWorkflow/ED-WF-06-NativeSource-Linux-2026-11-12.md)
retains failed fixture/setup attempts. Full regression passes 486/486 without failures or skips;
all 41 latest exact-head hosted checks pass and PR #772 is merged. The consolidated
[acceptance](../../Tools/Build/evidence/EditorWorkflow/ED-WF-06.md) records compatible consumers,
immutable cooked closure, feature-OFF builds and native repacks. Workflow is **5/9 (55.6%)**;
Editor **0/8**, 2D **9/11** and physical/clean-host gates remain unchanged.

## ✅ Main Editor Sprite source Inspector (2026-11-13)

Supporting `nexora-whr.7.5` exposes imported source choices and Clear in the real main Inspector.
Complete selections share one metadata Undo/Redo and wrapped scene Save/Reload. Mixed, missing
source/Atlas and unsupported references have distinct states; unsupported bytes remain protected.
Writable project, stopped Play, focus/recovery and live selection/version checks guard every action.
The popup owns its bounded immutable catalog, paths and keys; stale imports or context changes
cancel it. No filesystem work or Runtime Sprite mutation occurs in the widgets.
Actual 1×/2× controls and Linux X11 interaction verify batch assignment, Clear, exact Undo/Redo,
save/reopen and unchanged read-only project bytes. See [execution evidence](../../Tools/Build/evidence/EditorWorkflow/ED-WF-07-SourceInspector-Linux-2026-11-13.md).
Module-backed main Play was independently delivered by `nexora-whr.7.4` / PR #776; Scene View, configured package Play, Build,
source animation/reload and full ED-WF-07 remain open. Workflow **5/9**, Editor **0/8**, TwoD **9/11**
and physical acceptance are unchanged.

## ✅ Source-bearing Studio document persistence (2026-11-13)

Supporting `nexora-whr.7.2` adds immutable source ownership, schema-2 ND2D persistence and source-aware
Undo/reopen/isolated Play while retaining source-free schema 1. Studio imports/assigns/previews source
UUIDs; Player validates embedded Atlas/source closure before module callbacks. Actual moved,
source-deleted packages and native pixels cover persisted Pivot/trim and negative scale. Initial
fixture/build-scheduling failures remain separate from accepted results in the
[cloud evidence](../../Tools/Build/evidence/EditorWorkflow/ED-WF-07-AuthoredSources-Linux-2026-11-13.md).
The independent Flip/repack task `nexora-whr.6.4` is accepted above. Main Editor authoring rendering,
No-module Play/Build conversion and live source animation/reload remain open. ED-WF-07 is not accepted;
workflow **5/9**, graphical Editor **0/8** and 2D **9/11** remain unchanged.

## ✅ Main-scene Sprite source Build support (2026-11-13)

Supporting `nexora-whr.7.7` captures saved source UUID references with owning, deduplicated imported
source/Atlas closure and exact publication baselines. Capability-3 games preserve source-free
capability-2 bytes and keep Runtime independent of optional TwoD payload codecs. The canonical
Player validates typed closure and renders actual remapped World transforms as a 2D overlay.
CPU cases cover stale/malformed/missing closure, owning lifetime and additive ID remapping;
real Build/SDK Zig, source deletion, Unicode relocation and zero-argument Player execution verify
exact Sprite color masks and unchanged underlying native 3D pixels. See [Build evidence](../../Tools/Build/evidence/EditorWorkflow/ED-WF-07-SourceBuild-Linux-2026-11-13.md).
Configured packaged Play preview, Scene View, source animation/pool and hot reload remain open;
independent module-free Play work is preserved. ED-WF-07 remains open. Workflow **5/9**, graphical
Editor **0/8**, TwoD **9/11** and physical/clean-host gates are unchanged.

Integration with independently delivered module-free Play (PR #778, main `2e900aa2`) is covered by
the [main integration supplement](../../Tools/Build/evidence/EditorWorkflow/ED-WF-07-SourceBuild-MainIntegration-Linux-2026-11-13.md):
full **521/521**, zero failures/skips, actual Development/Shipping native pixels, Monolithic and
2D-OFF builds. Native test transport repairs retain the same deadlines and all original assertions;
the earlier failed full gate remains separate evidence. Parent acceptance and milestone counts are unchanged.

## ✅ Source animation Clip foundation (2026-11-13)

Supporting `nexora-whr.8.1` adds source-only Clip schema 2 while preserving exact legacy Clip bytes
and C/Zig tables. Typed import/cook/catalog and Editor snapshots validate the complete source/Atlas
closure; explicit Play/Advance, immutable retained Clip ownership, ND2D history and Studio preview
preserve source identity and style. Real SDK/relocated Player Development and Shipping pixels cover
trim/Pivot, flips, negative scale, a second preloaded source/Atlas and stable source pool handles.
Full Linux **523/523**, zero failures/skips, Shipping/native, Monolithic and TwoD-OFF
gates pass. [Execution evidence](../../Tools/Build/evidence/EditorWorkflow/ED-WF-08-SourceAnimation-Linux-2026-11-13.md) retains earlier
failed/interrupted attempts. Source reimport, 32 real Zig reloads/rollback, configured packaged main
Editor Play and full ED-WF-07/08 remain open; the `repacked` native phase names a preloaded animation
frame, not live reimport. Workflow **5/9**, graphical Editor **0/8**, TwoD **9/11** and physical/clean-host
gates are unchanged.

The [compiler portability supplement](../../Tools/Build/evidence/EditorWorkflow/ED-WF-08-SourceAnimation-Portability-Linux-2026-11-13.md) records the
corrected full **523/523**,zero failures/skips,Development/Shipping native,Monolithic and TwoD-OFF
gates. GNU 15.2 before/after probes reproduce the original benchmark temporary warnings and verify
all 17 affected C++ translation units; explicit bounded test integer types address MSVC warnings.
Warning rules and native oracles remain intact. Initial `1ec968b6` local passes and failed CI remain
historical; complete ED-WF-07/08 and milestone counts are unchanged.

Additional `nexora-whr.8.1` native-fixture portability coverage passes523/523 and all12 local gates;
GCC15.2 compiles all309 actual TwoD consumer command variants, including the inherited CPU/native
PlayContext test. [Final native-context evidence](../../Tools/Build/evidence/EditorWorkflow/ED-WF-08-SourceAnimation-NativeContext-Linux-2026-11-13.md)
preserves both earlier committed receipts and the additional failed hosted head. Production32
source reloads/configured packaged Play and complete ED-WF-07/08 remain open; **5/9**, **0/8**, **9/11**
are unchanged.

## ✅ Isolated source reload generation foundation (2026-11-13)

Supporting `nexora-whr.8.2.1` stages independent TwoD World/metadata, camera/query, animation
clock/pause/style, scene requests/revisions and exact pool handles. Allocation/admission/reentry
failures preserve live values. Actual canonical SDK tests pass32 unique library replacements,
128 candidate rejections, leased old-code quiescence and sourceUUID reimport preserving anchors.
Full525/525, Shipping/native regression, Monolithic and TwoD-OFF gates pass; [execution evidence](../../Tools/Build/evidence/EditorWorkflow/ED-WF-08-GenerationStaging-Linux-2026-11-13.md)
retains initial failures and the interrupted run. This is a reusable isolated TwoD prerequisite:
production Player/Editor services and native reload integration remain open under `nexora-whr.8.2`;
full ED-WF-07/08, **5/9**, **0/8**, **9/11** and physical/clean-host acceptance are unchanged.

## ✅ Fenced Scene images in single-pass UI (2026-11-13)

Supporting `nexora-whr.7.8.1` adds explicit offscreen LDR Scene references sampled inside one UI
submission with pre-upload stale/foreign validation and fenced per-view resources. Actual Linux
Development/Shipping fixtures each pass12 frames,24 Scene draws,48 UI commands,two resizes and
asymmetric pixel/occlusion checks; an inverted-UV negative control fails as expected. Full527/527,
Monolithic and TwoD-OFF gates pass. [Execution evidence](../../Tools/Build/evidence/EditorWorkflow/ED-WF-07-SceneUiComposition-Linux-2026-11-13.md)
retains initial failures and separates local Linux from hosted platform checks. Parent `nexora-whr.7.8`
configured packaged Editor source Play remains guarded/open; production reload, full ED-WF-07/08,
Workflow **5/9**, Editor **0/8**, TwoD **9/11** and physical/clean-host acceptance are unchanged.
