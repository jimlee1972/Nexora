# ED-M6 native GPU timing: Linux evidence

Date: 2026-10-09. Final source base: `71daa3a5f2fb0efeb5aa92be3877cf05f158ed62` (memory PR #451).
Beads task: `nexora-62u.2.2`. This supporting implementation does not accept ED-M6 or a physical GPU
performance/calibration milestone.

The graphical Editor explicitly enables optional native GPU timing; default Surface/RenderSurface
consumers allocate no timestamp resources and record no timing commands/completion stream. Native
Presentation publishes a copied source, completed submission ID and optional milliseconds only at
existing GPU completion points. Vulkan uses two timestamp queries per fence-protected slot with
availability, native period and selected-queue counter bits. No query WAIT flag or extra GPU wait is
introduced. CPU completion duration is solely a single-wrap/range bound, never fallback GPU data.
DX12 uses queue frequency, per-slot queries and bounded readback after fences; every submitted list
is fenced even on DXGI presentation errors. Metal reads completed command-buffer GPU times with API
availability. Errors, unsupported/ambiguous counters and failed optional resources remain unavailable.
Resize/drain collect and release completed resources; abandoned recording generates no completed ID.

The owner ingests before UI Clear/Capture into a separate fixed history capped at 600 records, with
source/domain/software identity, optional latest/peak and independent eviction counts. Pause advances
only the completion watermark; Clear cannot replay an old result. Project changes retain the surface
history; new domains clear it. Native intervals are delayed command-buffer timing, not CPU frame time,
per-pass cost or display latency. Existing wall-time/RSS persistence is preserved; no GPU file format
or physical calibration is accepted here. C++ descriptors/classes must be rebuilt; stable C/Gameplay
ABI and module dependencies are unchanged.

Host: Linux x86_64, GCC 14.2, CMake 3.31.6, Slang 2026.18, clang-format 19, Xvfb and Mesa lavapipe.
The real 24-frame Editor reported source=Vulkan timestamps, software=1, 21 completed/retained GPU
intervals with finite latest native timing. This is a software Vulkan query result, not physical GPU
accuracy or a performance budget. Development is Modular with graphical shell, Slang and Zig enabled.
Both cached Minimal and Full Showcase Shipping configurations compile native Presentation with
Editor disabled; the new profiling option remains disabled by default for their consumers.

```bash
source /workspace/.nexora/env.sh
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development
ctest --preset linux-development -R '(gpu_timing|gpu_profile|vulkan_scene|process_memory|profiler)' --output-on-failure
cmake --preset linux-shipping
cmake --build --preset linux-shipping
cmake --preset linux-showcase-shipping
cmake --build --preset linux-showcase-shipping
build/linux-showcase-shipping/Apps/Showcase/NexoraShowcase --headless --frames=4
git diff --check
```

The required full Development configure/build/test gate passed **184/184**, no skips, in
**342.37 seconds**. Focused final integration/opt-in tests passed **15/15**, no skips, in
**4.64 seconds**. Minimal Shipping and Full Monolithic Showcase Shipping configure/build passed,
including the changed native Presentation adapter with Editor disabled. The Full Shipping
four-frame headless Showcase run also passed; its physical visual-room acceptance remains NOT_RUN.

Portable tests cover native period/frequency conversion, reduced/64-bit wrap, ambiguous bounds,
invalid/nonfinite/backward intervals, unsupported source and ordered copied publication. The real
Vulkan adapter call trace verifies optional pool allocation failure and unavailable read, no query
WAIT, fence-before-reset/read/free, resize and leak-free teardown including abandoned recording.
A separate default surface proves opt-out creates no pools or timing stream. Real Scene tests verify
finite query timing and latest submission after drain. 1x/2x GUI events and owner tests cover pause,
Clear watermark, valid zero/unavailable, capacity/drop counts, domain/source/software transitions,
project independence, copied ownership and detached owner display. Existing CPU/RSS/import tests pass.

Windows/DX12 and macOS/Metal implementations were not compiled or run on this Linux host; hosted
platform checks are tracked in the PR/Beads. Physical GPU calibration, per-pass tooling and versioned
GPU capture persistence/import remain open.
