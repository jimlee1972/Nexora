# V1 Phase B — Linux Vulkan Rendering Room acceptance

Recorded on 2026-10-02 in Linux x86-64 Codex Cloud, using GCC 14.2, Zig 0.14.0,
Xvfb 21.1.16 and Mesa lavapipe 25.0.7. Application build ID `e3eb6db69971` identifies
the base commit; the executable includes this working patch's native Vulkan scene and input fixes.

✅ The Rendering Room now uses native indexed, depth-tested GPU geometry with Lambert lighting,
row-major camera transforms and per-frame resources. Its 12-frame gate records matching
`scene_draws` and `native_scene_draws`, 12 acquires/presents, a resize generation, no backend fallback,
zero software-composited frames, and ordered gameplay/presentation shutdown. The 600-frame keyboard
gate injects normalized X11 D-key input and requires an observable public camera Transform update.
The previous ASCII key comparison did not match `Window::Key` and is corrected.

The native pixel contract independently verifies depth occlusion regardless of triangle order,
lighting, matrix translation, two resize generations, reuse of all frame slots, rejected invalid
geometry/nonfinite matrices/wrong-thread calls, rejected duplicate/mixed draw paths, and idempotent
ordered teardown, including cancellation of an acquired frame and rejection of subsequent GPU calls. Pixel readback stays in the test, not the render loop. Khronos validation-layer
reruns pass without Vulkan validation errors; the runner also fails if such an error is logged.

| Validation | Result |
| --- | --- |
| `cmake --preset linux-development` | PASS |
| `cmake --build --preset linux-development --parallel 4` | PASS |
| `CI=true ctest --preset linux-development` | PASS, 70/70; none skipped |
| `python3 Engine/Presentation/shaders/GenerateSceneShaders.py --check` | PASS; both SPIR-V stages validated and header reproduced |
| Three native scene/input gates with `VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation` | PASS, 3/3; no validation errors |
| `NexoraShowcasePackageDevelopmentEvidence` target | PASS, 13/13 checksums, seven staged Engine DSOs and isolated headless launch |
| `cmake --preset linux-shipping -DNEXORA_SHIPPING_PROFILE=Full` | PASS; Zig and workspace dependency selectors also supplied |
| `cmake --build --preset linux-shipping --parallel 4` | PASS, Shipping/Monolithic |
| `NexoraShowcasePackageShippingEvidence` target | PASS, 5/5 checksums and isolated headless launch |
| Shipping/Full native launch from an independently staged, reverified package copy | PASS, 5/5 checksums and 12 Vulkan GPU draws |
| clang-format of touched C++ and `git diff --check` | PASS |

The same workspace dependency selectors and writable-cache environment from the Phase A acceptance
are used. glslangValidator 15.1.0, spirv-val 2025.1 and clang-format generate the private scene header;
normal builds consume the checked-in header without these shader tools. The three focused tests are
`showcase.linux_vulkan_rendering_room`, `showcase.linux_vulkan_camera_input`, and
`window_presentation.vulkan_scene`. Shader generation and native GPU-room availability do not depend
on enabling the graphical Editor or Slang. `shipping-config.json` records Full/Monolithic selection. Linux Development packages now include all seven transitive Engine DSOs, use `$ORIGIN` for relocation, and reject build-tree dependency fallback before launch. A real ELF fixture proves that the old copied-executable launch could succeed while reading an external library and that the new verifier rejects it. Development and Shipping native package copies both pass 12 GPU frames. The preceding Phase A Development package record was checksum-only and does not establish Engine-library isolation; this record supersedes that limitation.

`interactive/rendering-room.png` is a capture of the live 600-frame Vulkan cube window before native
D-key injection, with command/report evidence beside it. `capture-showcase.py` preserves the local
capture procedure (Pillow/XCB and xdotool); it is supplemental visual evidence. `scene/scene.png` is
the native pixel test's retained frame converted losslessly from PPM to PNG. The CTest contract,
not a screenshot, determines correctness. Other subdirectories retain exact native launch logs,
reports, and Shipping/Full package staging evidence; `ctest.log` and `validation-layer.log` retain
the final gate results. Artifact paths intentionally preserve the original executor/staging paths.
`SHA256SUMS` protects this acceptance bundle.

This accepts the Linux/Vulkan Rendering Room binding, not all of Phase B or the full V1 Showcase.
The default Hub and other rooms still need complete visual content, the native scene has no GPU-room
UI overlay yet, and guided tour/native RenderGraph integration, physical-GPU performance,
Windows Shipping/Full clean-machine acceptance, Windows/Vulkan and macOS/Metal remain separate open
gates. The existing aggregate 24% estimate is retained conservatively. No GitHub Actions run,
Windows, macOS or mobile validation is claimed by this Linux cloud evidence.
