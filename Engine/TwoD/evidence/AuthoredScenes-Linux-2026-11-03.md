# Authored 2D scene integration — 2026-11-03

Scope: engine-owned ND2D catalog startup and deferred Replace/Additive/Unload through the existing
Scene V1 C/Zig service, plus 2D Studio scene-file selection. This is not main 3D Editor Sprite
persistence or per-scene script module binding.

## Executed Linux checks

- Standard `cmake --preset linux-development`, `cmake --build --preset linux-development` and
  `ctest --preset linux-development`: 263 configured, 0 failures; 255 passed and 8 explicitly
  skipped (physical X11 held-key and optional Slang renderer controls).
- Standard `cmake --preset linux-shipping` and `cmake --build --preset linux-shipping`: passed;
  Monolithic Shipping builds the 2D Player/compiler without Editor.
- `twod.scene_session`: shared document round-trip/truncation rejection, parent/metadata remap,
  failure/exception conservation, stable bridge routing, fresh IDs, additive/unload/persistent
  behavior, repeated replacement, wrong-thread rejection, cancellation and record retirement.
- `twod.scene_project_zig`: actual SDK adapter builds a Zig library, exports three documents,
  removes source files and runs the generic Player through replacement/additive/unload.
  Invalid scene export leaves no published package.
- `twod.scene_project_native`: the same real Zig/catalog runs 60 native Vulkan frames on Xvfb
  with Mesa llvmpipe. Title contains an embedded authored Sprite/Atlas. Passed separately and
  included in the final full gate. This is software native presentation, not physical GPU/art approval.
- Existing authoring/manifest/legacy-project tests retain compatibility. Portable SDK promotion
  regressions require both scene receipts for 0.3.0 while retaining 0.2.0 requirements.
- Additional Development build with `NEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON`: built
  `NexoraTwoDStudio` and `NexoraEditorImGuiTests`; `editor.imgui_contract` passed.
- Actual 2D Studio opened a two-file scene list with `--read-only --frames 12` on Xvfb/Vulkan;
  native preview/document lifecycle passed. This smoke verifies startup/rendering, not human
  scene-combo input acceptance.

The cloud worker initially lacked X11/Vulkan headers, a Vulkan ICD and clang++. Debian packages
were extracted under `/tmp`; normal feature settings/tests were retained. The Vulkan ICD was
selected explicitly from that temporary prefix. Initial environment failures were rerun after
repair; retained test logs are task-local, not repository build products.

## Remaining delivery gates

SDK 0.3.0 is a candidate until Windows Shipping producer, relocated Zig consumer, native DX12,
offline rebuild, all applicable CI and immutable release promotion pass. An actual published ZIP
hash is required before any downstream SDK lock change. No Windows/macOS/Android/iOS execution
is claimed by these Linux checks. Physical input/display/art/performance gates remain separate.
