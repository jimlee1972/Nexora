# ED project layout budget: Linux evidence

Date: 2026-10-07. Parent: `41aef39d57a2df785c5a7c153c91804006077f98` (Profiler JSON).

The layout reader previously accumulated an unbounded schema line and payload, then repeatedly
erased CRLF sequences. Large/corrupt project metadata could therefore consume excessive memory
or quadratic normalization time during graphical project open. ProjectWorkspace now applies the
same public **1 MiB raw-payload limit** to save/read. The chunked reader retains at most that
payload plus a ten-byte schema header, probing one extra byte to detect overflow; no unbounded
getline or metadata-sized allocation is used. CRLF normalization uses one in-place linear pass.

Schema 0/1, CRLF headers/payload and exact-limit records remain supported. Empty/NUL/over-budget
payloads, unsupported headers and non-regular/aliased metadata return an error without mutation.
Only a missing optional layout returns no value without error. Read-only saves and invalid
admission preserve last-good bytes and any unrelated stage. The existing single-writer atomic
replacement contract is unchanged; this is not a concurrent filesystem race guarantee. Public
object layouts, method signatures, C SDK wire contracts, module dependencies and test registration
are unchanged. The capacity constant is additive.

The existing `editor.workspace_budget` target now verifies exact-size save/read, legacy CRLF
headers, 524288 CRLF sequences, oversized LF/CRLF files, a 4 MiB schema line, unsupported/empty/NUL
files, directory/valid/dangling symlinks, missing-file error clearing, read-only save and last-good
plus occupied-stage preservation. A valid retry succeeds after every rejected-input group.

Isolated tools/sysroot and Vulkan environment match
[Profiler JSON](EditorEDM6-ProfilerJson-Linux-2026-10-07.md). Final gate:

```bash
cmake --preset linux-development
cmake --build --preset linux-development --parallel 4
ctest --preset linux-development
```

Configure/build passed. Full CTest: **123/123 passed**, none skipped, **203.85 seconds**, with
lavapipe and Khronos core/synchronization validation. The first restricted-socket attempt failed
to start Xvfb listeners and skipped three display-dependent Vulkan tests; a direct listener probe
confirmed the environment restriction. The complete gate was rerun with display socket access
enabled and passed without disabling any tests. Focused workspace/Profiler regression tests:
**4/4 passed**, 1.65 seconds. `git diff --check` passed. No linkage boundary changed; the parent
Profiler evidence separately records successful Shipping/Full and graphical Development/Monolithic
builds. Generated output and local tools remain uncommitted.

Source SHA-256:

- `Engine/Editor/src/ProjectWorkspace.cpp`: `52dbd62547d485117c967d8bb6a21b66580b70a8e2a2a097fe031dec2574e7cc`
- `Engine/Editor/include/Nexora/Editor/EditorWorkspace.h`: `38aecc6e31c0ced7221fcd3ee2728f8365ad22d8db24eae2261dee3f4b1f8a52`
- `Tests/Editor/WorkspaceBudgetTests.cpp`: `67e06b2039b75022ecfb79d6d1c06b9827d74439a64ba41a00d62f692a46b82b`

This delivers bounded layout persistence, not complete production-hardening or physical-host
acceptance. Full graphical milestone acceptance remains **0/8**. Hosted checks must pass before merge.
