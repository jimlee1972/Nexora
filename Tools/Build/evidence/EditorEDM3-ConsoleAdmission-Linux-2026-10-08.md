# ED-M3 bounded Runtime Console admission — Linux evidence

Date: 2026-10-08 UTC. Beads supporting task: `nexora-rd7.3.1`.
Source base: `06b11369daf2f9d0da86098df5343f8e00e122cf` (main, PR #440).

## Delivered behavior

`RuntimeConsole` previously bounded only record count: an oversized/malformed displayed field or
unknown severity could enter an owning snapshot and evict useful history. Admission now validates
category (256 bytes), source (1,024 bytes), message (16,384 bytes), NUL-free UTF-8 using the existing
Foundation decoder, and known severity. Byte limits are inclusive; empty fields and valid Unicode
scalars other than NUL remain accepted, without normalization or a new timestamp policy.

Requested record capacity is clamped to 4,096; zero retains its disabled-ingress behavior. Rejected
records increment the saturating dropped count without changing accepted history or consuming
accepted sequences. Oldest-record eviction occurs only after successful publication. The final
UINT64_MAX sequence can be admitted once; subsequent records reject permanently. Drop count includes
both rejections and evictions and saturates at UINT64_MAX. Accepted strings are rebuilt so a short
input cannot retain a producer's arbitrarily large reserved allocation. Allocation exceptions
propagate without changing ingress history/counters. Input allocations before admission and the
number/lifetime of returned owning snapshots remain caller responsibilities.

## Validation configuration and results

Linux managed cloud; GCC 14.2.0, CMake 3.31.6, Ninja 1.11.1, clang-format 19.1.7. Tools/dependency
packages are external to the repository. Default `linux-development`: Development, Modular,
`NEXORA_ENABLE_EDITOR_SDK=ON`, graphical shell OFF, Slang OFF. No user preset is required or committed.

```bash
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development
```

Final-source configure passed (0.5 s configure, 0.1 s generate), build passed (48 rebuild/link steps
following the initial 236-step full build), and full CTest passed **85/85**, **zero skips**, in
**22.40 s**. The full gate includes `runtime.console_record_admission` and the existing
`runtime.v1_m6_editor_sdk` behavior. Formatting all four touched C++ files passed
`clang-format --dry-run --Werror`; `git diff --check` passed.

The new test exercises:

- Exact ASCII and four-byte Unicode limits; one-byte oversize in each field, and a truncated final
  code point at the message byte limit.
- Embedded NUL in each field; isolated continuations, illegal leads, bad continuations, truncated
  two/three/four-byte encodings, overlong forms, surrogate encodings and scalars above U+10FFFF.
- Every accepted severity, negative/out-of-range severities, retained timestamp, overwritten caller
  sequence, empty fields, valid Unicode scalar boundaries and preserved newline/tab/control bytes.
- Full-capacity rejection preserving all fields/sequence, rejected records not consuming sequence,
  oldest-first eviction, zero capacity, and SIZE_MAX requested capacity clamping to 4,096 records.
- Producer string changes and later eviction leaving earlier snapshots intact; direct inspection of
  the production admission transaction confirms compacted owning strings release 1 MiB reserves.
- Four concurrent producers admitting 4,000 valid and rejecting 4,000 invalid records while a reader
  copies snapshots; retained sequences remain contiguous, final history contains 64 records, and
  dropped count is exactly 7,936.
- The same internal admission transaction at UINT64_MAX counters, without a public test/reset API:
  invalid input retains the last available sequence, the final valid sequence publishes once,
  eviction/rejection saturates drops, and exhaustion retains final history rather than wrapping.

## Compatibility and remaining acceptance

Runtime C++ record/class layout, module dependencies, stable C/Zig wires, scene formats and log routes
are unchanged. New constants and the inline constructor's explicit capacity clamp require C++ callers
to rebuild against the current header. Inputs exceeding the new budgets or containing invalid
severity/text are now rejected instead of being retained; callers can observe false and dropped count.

No linkage boundary changed, so Shipping is not required for this portable ingress change. This
evidence does not claim new GUI controls, complete Runtime/build log routing, native debugger
integration, physical-host PIE/focus acceptance, or Windows/macOS execution. Full ED-M3 acceptance
remains open. Both Editor roadmaps contain the same completed supporting implementation bullet; the
root README remains unchanged.
