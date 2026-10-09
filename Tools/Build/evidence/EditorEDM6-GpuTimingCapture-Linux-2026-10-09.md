# ED-M6 native GPU capture: Linux evidence

Date: 2026-10-09. Beads task: `nexora-62u.2.3`. Source dependency: native GPU PR #452
merged as `7bc2ba2fe5e5c2a7bf52dd956214c96e85246e75` after all 36 hosted checks,
based on memory PR #451. This supporting
implementation does not accept ED-M6 or a physical GPU calibration/performance milestone.

The graphical owner now explicitly exports/imports `.nexora/gpu-timing.json` independently of
wall-time CSV/JSON and process RSS. Schema 1 records the native Vulkan/DX12/Metal timing source,
software-device flag, native command-buffer scope, milliseconds, export-destination project UUID,
completed submission axis and retained/dropped counts. Submission/drop IDs are decimal uint64
strings, while optional finite nonnegative durations are numbers or null. Valid zero stays measured;
an all-unavailable capture is valid for a known source. Native handles/domain IDs, per-pass cost,
CPU fallback duration, display latency and process allocation ownership are not persisted/inferred.

Shared validation admits 1–600 ordered samples. Synchronous writer-gated atomic publication and
128-KiB read-only-compatible import reject closed/recovery projects, unsafe paths, wrong metadata,
corrupt/duplicate/missing/unknown fields, unordered IDs, overflow and trailing bytes. Failures retain
last-good files, unrelated staging and prior owning imports. The JSON token grammar is now shared
by all three distinct schemas, retaining nonrecursive bounded parsing and their original contracts.

Independent Export GPU / Import GPU / Clear GPU import controls follow access/modal/recovery/close
gates. The host owns its static vectors without a retained live borrow. Static/live clear and imports
do not mutate CPU/GPU/RSS histories or other imports; project root/UUID changes and detachment clear
static state/pending requests while live surface history remains. Static plots identify source,
software status, units, scope, dropped count and retained peak, using submission IDs with null gaps.

Host: Linux x86_64, GCC 14.2, CMake 3.31.6, Slang 2026.18, clang-format 19, Mesa lavapipe/Xvfb.
Development is Modular with graphical shell, Slang and Zig enabled. Cached Minimal Shipping is
Monolithic with Editor disabled and native Presentation enabled. These additive C++ APIs require
rebuilding their consumers; module dependencies and stable C/Gameplay ABI are unchanged.

```bash
source /workspace/.nexora/env.sh
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development
ctest --preset linux-development -R '(gpu|process_memory|profiler|frame_processing)' --output-on-failure
cmake --preset linux-shipping
cmake --build --preset linux-shipping
git diff --check
```

Before final dependency integration, the complete Development configure/build/test gate passed
**188/188**, no skips, in **352.28 seconds**. Focused tests passed **19/19**, no skips,
in **2.59 seconds**. After rebasing onto the accepted #452 main revision, the final
serial Development configure/build/test gate passed **190/190**, no skips, in **352.10 seconds**;
focused tests passed **19/19**, no skips, in **2.58 seconds**. Minimal Shipping configure/build
passed. `git diff --check` passed.

An earlier simultaneous run of two full graphical gates passed 188/190, with two Editor-window
startup failures. Isolated reproduction of both fixtures passed **2/2** in **58.28 seconds**,
then the complete gate passed serially with unchanged implementation and fixture deadlines.
Future native gates are serialized on this four-CPU cloud host.

Tests cover zero/null, all three sources and software flags, uint64 extremes, full double/locale
precision, every-byte truncation/mutation, exact/over byte and sample limits, invalid schema/fields,
source/project mismatches, independent capture preservation, read-only/recovery/closed projects,
aliased/nonregular metadata/files and occupied file/directory staging. Actual C++ exports pass
independent normal/optimized Python JSON readers. 1x/2x pointer events exercise one-shot actions,
owning snapshots, failed publication, empty history, independent clears, all-null source transition,
read-only/modal/recovery guards, project switch request cancellation and detachment. The real Linux
Editor process continues publishing native GPU/RSS evidence and proves no capture is implicitly saved.

Windows/macOS were not executed locally; hosted platform checks are tracked in the PR/Beads.
Physical GPU calibration, per-pass analysis and third-party capture formats remain open.
