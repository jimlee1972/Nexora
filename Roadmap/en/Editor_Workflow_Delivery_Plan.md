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
PR #762 is now merged as `f7537be874d8`, with the same tree as `d093841b`.
The [Cloud receipt](../../Tools/Build/evidence/EditorWorkflow/ED-WF-05-Cloud-d093841b.json)
records496/496 and all five gates exit0; the [CI inventory](../../Tools/Build/evidence/EditorWorkflow/ED-WF-05-CI-d093841b.json)
records41 successful checks/three workflows. This foundation does not accept ED-WF-05 or change4/9.
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

Local uncommitted follow-up coordinates production Play 2D resource branches and scene-service
quotas; 32 handoff cycles preserve animation clocks, pool handles, input edges and texture identity.
This is scoped Windows process evidence, excluded from PR762; main transaction/watcher admission,
actual Editor source edits and the full immutable-head gates remain required. No count change.

The [main Editor transaction checkpoint](../../Tools/Build/evidence/EditorWorkflow/ED-WF-05-Transaction-Windows.md)
adds strict bare/configured/2D World and service generations. Fourteen native targets build;
23 scoped Windows tests and two real no-argument GUI launches pass. The main C++ fixture records
96 replacements/576 expected rejections. Automatic source-edit/watcher/compiler admission and
full immutable-head Cloud/Shipping/CI remain pending; ED-WF-05 is unaccepted and4/9 is unchanged.

The [startup diagnostic follow-up](../../Tools/Build/evidence/EditorWorkflow/ED-WF-05-Startup-Diagnostic.md)
retains PR763 Windows package startup failure and adds bounded first-frame evidence. Two local
no-argument launches still pass; the CI root cause and exact-head full gates remain pending.
This is a diagnostic checkpoint, not an accepted fix; completion remains 4/9.

