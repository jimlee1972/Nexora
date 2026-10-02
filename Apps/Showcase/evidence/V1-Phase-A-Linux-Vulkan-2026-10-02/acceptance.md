# V1 Phase A — Linux Vulkan virtual-display acceptance

Recorded on 2026-10-02 in Codex Cloud (Linux x86-64, GCC 14.2, Zig 0.14.0,
Xvfb 21.1.16, Mesa lavapipe 25.0.7). The application build ID is `d4e62a402a17`;
this acceptance includes the working-tree changes to the Python evidence gate and its CMake wiring.

✅ The Linux Phase A shell acceptance executed without skipping. The application opened an X11
window, presented its software-composited clear color, triangle and diagnostics through native
Vulkan WSI, requested 960x540 resize, recreated the swapchain, and unloaded gameplay before ordered
presentation teardown. `windowed.json` records four acquired/composed/presented frames, one resize
request and generation, `backend_fallback: false`, and a PASS lifecycle. The gate checks these
values explicitly; Python optimization cannot disable acceptance checks.

Validation commands and results:

```text
cmake --preset linux-development                         PASS
cmake --build --preset linux-development --parallel 4    PASS
CI=true ctest --preset linux-development                 PASS, 67/67; none skipped
cmake --build --preset linux-development --target NexoraShowcasePackageDevelopmentEvidence
                                                        PASS, 6/6 checksums, isolated-copy launch
python3 -O Tests/Showcase/LinuxVirtualDisplaySmokeTests.py PASS, 6 tests
```

The managed executor initially lacked build and display tools. Dependencies were installed under
`/workspace/tools` without changing the system. Initial configure enabled `NEXORA_ENABLE_ZIG_GAMEPLAY`
and selected the Zig executable, X11 headers/library and Vulkan loader explicitly in the ignored
build cache. PATH, PYTHONPATH, LD_LIBRARY_PATH, ZIG_GLOBAL_CACHE_DIR, MESA_SHADER_CACHE_DIR and
VK_DRIVER_FILES select those workspace tools, writable caches and lavapipe. `launch.json` preserves
the exact native launch command. `ctest.log` contains the complete test results; `package.log`
contains the checksum-verified package launch evidence.

Scope limits: this is a virtual-display composition acceptance, not a screenshot or physical-GPU
quality/performance claim. `scene_draws: 0` and `rendering_mode: cpu_composite` are intentional.
Vulkan GPU scene binding, graphical room content, guided tour, Windows Shipping/Full native
clean-machine acceptance, macOS/Metal acceptance and versioned visual captures remain open.
The existing aggregate 24% estimate is retained conservatively; accepting Linux Phase A does not
establish completion of the V1 Showcase.

Artifacts are preserved verbatim, including executor-local paths. `SHA256SUMS` covers this record
and the launch/report/log artifacts. Future CI runs keep their own evidence under
`build/<preset>/artifacts/showcase-linux-vulkan/` and upload it even when the gate fails. Before each
run the gate removes earlier evidence, so a missing Xvfb cannot leave a stale passing artifact.
