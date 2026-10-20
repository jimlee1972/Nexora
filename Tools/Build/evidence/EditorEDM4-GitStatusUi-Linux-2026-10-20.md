# ED-M4 explicit graphical Git status — Linux cloud evidence

Date: 2026-10-20. Supporting Beads goal: `nexora-gu7.22.2`.

## Scope and contracts

The native Editor owns a real `GitStatusProvider` on its shared JobSystem and calls the explicit
Source control view with project UUID/root lifetime scopes. Opening a project/window never runs
Git. Only the trusted absolute executable selected by the user and Refresh start a query. The
existing bounded backend retains all authority, generation, cancellation and complete-publication
rules. No write command, credential storage, network service or serialized format is introduced.
The additive C++ UI export requires rebuilding consumers; shutdown drains the provider before
shared jobs and libraries. See Editor and EditorImGui READMEs for ownership and threading.

## Actual widget coverage

`editor.git_status_ui_1x` and `editor.git_status_ui_2x` drive real product pointer/key/text controls
at 1x/2x. Real Git queries disposable Unicode-path projects. Assertions cover clean, external
changes, renamed/Unicode/percent paths, POSIX raw/control bytes with escaped display, unavailable
tool/repository, read-only inspection and unchanged source/index bytes. More than 500 rows remain
in the capture while fewer than 100 render. Negative native process fixtures cover malformed
output, Cancel, old-scope completion and running-child shutdown. A private one-shot capture
allocation fault preserves last-good; active executable drafts and captures clear across project
replacement. Retained owning captures survive provider shutdown.

The freshly rebuilt focused tests passed both scales (0.62 s) before latest-main integration.
Complete final Linux, linkage/Shipping and hosted CI validation is pending; this record does not
claim delivery yet. No physical display/DPI/IME, glyph, nested-repository discovery, diff/merge or
full source-control/ED-M4 acceptance is claimed.

## Latest-main focused and Shipping checks

Latest main `43d6bb967e41c9a5d572285056d2da7e0e168f0c` was merged normally before the final
source freeze. The actual-widget tests also explicitly revoke refresh permission and send native
FocusChanged loss while a child is running: both cancel/drain and retain last-good. Freshly rebuilt
focused tests pass 2/2 in 0.88 s. Source/CMake hashes are retained outside the repository.

Minimal Monolithic `linux-shipping` configure/build passes all 102 build steps with cached pinned
Vulkan headers and Slang/Zig disabled. This profile strips Editor and disables BUILD_TESTING;
`ctest --preset linux-shipping` reports no tests. It is a packaging/linkage-policy check, not
Shipping Editor widget execution. Development and applicable hosted Editor checks cover those
boundaries separately. Documentation routing tests pass 16/16, Markdown/local-link/bilingual checks
and touched C++ formatting pass. The final full Development gate is queued behind the ongoing
independent main/authoring gate to avoid competing heavy hierarchy suites; no full pass is claimed yet.

## Complete final local delivery gate

The unchanged frozen source/CMake passes fresh Linux preset configure/build and the full serial
Development suite:294 registered,282 actual passes,12 existing native feature/environment skips,
zero failures,620.87s. Actual Git status1x/2x pass0.37s/0.39s; unchanged hierarchy Deep71.78s and
100k260.82s use the original180/300s deadlines (scale1). Full LastTest and all eight matching
source hashes are archived externally. The Git test registrations and README contract use stable
interior insertion points, preserving independently appended authoring/reopen coverage.

```sh
cmake --preset linux-development
cmake --build --preset linux-development
CTEST_PARALLEL_LEVEL=1 ctest --preset linux-development
cmake --preset linux-shipping -DNEXORA_ENABLE_SLANG=OFF -DNEXORA_ENABLE_ZIG_GAMEPLAY=OFF
cmake --build --preset linux-shipping
ctest --preset linux-shipping
```

Shipping is the102-step Minimal/Editor-stripped build noted above; it has no registered tests.
Documentation16/16, links/bilingual pairing, touched C++ format and diff checks are repeated before
publication. Applicable hosted exact-head checks must succeed before squash merge; no hosted or
physical/native-window/display acceptance is substituted by this Linux record. The parent Git
source-control goal remains open for diff/merge, nested discovery and broader integration.
