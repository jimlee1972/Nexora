# ED-M0 Linux Vulkan validation slice

Date: 2026-10-06 (Asia/Taipei). Based on merged baseline `e8ebe86`; runtime fixes and
validation changes accompany this record. This closes the Linux automated validation slice,
not WP1 as a whole or ED-M0 acceptance.

## Reproduced failures and fixes

Enabling synchronization validation exposed `SYNC-HAZARD-READ-AFTER-WRITE` in the Editor
and native PBR frame. The acquired color transition allowed writes only, while UI/tone
render passes use `LOAD`. Acquisition now allows attachment reads and writes; the shared
UI/tone render pass explicitly orders earlier attachment writes before its load/blend access.

The native RHI offscreen triangle also emitted `VUID-VkShaderModuleCreateInfo-pCode-08737`
and `08740`: its Vulkan 1.0 device loaded canonical Slang SPIR-V 1.3 with `DrawParameters`.
It now requires Vulkan 1.1, checks `shaderDrawParameters` during physical-device selection,
and enables that feature at device creation. The GPU-driven gate additionally exposed
`VUID-vkCmdDrawIndirect-drawCount-02718`: multiple
indirect draws lacked `multiDrawIndirect`. Selection now checks and device creation enables
that feature as well. Unsupported candidates are rejected explicitly.

The Editor display harness previously discarded successful process output and accepted
exit-zero validation errors. It now retains both streams and rejects validation errors on
either stream, including the intentionally killed recovery process. Four subprocess regression
tests cover preservation, both-stream exit-zero errors, scenario-controlled nonzero exits,
and ordinary warnings. Cleanup-only process waits retain their existing behavior.

## Reproduction

The workspace toolchain and software display are the same as the
[baseline record](EditorEDM0-Linux-2026-10-05.md). Added validation layers: 1.4.309.
The extracted runtime also needs its `VK_LAYER_PATH`; a normal system installation does not.

```bash
export VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation
export VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT
export VK_LOADER_DEBUG=layer
cmake --preset linux-development -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON -DNEXORA_ENABLE_SLANG=ON
cmake --build --preset linux-development
ctest --preset linux-development -V
cmake --preset linux-shipping
cmake --build --preset linux-shipping
```

The loader log confirms insertion of the validation layer into instance and device chains.
Focused PBR, Editor display, and validation-output tests passed 3/3 after the synchronization
fix. The native RHI test passed with no validation diagnostics after feature negotiation was fixed.
Final Development configure/build passed; CTest **116/116 passed**, none skipped, in 173.48 sec.
The retained full-suite output contained no `Validation Error`, `VUID-*`, or `SYNC-HAZARD-*`
diagnostics. Shipping configure/build passed (testing is OFF by that preset).
Changed Markdown validation reported zero errors; documentation CI regression tests passed 16/16.

CI installs the validation layer, enables synchronization validation in focused shell/native passes,
requires loader evidence
of device-layer insertion, retains verbose Editor
and native logs as `editor-linux-validation-logs`, and fails on diagnostics even when the
native RHI test exits zero. Native coverage includes offscreen RHI, scene rotation pixels,
PBR, and GPU-driven indirect submission; the Editor harness covers startup, resize,
layout repair/migration, and recovery.

The first hosted CI run exposed fixed-delay gesture/Play failures under validation overhead,
without Vulkan diagnostics. The complete Editor interaction suite remains required in its normal
configuration; a separate core/synchronization pass repeats the ED-M0 display/recovery gate.
Both configurations retain logs. This does not claim synchronization validation for every hosted
Scene/Game interaction; the local full-suite layer-enabled result above remains separate.

## Acceptance limits

Xvfb/lavapipe are a virtual display and software Vulkan device. Physical Linux display,
Windows/DX12 DPI and IME, debugger integration, and remaining work-package acceptance
remain open. No Windows, macOS, mobile, physical-display, or sanitizer result is implied.