The [live-script checkpoint](../../Tools/Build/evidence/EditorWorkflow/ED-WF-05.md#live-script-workflow-checkpoint-unaccepted)
now wires actual configured Play to background observation/shared compilation and state-preserving
admission. Local SDK/build/coordinator regression passes3/3; one real Editor source replacement
passes. The strengthened32-cycle native run is executing, and startup CI still fails. Full
immutable-head gates and remaining scenarios are pending; acceptance stays4/9, Editor0/8.

Follow-up evidence: the local32-cycle attempt stopped after11 replacements when Editor exited;
it is FAIL, not accepted. Cloud at0f901cb3 is also FAIL (498/499, pointer release/pose observation).
The extracted-ZIP Windows32-cycle gate now runs automatically on the active implementation branch and on explicit package workflow runs; original
input/startup deadlines remain. See [current evidence](../../Tools/Build/evidence/EditorWorkflow/ED-WF-05.md).

Operator clarification: the11-replacement local run was manually closed by the user. It is
operator-interrupted rather than a product crash, and still does not satisfy32-cycle acceptance.
The isolated hosted Windows gate remains required; the unrelated Cloud pointer failure stays open.

The [startup fit/exposure receipt](../../Tools/Build/evidence/EditorWorkflow/ED-WF-05-Startup-Fit-Windows.json)
records5/5 native Windows startup/branding/entry passes (26.58s). Hidden outer bounds fit the monitor
work area; an owned test launcher establishes foreground-parent context without substituting for
the Editor or relaxing pixels/response/NoActivate/deadlines. Fresh immutable-head CI/Cloud and
extracted-ZIP32 remain pending; no work-order/milestone count changes.


The `51bd8a16` hosted Windows package now passes its original startup gate and actual extracted-ZIP32-cycle run: counter0→27512, Scene1/entity4 retained, actual Ctrl+P and equal-mtime edits, observed compiler children drained, normal exit0 and owned-job processes0. The downloaded artifact digest and ZIP CRC are verified; see [package receipt](../../Tools/Build/evidence/EditorWorkflow/ED-WF-05-Package-51bd8a16.json). This is scoped Windows evidence, not final acceptance. Full Cloud revalidation remains FAIL495/499 with four Prefab GUI failures; all Development/Shipping build commands exit0, and the former pointer case passes. Two CI aggregates fail because four macOS/iOS jobs were abandoned/cancelled. No merge or accepted-count change.
A supplementary native harness now requires rejected compilation, state restore, candidate initialization and candidate-start World mutation to leave old dispatch/state/handles intact, followed by successful recovery. Its first local attempt observed four rejections but lost the runner before recovery completed; no terminal exit/receipt exists, so it is incomplete. A new checkpoint preserves partial observations without claiming PASS. Updated harness/admission validation remains pending.

The next readiness repair keeps the Linux main window hidden until its first completed frame, and X11 acquisition now requires a visible window within the original15-second budget. Native source-apply pixel waits inherit the containing10-second gesture deadline; failed fixtures are retained. These changes are not yet accepted or claimed as the proven cause of all four Cloud failures. Windows native tests share explicit existing-input-desktop selection and an owned foreground launcher immediately before each real process launch; they never send keys to foreign windows. The initial extracted-ZIP fault attempt refused Play input on an unowned foreground before dispatch, and a local startup rerun rejected occlusion; both failures are retained. No LP worker-budget candidate is adopted: its historical full CTest also fails493/499.

The integration now includes upstream main `1df6ab357da69ee3257097a9b5176695b7c96b9a` (PR #765 authored Scene V1). GameplayBridge keeps both catalog entity routing and the legacy World-pointer rebind contract. Catalog bridges reject single-scene fork/rebind without writes, with a regression preserving World/primary/revision/API state; this does not claim complete catalog reload, which remains part of the later integration work. Fresh post-merge native build/scoped tests and immutable Cloud/Shipping/CI remain required.

Post-integration Windows checkpoint: all9 targets build(exit0), and all6 scoped bridge/catalog/compiler/proxy/reload/packaged-Play CTests pass(exit0,5.64s). [Receipt](../../Tools/Build/evidence/EditorWorkflow/ED-WF-05-Main-Integration-Windows.json). No GUI/full-gate or count acceptance follows; fresh immutable Cloud/Shipping/CI remains mandatory.

The8870 hosted Windows build and original five startup checks pass, but package verification
rejects the launcher foreground precondition before the32-cycle run. The complete job log and
73,590-byte failure artifact are retained. Launcher follow-up uses a real owned Button interior
inside the work area, bounded message acknowledgement and guarded native click/release when
activation is denied. Three native launcher-only trials pass locally with zero surviving windows;
this is not new Editor acceptance. Linux hosted display also fails packaged Play with
X_SetInputFocus BadMatch: its two private searches still selected an unmapped Editor. Both now
require onlyvisible under their original budgets. [Launcher/failure receipt](../../Tools/Build/evidence/EditorWorkflow/ED-WF-05-Launcher-Windows.json).
Exact-head Editor/Cloud/Shipping/fullCI gates remain pending; no accepted-count change.

The active8870 Cloud run also reports two Showcase failures: shadow-off pixel comparison
and the original30-second virtual-display child deadline. A scoped follow-up retains partial
timeout streams, a rejected launch receipt and drained/reaped Xvfb diagnostics. Failed
interaction fixtures, file-backed child streams and input command journals are retained;
13 portable policy tests pass. This repairs evidence collection, without claiming either native failure
is resolved or changing deadlines/assertions. Full gate terminal results and exact-head
revalidation remain pending. [Diagnostic evidence](../../Tools/Build/evidence/EditorWorkflow/ED-WF-05.md#linux-showcase-failure-evidence-follow-up-unaccepted).
The same Cloud run later reports the original game-build worker and script-build CTest
deadlines failing; their fixtures are retained for diagnosis, with no timeout waiver.

Immutable885693db hosted Linux Editor CI now passes250/250 tests(1406.98s), including
the historical Prefab/pointer/packaged-Play failures and both SDK build cases; scoped
native Player4/4, Editor validation3/3 and renderer20/20 also pass. The downloaded artifact's
CRC and SHA-256 are verified. [Scoped receipt](../../Tools/Build/evidence/EditorWorkflow/ED-WF-05-Linux-CI-885693db.json).
This does not replace complete Codex Cloud/Shipping or Windows package acceptance;4/9 unchanged.

The885693db extracted Windows ZIP now passes32 native source replacements, four real
failure/rollback cases, valid recovery and Stop/compiler drain (Editor exit0, owned Job0).
Scene1/entity4 persist; counter0→32570→37300. Relocated/no-argument Editor/game launch,
splash ordering/responsiveness and no-console checks pass. Both ZIP CRCs, artifact SHA-256
and all22697 inventory payload hashes are verified; one public Editor executable, no source
checkout. [Windows receipt](../../Tools/Build/evidence/EditorWorkflow/ED-WF-05-Package-885693db.json).
Full Cloud/Shipping/CI, Sprite integration and final delivery remain open;4/9 unchanged.

All42 exact885693db CI checks and all4 workflows now succeed; see the
[complete CI receipt](../../Tools/Build/evidence/EditorWorkflow/ED-WF-05-CI-885693db.json).
The separate original8870 Cloud run has at least10 failures and still requires its final
inventory/Shipping results plus immutable-head revalidation. No merge or acceptance-count change.

The original8870 Cloud run is sealed as FAIL491/502,11 failures,zero skips/retries; all four
Development/Shipping configure/build commands exit0. [Complete receipt](../../Tools/Build/evidence/EditorWorkflow/ED-WF-05-Cloud-8870d63e.json).
SDK follow-up removes per-snapshot Zig build-runner bootstrap through the common CMake entry,
preserving canonical code/tests, per-module optimization/target, compiler-rt and private caches.
Scoped Windows rebuild and4 original cases pass; the separated original script/authored-test
rejection cases pass2/2 without extending original limits. [SDK evidence](../../Tools/Build/evidence/EditorWorkflow/ED-WF-05-SDK-Direct-Windows.json).
Latest-main integration and full immutable Cloud/Shipping/CI/native package revalidation remain
pending;4/9 unchanged.

Mainb1671eaa crash logging is integrated with the SDK fix. Both migration save-state calls
retain capture protection and diagnostic context. All14 selected Windows targets build and
all15 scoped native CTests pass(658.98s). [Integration receipt](../../Tools/Build/evidence/EditorWorkflow/ED-WF-05-Crash-Integration-Windows.json).
Full immutable Cloud/Shipping/CI and refreshed extracted/native Windows package acceptance
are still required;4/9 unchanged.

The user confirms the earlier local Editor exit was operator initiated. That interrupted
run is neither a crash nor a completed 32-reload acceptance. Exact0c82 Windows CI separately
fails startup before `prepare-display-returned`, with the main window still hidden; the
font preparation cause remains under investigation. Showcase CI observes tour step6 at
210.028 seconds but reads an empty final export. The export reader now waits for complete
schema-valid JSON and nonempty Markdown within the original five-second deadline, retaining
the accepted strings as evidence. A headless Windows regression invokes the actual production
functions: the original reader fails, the revised reader passes delayed empty/partial/complete
publication and bounded incomplete-output rejection. Native CI and full Cloud gates for these
changes remain pending; no merge, new acceptance mark or final ZIP delivery.
Startup follow-up adds opt-in flushed font substage checkpoints, enabled by the Windows
startup fixture, without changing the original35-second readiness bound or glyph coverage.
The checkpoint identifies where a later failure stops; it does not itself prove a startup fix.
Scoped Windows Editor/font rebuild passes(111.99s), `editor.font_glyphs` passes with
diagnostics enabled(0.98s case/1.11s CTest), and the real-function export regression passes.
[Failure archives, source hashes and local validation](../../Tools/Build/evidence/EditorWorkflow/ED-WF-05-CI-Followup-Windows.json)
retain the original failed CI evidence; full new-source acceptance remains open.

Exactcc282e0d Windows startup still fails(37.54s): the first whole-font read takes23.668898s,
then the second read has no end checkpoint before the unchanged35-second readiness bound.
The underlying filesystem delay is unproven. Windows default system fonts now use owned read-only
file mappings with stable-size/write exclusion and atomic-retirement sharing; explicit sources
and POSIX retain owning copies. Source/profile/atlas caps, selection/preflight and ImGui's own
per-atlas copies remain. Scoped Editor/font build passes107.71s; final parity-test rebuild45.33s;
glyph suite including mapping lifetime/retirement passes1/1(1.19s case/1.59s CTest). Real Windows
system-font comparison matches all7182 profile scalars(presence/UV/ink/raster hash), including the
same35 reported misses, faces and atlas extent. This warm-host CPU evidence does not establish cold
CI improvement. [Failure and mapping receipt](../../Tools/Build/evidence/EditorWorkflow/ED-WF-05-Font-Mapping-Windows.json).
Full exact-source Cloud/Shipping/CI and original native startup/package acceptance remain open;
no merge, new acceptance mark or final ZIP. Work orders4/9 unchanged.

The original0c82 complete Cloud matrix is sealed FAIL449/504,55 failures,zero skips/retries;
all four Development/Shipping configure/build commands exit0. Forty-five original outputs
explicitly report read-only Logs; ten keep their distinct timeout/assertion/startup classification.
[Full original receipt and Prefab failure snapshot](../../Tools/Build/evidence/EditorWorkflow/ED-WF-05-Cloud-0c82cf37.json).
The separate writable-state matrix does not change that result.

The preserved Prefab case records456 inputs over176.397s, late in readonly checks. Its first455
completed transports each include a fixed200ms settle, in addition to existing actual GUI ACKs.
The harness now awaits ACK-bound key/pointer phases directly, preserving unobserved settles,
all scenario assertions and original8s phase/180s outer bounds. Transport completion/error records
and a remaining-phase command bound expose blocked commands without retry. The original trace
has no completion records, so it does not prove an exclusive timeout cause. Windows Python
process-policy tests pass10/10; native Prefab/full exact-source acceptance remains pending.

Exactb2550b33 Windows startup passes both original and relocated no-argument launches:
four first-frame readiness durations are3109/1500/1578/1500ms, white fraction0, responsive
input, normal exit0 and no orphan windows. The package gate then fails before Zig execution:
Ninja's generated batch file cannot enter the Unicode private working directory at line2.
[Partial package receipt](../../Tools/Build/evidence/EditorWorkflow/ED-WF-05-Package-b2550b33.json).
The controlled old-command reproduction passes in UTF-8 but fails in a hidden child-owned
OEM437 console, while the parent's code page remains unchanged. The SDK now dispatches serial
native argv through a short private CMake script, preserving literal data, existing compiler/test
options, failure propagation and process ownership. Scoped Windows builds and the original five
cases pass; the final strengthened OEM437 policy suite and real Zig Unicode-temporary case pass2/2
(46.14s). The policy stub alone is not Zig/Player acceptance.
[SDK receipt](../../Tools/Build/evidence/EditorWorkflow/ED-WF-05-SDK-Argv-Windows.json).
Full exact-source Cloud/Shipping/CI and refreshed native package gates remain required;
ED-WF-05 stays unaccepted, work orders4/9, with no final ZIP delivery.
