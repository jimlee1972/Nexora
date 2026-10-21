# Bounded read-only Git diff provider — Linux, 2026-10-21

Scope: Beads `nexora-gu7.22.3`, an SDK/backend slice of M4-06. Graphical diff/merge,
nested discovery, production-scale provider acceptance and ED-M4 remain open.

## Implementation and contracts

`GitDiffProvider` launches an explicitly trusted absolute Git executable through the existing
owner-thread `BuildProcess`/JobSystem boundary. A canonical repository root, fresh scope and
nonwrapping operation ID qualify each request. Inputs and captures own storage; workers hold no
project/document or native-module borrows. Shutdown drains before JobSystem/library teardown.

Requests select staged or working-tree output and one/two distinct literal relative UTF-8 paths.
Fixed argv uses `--literal-pathspecs`, `--no-pager`, `--no-optional-locks`, no external diff/textconv,
color, rename detection or submodule recursion, and explicitly disables fsmonitor/untracked-cache
updates. No shell/network/index-mutation/apply command is launched. Repository configuration,
environment and other Git helpers (including attribute clean filters) remain trusted; this is not
a configuration/process sandbox.

Complete merged stdout/stderr is bounded to 256 KiB, with no truncated tail publication. It is raw
Git output, not a parsed patch, atomic source snapshot or merge candidate. Binary/conflict notices,
warnings and NUL bytes are retained exactly; consumers must escape/redact before display/storage.
An empty result describes the exact request, including possibly nonexistent/untracked paths, and
does not establish repository cleanliness. Invalid UTF-8 status names cannot cross this process
argv boundary. No format, module dependency or stable C/Gameplay ABI changes. C++ SDK users rebuild.

Failed intake preserves state. Complete same-scope failure/cancellation retains last-good with its
original request identity. Scope mismatch permanently rejects the operation even after switching
back. Distinct typed unavailable/process/overflow/allocation/cancelled/stale/stopped outcomes avoid
presenting failure as an empty successful diff. Captures remain owning after later queries/teardown,
but must be released before unloading the libraries that implement their deleters.

## Final Linux validation

The validation branch began at main `20c597cedad1813e448dd5dddb332b0a626e8788` and normally
integrated accepted main `5f9269318b6950703e829f5c924df7f7aa2ae586`. Its upstream tree is exactly
`9a24dc26465cb2849f3c1b6fa95af032ca2b0095`. Final implementation/test/CMake sources are frozen:
all 37 checked SHA-256 values remain unchanged across the complete gate. Publication carries
that same source tree onto a fresh latest-main branch; private integration history is retained.

```sh
cmake --preset linux-development -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON \
  -DNEXORA_ENABLE_SLANG=OFF -DNEXORA_ENABLE_ZIG_GAMEPLAY=OFF \
  -DNEXORA_BUILD_PROJECT_PLAYER=ON
cmake --build --preset linux-development
CTEST_PARALLEL_LEVEL=1 ctest --preset linux-development
```

GCC 14.2, CMake 4.4.4, Linux cloud, graphical shell enabled; existing pinned ImGui/Vulkan-header
sources are reused. Final build: 327 integration steps over the initial 791-step complete product
build. Full gate: **304 registered, 292 passed, 12 existing environment skips, zero failures,
696.16 seconds**. The skips are unavailable native Vulkan/renderer/material-device cases; they
are not physical-device acceptance. Original Deep/100k cases pass at scale 1 in 66.02/250.44 s
with unchanged 180/300-second limits. These are functional gates on a shared cloud host, not
isolated production performance percentiles or budgets.

Focused parser/status/new diff/status-UI tests pass 5/5 in 2.29 s; new `editor.git_diff_provider`
passes in 0.71 s. Actual temporary-repository coverage includes:

- staged versus external working changes, literal bracket/pathspec-magic/dash/Unicode/POSIX
  newline names, binary notices, rename deletion/addition, conflict and linked gitfile worktrees;
- unchanged index/config/source bytes and blocked external diff/textconv/fsmonitor;
- exact 256 KiB output, complete raw warning/NUL bytes and overflow rejection;
- invalid inputs, actual broken Git, one-shot intake/outcome/capture allocation faults;
- queued/running cancellation, scope-switch-back rejection, shutdown and owning retained captures.

The native negative fixture explicitly selects binary stdout on Windows, preserving its exact
byte oracle instead of CRT newline translation. That branch requires hosted Windows validation;
no local Windows/macOS pass is claimed. An earlier fresh-default ProjectPlayer-OFF gate also
passed 301/289 actual passes/12 skips in 797.81 s; the final gate above enables all three optional
ProjectPlayer CLI cases and is the acceptance result for the corrected final source.

`cmake --preset linux-shipping` and `cmake --build --preset linux-shipping` pass all 102 fresh
Minimal Monolithic build steps. Shipping strips Editor and registers no tests; the absence of
tests is not reported as a test pass. Local Slang/Zig are disabled there as well. Documentation
tests pass 16/16; changed/new local Markdown links, EN/zh-TW pairing, touched C++ formatting and
diff checks pass. The root README remains unchanged.

Exact-head hosted CI must succeed before squash merge. No graphical diff/merge, complete
source-control or milestone acceptance is claimed; ED-M4 and physical targets remain open.
