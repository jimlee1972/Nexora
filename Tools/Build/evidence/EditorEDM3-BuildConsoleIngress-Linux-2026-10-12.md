# Scoped local build output ingress — Linux cloud, 2026-10-12

Beads: `nexora-rd7.3.3`. Base: `20fe7ad9461d3d362c822c62d8cc6b9e122c85b3`.

Real local process output is forwarded to the owning graphical Runtime Console with project
scope/operation identity and wall-clock receipt timestamps. Control and non-ASCII bytes are
preserved as hex escapes. No severity, artifact validity or build success is inferred. Each
owner poll consumes at most 16 KiB raw bytes in four bounded messages, plus one source-loss warning.
Raw-byte loss remains distinct from Console rejected/evicted record counts. Scope changes,
retained-tail movement, older/regressed snapshots, binary bytes, chunked draining, zero-capacity
Console and snapshot budgets/overflow have executable conservation checks. The actual Xvfb
Build Console fixture requires nonzero delivered/retained Console records from real child output
and preserves saved authoring bytes through run, fail, cancel and close.

Output remains session-local. There is no new persistence/transmission or command/environment
capture. The complete Runtime/build/device log system and native debugger integration remain
open; this accepts one cloud routing slice, not full ED-M3 or physical-host milestones.

## Executed validation

The complete Linux Development gate passed **244/244**, no failures/skips, in **839.01 s**:

```bash
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development
```

Focused ingress/Core/native process checks passed3/3 in7.41s; the actual native process fixture
passed again in the full suite in7.38s. The first focused binary fixture caught a missing source
scope/operation label; the corrected implementation passed both focused and complete gates.
Native scenarios preserved the exact saved authoring scene bytes. Shipping Monolithic configure
and build passed with Editor stripped and testing OFF:

```bash
cmake --preset linux-shipping
cmake --build --preset linux-shipping
```

Three changed C++ files passed clang-format dry-run; documentation routing passed16/16.
GCC14.2, Dear ImGui, Xvfb and Mesa lavapipe were used with the existing external task-local SDK
paths. No Windows/macOS/physical GPU acceptance was run locally. Root README is unchanged.
Hosted CI results are recorded separately in the PR/Beads and are not inferred from local gates.

## Validated source digests


- `Apps/Editor/BuildConsoleIngress.h`: `7ca1c31fa3d682b9c0520fe42099e689e32d54bfacc97a7155863112722190ad`

- `Apps/Editor/main.cpp`: `b836f48b10a1e32fc032d70c19b626859696a9c31766b17a1236d3528d5d847b`

- `Tests/EditorImGui/BuildConsoleIngressTests.cpp`: `bc072be68aae266cf97aff001fe509da6955799f2156669a2c3d4d3850e27ecd`
