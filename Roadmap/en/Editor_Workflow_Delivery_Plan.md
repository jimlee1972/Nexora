# Editor script workflow and Sprite integration delivery

[繁體中文](../zh-TW/Editor_Workflow_Delivery_Plan.md) · [Epic #741](https://github.com/jimlee1972/Nexora/issues/741)

Updated: 2026-11-01. Baseline: `8c4aa989ee7443cc0b2129d54f0f0f77c73d2b49`.
Initial documentation publication base: `67db45d23e631b0e05b42c23716245a34ae996d5`.
ED-WF-04 integration base: main `a21c1ca2293e4200de8dcaa4b892ffef34687c9c`, including the appearance update.
Acceptance snapshot: **4/9 work orders (44.4%)**; ED-WF-01 merged in [PR #751](https://github.com/jimlee1972/Nexora/pull/751), ED-WF-02 in [PR #754](https://github.com/jimlee1972/Nexora/pull/754), ED-WF-03 in [PR #756](https://github.com/jimlee1972/Nexora/pull/756), ED-WF-04 in [PR #758](https://github.com/jimlee1972/Nexora/pull/758).
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
| ED-WF-06 | [#747](https://github.com/jimlee1972/Nexora/issues/747) | 01 | Compatible C/Zig source APIs and cooked Player dependency closure | Pending |
| ED-WF-07 | [#748](https://github.com/jimlee1972/Nexora/issues/748) | 06 | Source indexing/Inspector/Undo/save/reopen/Play/Build/reimport in the real Editor | Pending |
| ED-WF-08 | [#749](https://github.com/jimlee1972/Nexora/issues/749) | 04, 06, 07 | Source animation anchors, live pool instances and valid handles survive replacement | Pending |
| ED-WF-09 | [#750](https://github.com/jimlee1972/Nexora/issues/750) | 02, 03, 05, 07, 08 | Integrated native acceptance and a verified, extracted Windows Editor ZIP | Pending |

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
workflows and final delivery remain ED-WF-05 through ED-WF-09.
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
