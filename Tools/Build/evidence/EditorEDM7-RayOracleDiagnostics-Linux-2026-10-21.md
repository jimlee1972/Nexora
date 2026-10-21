# Native ray oracle failure diagnostics — Linux, 2026-10-21

Beads `nexora-gu7.38.1.1` is a finite diagnostic child; intermittent Windows ray mismatch `.38.1`
and overall M7-03 remain open. No production correctness repair or physical GPU acceptance is claimed.

Own PR688 head8d4181df push workflow39573377982/job119133244334 passed the initial WARP ray
preflight, then failed only `renderer.ray_tracing` in the full353 suite: ray distance differs from
CPU traversal,0.34s, after mode1/triangles2 and instances1/BLAStriangles8. Vendor5140/device140,
software1/refit0; enhanced-barrier variant passed. Suite duration1121.68s. Renderer/RHI/Core/
Foundation and ray shader/test inputs were unchanged by that seven-file Editor feature. This is
consistent with recorded same-head native variance, not proof of a driver bug or any root cause.
The original failure remains in Actions; no source assertions/timeouts are weakened.

The test now retains bounded owning workload sources and creation-site metadata. Real Record
captures the actual native-versus-compute dispatch path without changing command recording.
Comparisons print only on existing oracle failure, retaining backend/mode/vendor/device/driver/
software/barrier policy, creation function/line, exact ray, captured original geometry and expected/
actual decoded floats plus32-bit representations. Old snapshots remain distinguishable after rebuild.
Each parallel recorder owns a separate Workload and comparison follows cohort draining.

Synthetic CPU-only cases in the existing executable corrupt primitive, distance, each barycentric,
canary and NaN. Every existing assertion must still reject, expected/actual/input/site evidence must
remain present, and matching output must remain quiet. This validates actual comparison/reporting
code without claiming native Windows execution. Initial registered focus1/1 passes0.01s; native
ray/reference artifacts are unavailable in the local Slang-disabled profile and reported honestly.
Full required Linux and documentation results are pending. No production ABI/dependency/linkage
boundary changes; this child does not change the underlying oracle or shader code.
