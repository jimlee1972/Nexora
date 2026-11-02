# ED-WF-01: workflow roadmap and acceptance baseline

Date: 2026-10-31. Epic: [#741](https://github.com/jimlee1972/Nexora/issues/741).
Work order: [#742](https://github.com/jimlee1972/Nexora/issues/742).
Baseline: `8c4aa989ee7443cc0b2129d54f0f0f77c73d2b49`.

Publication base: latest remote main `67db45d23e631b0e05b42c23716245a34ae996d5`; other-work roadmap updates are retained
as baseline content, not included as changes delivered by this work order.

## Confirmed baseline

- `Engine/EditorImGui/src/EditorImGui.cpp` already exposes script selection, manifest, Build and
  Cancel. Completing persistent settings, Build & Run and diagnostic/progress integration remains
  ED-WF-02; this baseline does not claim those features are delivered.
- `Engine/Editor/src/GameBuild.cpp` still stages `Build.commands`, `Build.sources`, `Build.tools`
  and `Game.inventory` into output. ED-WF-03 must separate development/publication records without
  weakening verified replacement, rollback or readonly-source behavior.
- `Engine/Runtime/src/GameplayModuleHost.cpp` has generations, dynamic library admission and
  migration callbacks. `Bindings/Zig/game.zig` leaves save/load callbacks null. Existing Runtime
  tests are not proof of the complete stateful Editor pipeline or candidate side-effect rollback.
- `Engine/TwoD/SpriteAssets.md` explicitly leaves main Editor source admission, Player catalogs,
  C/Zig descriptors and automatic animation/pool publication as follow-up integration work.
- Native Windows GUI entry, approved branding and hidden Editor startup are retained foundations,
  not new accomplishments counted toward this nine-work-order plan.

## Tracking and scope

The Epic links all nine work orders (#742 through #750), dependency edges, acceptance gates and
per-task evidence targets. The paired [English](../../../../Roadmap/en/Editor_Workflow_Delivery_Plan.md)
and [Traditional Chinese](../../../../Roadmap/zh-TW/Editor_Workflow_Delivery_Plan.md) plans define
the same graph and contracts. Editor/TwoD roadmaps and the roadmap index link the new plan.
All affected roadmap documents were reviewed for Build UI, reload, source assets and acceptance;
no existing full graphical milestone or physical M6/M7 claim is promoted by this documentation.
The repository README remains unchanged and reader-facing.

Progress at publication preparation is 0/9 accepted, not a completion claim. After documentation
validation, a merged PR and issue acceptance, the Epic records ED-WF-01 as the first accepted
work order; later implementation batches update the roadmap snapshot accordingly.

## Validation

This batch changes documentation only. Markdown/link/bilingual validation, documentation routing
tests and `git diff --check` must pass before publication. Exact executed results will be recorded
below before commit. Native/Linux implementation gates are not claimed for this documentation
batch; ED-WF-02 through ED-WF-09 retain their required complete implementation gates.

Local documentation validation: PASS (Windows development host; not Linux/native product acceptance).

| Check | Result |
| --- | --- |
| `DocumentationCI.validate_documents` over all eight changed/new Markdown documents | PASS; local links and bilingual pairs valid |
| UTF-8/no-BOM and Epic/child issue mapping | PASS; #741 through #750 match both plans |
| `python Tools/Build/TestDocumentationCI.py` | PASS; 16 tests, exit 0, 25.320 seconds |
| `git diff --check` and reader-facing README preservation | PASS; README unchanged |

The final fixture run uses an external `python3.exe` alias for the same Python 3.14 interpreter,
the repository-pinned Markdown packages and a real empty Git excludes file. Earlier Windows
fixture attempts failed on the Store executable alias (2 errors), then an invalid null-device
excludes override (16 errors); these were test-environment failures, not passing runs. No test
assertion, workload or source implementation was changed to obtain the final 16/16 result.

✅ Accepted after [PR #751](https://github.com/jimlee1972/Nexora/pull/751) merged as
`60f42e90868b06cabc9509d85a4e3ee6d5b6c1cd`. The reviewed head was
`1c2e2fd7ebe241c964f15947ed96398769ab4e3f`, tree `2bedbb3449ec924be6c58cbd9f18ecca9299b900`.
All 35 check-runs completed: 25 success and 10 intentional documentation-route skips, none failed
or pending. Push Build [41177030339](https://github.com/jimlee1972/Nexora/actions/runs/41177030339)
also completed successfully: Linux Development 487 passed/1 conditional skip, Windows Development
429 passed/1 conditional skip, macOS 404 passed/1 conditional skip, Linux Editor display and
build-contract. These hosted platform results do not accept physical/clean-host gates.
The Epic and both workflow roadmaps now record 1/9 accepted; ED-WF-02 remains pending acceptance.

## External services and Beads

The original cloud host was unavailable when dispatch was attempted. No successful dispatch or
cloud execution is claimed. Work continues in the existing local checkout with remote CI gates.

Pinned `bd version` returned 1.3.1. The existing `refs/dolt/data` was verified; bootstrap dry-run
selected clone from the existing repository/database rather than creating shared history.
Actual bootstrap returned exit 1 because its workspace gate reported access denied on the external
task directory. Shared task graph reads/mutations stop; no shared database was initialized,
replaced or force-pushed. GitHub work orders remain the visible tracking record. External task
configuration and failed bootstrap state are retained outside the repository.
