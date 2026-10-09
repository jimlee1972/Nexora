# ED-M6 process-memory capture: Linux evidence

Date: 2026-10-09. Source base: `84a9580d1ac4dbb5c8dc54e9ad5a76ea46160399`.
Beads task: `nexora-62u.2.1`. This implements a supporting Profiler workflow; it does not accept
ED-M6 or any physical-display milestone.

The application already samples real current process RSS / working-set bytes at most once per
250 ms. This change retains up to `min(ProfileSession capacity, 600)` attempts in fixed storage,
with monotonic sequence, elapsed milliseconds, optional bytes and separate saturating eviction
counts. Reads that fail retain explicit unavailable attempts. The history crosses project changes;
the export-project UUID identifies the destination, not per-project allocation ownership.
Pause preserves elapsed gaps; Clear resets live history/origin while retaining pause state. Static
imports remain separate from live history and wall-time imports. Charts use elapsed time and break
lines at missing reads. All-unavailable histories display unavailable peak instead of a fabricated
zero measurement.

Schema-1 `.nexora/process-memory.json` exports source/metric/scope, byte/millisecond units, origin,
export-project UUID, count and decimal-string uint64 sequence/bytes/drop values. Unavailable bytes
are JSON null. Atomic publication respects project write/recovery gates and preserves last-good
files and unrelated staging. Read-only import is allowed. The 128 KiB nonrecursive reader requires
1–600 ordered samples and exact supported metadata; corruption, duplicate/unknown/missing fields,
nonfinite/overflow numbers, unsafe paths, wrong project and trailing data reject without mutation.
C++ consumers must rebuild for the class/API additions. Stable C/Gameplay ABI and module dependencies
are unchanged; existing wall-time CSV/JSON formats remain unchanged.

Host: Linux x86_64, GCC 14.2, CMake 3.31.6, Slang 2026.18, clang-format 19, Xvfb and Mesa lavapipe.
Development is Modular with Editor graphical shell, Slang and Zig enabled. Shipping is Minimal
Monolithic with Editor and BUILD_TESTING disabled, so its result verifies the existing engine
boundary, not an Editor Shipping application. Tools and SDKs are outside the repository.

Validation commands:

```bash
source /workspace/.nexora/env.sh
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping
ctest --preset linux-development -R 'editor.(process_memory|profiler|linux_process_memory)' --output-on-failure
git diff --check
```

The initial full graphical Development gate passed 179/179, no skipped tests, in 339.67 seconds.
Shipping configure/build passed. The focused native gate passed 7/7 in 1.22 seconds, including the
actual 24-frame Editor process with real OS memory sampling and native presentation. The final full graphical gate passed **181/181**, no skips, in **340.84 seconds**, including the
additional independent Python normal/optimized interchange, unavailable-only and metadata-path
fixtures. A sandbox-restricted Xvfb run
could not create its display; rerunning with the environment's permitted native network/socket
access passed without code changes.

Tests cover byte precision above 2^53 and UINT64_MAX, zero versus unavailable values, locale changes,
pause gaps, endpoint clocks, 600/601 samples, 128-KiB exact/over limits, every-byte truncation and
mutation preservation, equivalent JSON escapes, corrupt metadata, occupied staging and rejected
replacement. GUI pointer fixtures run at 1x/2x and cover independent one-shot export/import/clear,
failed publication, live/static independence, empty/live history, read-only, no project, project
changes and close/recovery modals. Python normal/optimized readers consume the actual C++ export
and validate the interchange independently.

GPU/allocator profiling, arbitrary capture import and physical Windows/macOS/Linux calibration or
visual acceptance are not covered by this evidence. Hosted CI results are tracked in the PR and
Beads; compile/test success must not be substituted for physical-host acceptance.
