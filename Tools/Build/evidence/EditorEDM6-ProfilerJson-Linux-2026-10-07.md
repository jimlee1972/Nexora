# ED Profiler JSON export: Linux evidence

Date: 2026-10-07. Parent: `643d46f056aaf624aa59510a90844553823abad5`.

The Profiler now offers a schema-1 JSON companion to its existing CSV export. The synchronous
ProjectWorkspace owner validates 1-600 finite, nonnegative wall-time samples with strictly
increasing nonzero frame IDs, blocks read-only/unresolved-recovery writes, and uses the shared
atomic replacement helper. Invalid captures and failed replacement preserve the last-good file;
an occupied staging path remains untouched. CSV bytes and its request API are unchanged.

JSON identifies source, measurement scope, milliseconds, project UUID and sample count. Frame
IDs and dropped-frame counts are decimal strings, preserving all uint64 values for JavaScript
consumers. Timings retain full double precision under the classic locale. GPU/memory availability
is false and sample values are null, including when supplied FrameSample fields contain values:
this export measures Editor frame processing wall time, not instrumented GPU or memory data.
The application consumes separate one-shot CSV/JSON UI requests before adding the current frame.
Neither UI nor exporter retains borrowed samples. New C++ methods are additive; public C SDK wire
contracts and public object layouts are unchanged. No new dependency or module boundary was added.

The actual Window pointer/button path exercises both controls at 1x/2x DPI, independent one-shot
requests and empty/read-only/recovery/close-modal gates. Service tests cover 600/601 sample limits,
invalid IDs/times, occupied staging, failed destination replacement, cleanup and retry. Independent
Python JSON consumers (normal and optimized interpreter) reject duplicate keys/nonfinite numbers
and verify the actual C++ export, including IDs above 2^53, UINT64_MAX counts and DBL_MIN/DBL_MAX.

Environment/isolated tools match
[import admission](EditorEDM1-ImportAdmission-Linux-2026-10-07.md). Commands:

```bash
cmake --preset linux-development
cmake --build --preset linux-development --parallel 4
ctest --preset linux-development -R 'editor.profiler_export|editor.profiler_json_schema'
ctest --preset linux-development
cmake --preset linux-shipping -DNEXORA_SHIPPING_PROFILE=Full -DNEXORA_ENABLE_EDITOR_SDK=ON -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=OFF -DNEXORA_ENABLE_SLANG=ON
cmake --build --preset linux-shipping --parallel 4
cmake --preset linux-development -B work/profiler-json-monolithic -DNEXORA_LINK_MODE=Monolithic -DBUILD_TESTING=OFF -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON -DNEXORA_ENABLE_SLANG=ON -DCMAKE_PREFIX_PATH="$PWD/work/sysroot/usr" -DCMAKE_CXX_FLAGS="-I$PWD/work/sysroot/usr/include" -DFETCHCONTENT_SOURCE_DIR_NEXORA_IMGUI="$PWD/build/linux-development/_deps/nexora_imgui-src" -DFETCHCONTENT_SOURCE_DIR_NEXORA_VULKAN_HEADERS="$PWD/build/linux-development/_deps/nexora_vulkan_headers-src"
cmake --build work/profiler-json-monolithic --target NexoraEditor --parallel 4
```

Development configure/build passed with the isolated X11/Vulkan sysroot, graphical shell and Slang
2026.18 enabled. Focused tests: **3/3 passed**, 0.30 seconds. Full CTest: **123/123 passed**, none
skipped, 205.02 seconds, with lavapipe and Khronos core/synchronization validation. Shipping/Full
configure/build passed. Shipping deliberately excludes standalone Editor/EditorCore, so enabling
its graphical shell fails the existing policy check; no policy was changed to bypass that check.
The separate Development/Monolithic build validates the complete graphical NexoraEditor link using
the same already-fetched ImGui/Vulkan header sources. Generated files and tool setup are uncommitted.

Source SHA-256:

- `Engine/Editor/src/ProjectWorkspace.cpp`: `19b78321eaefc4a7b84e77d06cd1535b2a901b6df5712efd3d3eeb3ea11bd677`
- `Engine/Editor/include/Nexora/Editor/EditorWorkspace.h`: `a10b0c0d78775dd10bafd3e9e9039fea35ade0961fde96263786d05ab5442d6b`
- `Engine/EditorImGui/src/EditorImGui.cpp`: `0acef960c1830f7358805dd7d1ee97d382f518d1c07ffcf60f096a145756f0e8`
- `Tests/EditorImGui/ProfilerExportTests.cpp`: `cf46032c359f9a3f1348c2224067b391253138f0f13e88773632965e50cf1bee`
- `Tests/EditorImGui/TestProfilerJson.py`: `ba041744a7e993911945ef240e6bfb7e65d56eb50085bf190ff0350b2bcfe4f4`

Capture import, measured GPU/memory traces and full ED-M6 workflows remain open. Automated DPI
contracts do not establish physical-device/installed-IME acceptance; graphical milestone acceptance
remains **0/8**. Hosted target checks are separate evidence and must pass before merge.
