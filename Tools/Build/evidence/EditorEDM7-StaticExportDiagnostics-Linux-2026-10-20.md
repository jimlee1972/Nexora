# ED-M7 StaticView checked report output and failure diagnostics — Linux, 2026-10-20

Delivery: `nexora-gu7.38.2.1`; parent `nexora-gu7.38.2` investigation remains open.
Delivery base: main `6bed775d80bab8a4766293eed8b9db9fb52d835d`.

## Reproduction and behavior

With a real Editor-generated package, the previous verification-only ProjectPlayer returned 0
with empty stderr when stdout inherited a read-only file handle. The sink stayed unchanged and
no report was written. This independently proves an unchecked stream failure. Verification now
explicitly flushes/checks stdout and returns 1 with a stderr diagnostic on failed report output.
Successful JSON, Runtime package ownership, ABI, persisted formats and dependencies stay unchanged.

The complete normal and optimized Editor fixture workflows inherit real read-only stdout handles,
require exit 1 and the diagnostic, and check unchanged sink/package bytes. Existing Unicode path,
complete report/schema/opaque/material and deterministic repeat-export checks remain mandatory.
The fixture driver also retains stage/exit and escaped prefixes of at most 2,048 characters per
stream plus original lengths on failure. Uncaptured stdout differs from captured empty output.
No report is repaired or trimmed, no exception is swallowed and no retry is introduced.

## Separate hosted observation

PR666 head `5558f2ad276a2344f77664b24bcfe372bcd04c10`, PR workflow39457822722,
Windows job118754343891, passed 326/327 in 1356.44 s: optimized verification exited zero but
json.loads rejected stdout at line 1/column 1. Original logs omit both streams; empty versus
non-JSON is unknown. Same-head push job118747959591 passed 327/327 in 1021.63 s, including
normal/optimized CLI 16.77/16.40 s. Both use Python3.14.8/windows-2025-vs2026.
A single targeted Windows retry118779165774 passed 327/327 in 1340.71 s, CLI 23.09/22.31 s.
The Linux read-only-handle reproduction does not establish causality for that intermittent
hosted failure. Its parent investigation remains open after this focused repair.

## Local validation

Linux x86_64/GCC14.2, Development Modular, Editor/graphical shell and verification-only
ProjectPlayer enabled; native ProjectPlayer, Slang and Zig gameplay disabled. Initial configure
uses these flags (plus the local pinned ImGui/Vulkan-header source cache):

```sh
cmake --preset linux-development -DNEXORA_ENABLE_EDITOR=ON -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON -DNEXORA_BUILD_PROJECT_PLAYER=ON -DNEXORA_ENABLE_PROJECT_PLAYER_NATIVE=OFF -DNEXORA_ENABLE_SLANG=OFF -DNEXORA_ENABLE_ZIG_GAMEPLAY=OFF
cmake --preset linux-development
cmake --build --preset linux-development
CTEST_PARALLEL_LEVEL=1 ctest --preset linux-development
```

The preparatory build passed 768 steps, then integrated latest main and rebuilt 416 steps.
The complete delivery-branch gate passed 291 registered tests: 279 actual passes, 12 existing
native feature/environment skips, zero failures, 609.72 s. No deadline/assertion was relaxed.
The 100k hierarchy lifecycle passed 227.98 s with timeout multiplier 1.

- `editor.static_project_export_cli_diagnostics`: 0.26 s; optimized: 0.66 s, six cases each.
  Actual child interpreters cover valid JSON, empty/non-JSON zero exits, stderr, nonzero valid
  JSON, uncaptured output and escaped/bounded control-character output. unittest checks remain
  active with optimization. These tests register independently of optional ProjectPlayer.
- Actual product `editor.static_project_export_cli`: 24.13 s; optimized: 24.38 s, including
  real read-only stdout failures and every original package/report/determinism assertion.
- Earlier focused normal/optimized workflows also passed against the actual built Editor producer
  and a directly compiled checked-output player using the unchanged Runtime/Foundation libraries.
- Documentation regression tests 16/16, changed Markdown/local links/bilingual pairing,
  touched C++ formatting and diff checks pass before publication.

Hosted delivery CI is recorded in the PR checks. No M7-03 root cause, Editor milestone or
physical-host acceptance is closed by this supporting delivery.
