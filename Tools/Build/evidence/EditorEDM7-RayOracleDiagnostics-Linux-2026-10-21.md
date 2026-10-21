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
Final required Linux and documentation results follow. No production ABI/dependency/linkage
boundary changes; this child does not change the underlying oracle or shader code.


## Required Linux acceptance

Private786b15a00e347b36e8ee4b186cef6d5ecd68b1dc/treea799195dd9152cf24efccfa6da8ccc9a54d71fe4
runs the required `cmake --preset linux-development`, `cmake --build --preset linux-development`,
`CTEST_PARALLEL_LEVEL=1 ctest --preset linux-development` after all329 consumers rebuild. Final
configure/build is current (no work remaining); **315 registered,303 actual passes,12 environment
skips,zero failures,708.46s**. All495 frozen production/test/shader/helper source hashes match before
and after the full suite, including pinned shared ImGui source paths. An initial hash-manifest
script rejected an external dependency path outside this worktree; no test/source was changed,
and the corrected manifest includes that absolute source path explicitly.

Deep Hierarchy69.73s,100k lifecycle227.75s and overview61.69s preserve original180/300 limits and
scale1. One bounded build may overlap in another own worktree; these shared-cloud functional results
are not isolated performance baselines. Only one full CTest suite runs at a time. The final focused
ray executable passes1/1 in0.02s and its actual output explicitly reports synthetic CPU diagnostic
verification and native ray/reference unavailable. The touched test CPP passes strict shadow syntax
checks and formatting/diff checks; documentation routing passes16 tests in1.418s. No linkage boundary
changed, so no new Editor-enabled Shipping claim is made. Hosted native Windows/macOS gates remain
separate; diagnostic delivery does not close `.38.1`, M7-03 or physical GPU acceptance.
