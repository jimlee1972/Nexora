# ED-M4 additive-scene dependency admission: Linux evidence

Date: 2026-10-08. Source parent: `69f2d85246e76628e12e7a68c4f91493a48ec3ca`.
Beads implementation task: `nexora-pmb.2.1`.

Previously, `AdditiveSceneGraph::Add` inserted unchecked initial dependency IDs. A self-dependency
could succeed, make `LoadOrder` fail and prevent removing that same scene. Missing initial IDs could
also leave unresolved edges and permit a later insertion to close a cycle. `SetDependencies`
already rejected missing/self edges, but initial admission bypassed that contract.

Admission now checks nonzero unique scene ID, nonempty path and existing non-self dependencies
before modifying the graph. Dependencies are sorted and deduplicated consistently with
`SetDependencies`. Existing scenes cannot reference the new ID, so admitting only edges to existing
scenes preserves acyclicity. Rejection retains every existing descriptor and edge. The graph owns
descriptors, not documents or files; no scene payload, class layout, serialization schema, public
signature or module dependency changes.

The dedicated `editor.additive_scene_contract` regression uses owned and referenced scenes. It
rejects zero scene IDs, empty paths, self/missing/zero dependencies, mixed valid/missing edges and
duplicate IDs without changing retained paths, ownership, edges or load order. It then admits a
previously missing ID and removes scenes in safe reverse dependency order, verifying no rejected
incoming edge survived. Descending root insertion, duplicate edge normalization and a multi-level
chain verify deterministic load order. Replacement cycle/self/missing rejection preserves that
graph and its removal behavior.

Validation ran in a Linux 6.18.44 cloud worktree with GCC 14.2.0, CMake 3.31.6 and Ninja
1.11.1.git.kitware.jobserver-1. The configured defaults are graphical shell **OFF** and Slang **OFF**.
External tool/sysroot settings were sourced from the session's tools environment; no machine
configuration or generated output is committed. Native Vulkan tests used Mesa lavapipe and Xvfb,
with permission for local display sockets.

```bash
cmake --preset linux-development
cmake --build --preset linux-development -j 2
ctest --preset linux-development -R '^editor.additive_scene_contract$' --output-on-failure
ctest --preset linux-development
git diff --check
```

Configure passed in 52.6 s; build passed all 234 steps. Focused regression **1/1 passed**, 0.00 s.
Full Linux Development CTest **84/84 passed**, zero skipped, **21.99 s**. Changed C++ files were
formatted with clang-format 19.1.7 and `git diff --check` passed. Shipping was not required because
this repair changes no module linkage boundary. Root README remains unchanged.

This is a portable graph prerequisite. Additive document tabs, coordinated multi-document save,
cross-scene reference behavior, external-change conflict UI, crash/reopen goldens and full ED-M4
acceptance remain open. Windows/macOS/mobile or physical-display acceptance did not run.

## Integration with merged Inspector prerequisite

After PR #440 merged, this branch integrated main
`06b11369daf2f9d0da86098df5343f8e00e122cf`. The CMake conflict was resolved by retaining both
`editor.additive_scene_contract` and `editor.inspector_atomic_batch`. The full preset configure,
build (`-j 2`) and CTest gate ran again: **85/85 passed, zero skipped**, **24.63 s**.
The same graphical OFF / Slang OFF settings apply; no graph implementation change was needed.
