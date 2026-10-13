# Shared native geometry upload keys — 2026-10-13

Beads: `nexora-owg.1.4`. Base: `86b111db59932505bf9fef0eb4e74b048c0c7ee9`.

Vulkan, DX12 and Metal retain exact owning keys for the vertex/index upload prefix. The shared
private helper compares both byte spans, their boundary and destination offset. DX12's offset varies
with palette/shadow constant layout; geometry equality alone cannot justify reuse at a new offset.
Each adapter separately requires the same live sufficiently large GPU allocation, clears keys on
successful replacement/resize/teardown and retains them on failed growth. Instances/material/tone
data remain fresh. CPU key payload stays below5 MiB per slot, at most three slots, with one staged
candidate; existing GPU upload budgets remain unchanged. No public descriptor, diagnostics layout,
module dependency or stable C ABI changes.

Portable tests verify owning copies, changed vertex/index bits, identical concatenated bytes with a
different boundary, destination changes, retired allocation identity, failed/empty candidates,
cumulative counters and idempotent zero-retention teardown. Actual Vulkan tracing/pixels still cover
changing instances, instance-only growth and failed allocation; native PBR and Metal fixtures assert
real cache availability/hits, retained payload and released keys alongside existing pixel oracles.

## Validation

```bash
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping
```

Linux Development configure and the433-action build passed. The complete suite passed **246/246**,
zero failures/skips, in **833.60s**. Portable exact-key tests passed; actual Vulkan PBR passed6.97s
and fence/geometry tracing/pixels passed2.12s. Console passed7.61s. Shipping configure and the7-action
build passed. Seven changed C++/Objective-C++ files passed clang-format dry-run; documentation16/16,
Markdown links/bilingual pairs and diff checks passed. Fresh hosted results are recorded in the PR
and Beads after publication. Windows/DX12 and macOS/Metal runtime are hosted evidence, not Linux
execution. Per-asset residency and full ED-M2 acceptance stay
open; overall target acceptance remains0/8 and root README is unchanged.
