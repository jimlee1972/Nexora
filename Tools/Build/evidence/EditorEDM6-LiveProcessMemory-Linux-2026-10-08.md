# ED-M6 live process resident memory: Linux evidence

Date: 2026-10-08. Source parent: `69f2d85246e76628e12e7a68c4f91493a48ec3ca`.
Beads task: `nexora-032.2.1`. This evidence covers the companion live-memory implementation,
not acceptance of the complete ED-M6 milestone.

Core's `CurrentProcessResidentBytes` observes current process RSS / working-set bytes, including
shared resident pages. Linux reads a bounded `/proc/self/statm` response and converts resident
pages using the native page size with checked uint64 multiplication. Windows and macOS use their
native process APIs, but those branches were not compiled or run in this Linux environment.
Failed/unsupported observations return unavailable. Linux RSS accounting is an OS observation
that can include batched counter updates, not exact allocator accounting or GPU memory.

The application calls the sampler only after a successful graphical Present. ProfileSession owns
separate optional latest/observed-peak values and attempt/success counters; reads are throttled to
one per 250 ms on a monotonic clock. The graphical Profiler copies those scalar observations.
Capture pauses OS reads and retains previous observations. Clear resets observations and the
sampling timer without changing pause state; the next capturing frame may sample immediately.
Failed reads make the latest value unavailable while retaining the observed peak. Scope is
process-wide across project switches and detachment; the peak is the largest successfully observed
value since Clear, not the OS lifetime peak. Counters saturate and the clock's upper boundary is
handled without overflow. The existing FrameSample and schema-1 wall-time CSV/JSON formats remain
unchanged and exclude these live RSS observations. No stable C/Zig ABI or module dependency is added;
the public Core function and Editor C++ class/API additions require C++ consumers to rebuild.

Host: Linux x86_64, GCC 14.2, CMake 3.31.6, Ninja
`1.11.1.git.kitware.jobserver-1`, Slang 2026.18, clang-format 19.1.7,
Xvfb 21.1.16 and Mesa lavapipe 25.0.7. Tools/SDKs were installed outside the repository.
Development is Modular with graphical shell and Slang ON; Shipping is Monolithic with Editor
graphical shell, Slang and testing OFF. Xvfb uses a real local X11 server; this is virtual-display
evidence, not physical display or Windows/macOS acceptance.

```bash
cmake --preset linux-development -DNEXORA_ENABLE_SLANG=ON -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON
cmake --build --preset linux-development -j 4
ctest --preset linux-development -R '^(core.process_memory|editor.(process_memory_profile|process_memory_display|linux_process_memory|profiler_export|profiler_json_schema|profiler_json_schema_optimized))$' --output-on-failure -V
cmake --preset linux-development
cmake --build --preset linux-development -j 4
ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping -j 2
```

Results: focused gates **7/7 passed** in 0.96 seconds; full Development CTest **141/141 passed,
zero skipped**, in 212.49 seconds. Development configure/build succeeded (332 initial build steps;
the final required build reported no additional work). Shipping configure/build succeeded,
72 build steps; Shipping has no tests by preset.

- `core.process_memory` validates decimal token/unit/overflow/error handling and uses a real Linux
  anonymous 16 MiB mapping with one volatile write per native page. Measured RSS rose from
  **1,953,792 to 18,747,392 bytes** after touching 16,777,216 bytes. The assertion allows native
  counter batching and requires substantial actual residency, not a mocked value.
- `editor.process_memory_profile` covers exact throttle boundary, unchanged copied snapshots,
  failed latest/retained peak, valid zero versus unavailable, pause/clear/resume, clock rollback/
  upper boundary, uint64 byte precision and the production default OS reader. FrameSample GPU/
  memory fields remain zero and wall-time capture behavior remains separate.
- `editor.process_memory_display` exercises real ImGui frames and Window pointer/button events
  at 1x/2x DPI. Capture/Clear controls, copied ownership, failure, project switch/detach and missing
  session cleanup are covered without widget-initiated OS reads. Injected reader values in these
  owner/UI contract tests are not claimed as native measurement evidence.
- `editor.linux_process_memory` launches the actual Editor for 24 frames under Xvfb. The bounded
  owner diagnostic reports scope `current_process_resident_set`, bytes, **1 attempt/1 success**,
  latest/observed peak **96,821,248 bytes**. Renderer diagnostics report **24 acquired/presented
  frames and 187 native UI draws**, with no implicit CSV/JSON write. Values vary by process/host;
  they are recorded observations, not performance budgets.
- Existing CSV/JSON export/import and independent normal/optimized Python schema-1 parsers pass.

Implementation SHA-256 at validation:

| File | SHA-256 |
| --- | --- |
| `Engine/Core/src/ProcessMemory.cpp` | `9891e8b877de2590796c1422e63bdc8c2c2c5019f387c8746c8ac201a41a58ca` |
| `Engine/Editor/src/EditorProduction.cpp` | `8a6b66ebf26d51c4afb15a0e3518582e366ba92081fec1b3f3eeb047c37f875c` |
| `Engine/EditorImGui/src/EditorImGui.cpp` | `50d4986e687d89b85bcb3c5b42477fceddd2497cf8504a1f673109e7bc89cd7a` |
| `Apps/Editor/main.cpp` | `77fdb6c1a96b87d70d8493e58c0cd4d86a521349295caaabf2793a76ab976d88` |

Saved/versioned memory traces, GPU timestamps and GPU-memory accounting, allocator attribution,
remote profiling, production budgets/soak, physical-display and Windows/macOS host validation
remain open. No ED milestone acceptance flag or root README progress summary changes.

## Integration with accepted main

The RSS implementation commit `b9b180441b37d36f028ada335460a23cfba33261` was integrated with
main `06b11369daf2f9d0da86098df5343f8e00e122cf`, which includes the accepted atomic Inspector
batch change. The resolved Editor test registration retains both `editor.process_memory_profile`
and `editor.inspector_atomic_batch`, each registered once. The four implementation SHA-256 values
above remain unchanged, and all twelve RSS production/test source files match the RSS implementation
commit. The RSS slice changes only its integration evidence and combined test registrations;
the accepted main separately adds the Inspector adapter implementation and its tests/documentation.

The resolved integration tree passed the required gate with graphical shell and Slang ON, Modular
Development, and the same Linux/Xvfb/lavapipe environment:

```bash
cmake --preset linux-development
cmake --build --preset linux-development -j 4
ctest --preset linux-development
```

Configure and build succeeded (196 rebuild/link steps). Full CTest passed **142/142, zero skipped**,
in **215.35 seconds**, including both new feature gates, actual Linux process RSS, native graphical
flows and the existing schema-1 capture regressions. `git diff --check` passed. No source changed
while this gate ran. The earlier successful 72-step Shipping build continues to cover the unchanged
Core memory implementation; Shipping was not rerun for this Editor integration.
