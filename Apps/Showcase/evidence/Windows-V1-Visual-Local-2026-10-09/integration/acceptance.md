# Windows current-main integration repair

The current-main merge `3b730fc5` lost the PreparedSave type and mixed preparation/publication
with the exact written-bytes Save overload. It also omitted the owning Runtime scene-capture
implementation. Restore the admitted contracts, retaining immutable prepared bytes, no-IO
preparation, generation/content revalidation, exact written bytes and failed-publication state.
The complete CaptureRuntimeScene implementation is byte-identical to accepted `88394c37`.

The static project loader now uses if constexpr for its fixed native-endian branch, avoiding
MSVC /WX C4127 while preserving the same little-endian rejection and validation policy.
DX12 diagnostics now report the mode already used by Present; the native test verifies Immediate
after acquire/present/resize. This does not guarantee display refresh or exclude compositor pacing.

## Validation

Visual Studio 2022/MSVC 14.44, repository-local Zig 0.14.0, bundled CMake 3.31.6-msvc6.
VsDevCmd x64 and bundled Ninja are injected into the task-local environment before the full gate.

```powershell
cmake --preset windows-showcase-development
cmake --build --preset windows-showcase-development --config Development --parallel 4
ctest --preset windows-showcase-development --parallel 4 --output-on-failure
```

All commands pass: **113/113 CTest, 296.04 seconds, no skips**. Native DX12 PBR,
prepared_scene_save, external_scene_save_contract, scene_runtime_capture and static_project_package
all pass. Initial compile/link failures and final configure/build/CTest logs are retained here.
Hashes describe the compiled patch against source base `577189ae`; production Shipping is frozen
separately after committing these sources. Linux full gate and final Windows Shipping/native
visual/performance/video acceptance remain pending. These are supporting portability/integration
repairs and do not accept any Editor milestone or final VIS art/performance.
