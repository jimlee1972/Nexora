# Nexora Window and Native Presentation Roadmap

> Version: v1.0 | Status: planning baseline | Updated: 2026-10-02

> **Progress: implementation complete** (WP-M0 through WP-M4 are implemented. Windows/DX12 acceptance for WP-M1/WP-M2 is recorded; Linux Showcase Vulkan composition now has non-skip Xvfb/lavapipe acceptance; physical-display, Windows/Vulkan, and macOS/Metal still require target-host runners.)

## 1. Purpose and ownership

This roadmap owns the missing OS-window and window-system presentation boundary shared by the Zig
Showcase, the graphical Editor, and future game executables. RHI continues to own GPU resources and
queues; a platform window module owns native handles and events; the presentation adapter owns the
swapchain and converts window changes into backend-neutral surface events. Zig gameplay never
receives a native window, device, queue, or swapchain pointer.

## 2. Current baseline

- ✅ X11 modifier events report post-transition flags, preserve held left/right partners, and
  clear tracking on focus loss/destruction. The native Xvfb gate verifies all four modifier families.

- ✅ Validation and native RHI devices execute deterministic offscreen workloads.
- ✅ Renderer scene extraction and offscreen `Present` state validation are covered by tests.
- ✅ `NexoraShowcase` preserves its headless lifecycle and can own a reusable native render surface.
- Implemented: Win32 window/input translation and DX12 presentation have WP-M1/WP-M2 Windows acceptance evidence; Showcase integration remains a separate target-host gate.

## 3. Required contracts

- Backend-neutral `WindowDescriptor`, `WindowHandle`, `WindowEvent`, and `SurfaceDescriptor` types.
- Explicit ownership: the application owns the window; the presentation surface cannot outlive it;
  GPU work is drained before swapchain or window destruction.
- Threading rules for event pumping, resize, fullscreen transitions, and render submission.
- Recoverable results for unsupported backend, surface loss, out-of-date swapchain, minimized/zero
  extent, device loss, and display/DPI changes.
- Input snapshots derived from timestamped window events without exposing platform message structs.
- Native types remain in private platform/backend translation units.

## 4. Milestones

### ✅ WP-M0 — Contract and module boundary

- Define the public window/surface descriptors, events, errors, ownership, and threading contract.
- Add feature options and module-graph declarations without making headless builds depend on a window SDK.
- Add fake-window and fake-surface lifecycle, resize-coalescing, zero-extent, and teardown tests.

Delivered evidence: `Nexora::Window` and `Nexora::Presentation` expose only backend-neutral public
types; their ownership, lifetime, threading, resize, and recovery rules are recorded in module
READMEs. Both modules are guarded by `NEXORA_ENABLE_WINDOW_PRESENTATION`, their dependencies are in
the validated module graph, and `window_presentation.contracts` exercises the fake lifecycle gate.
No native window or swapchain backend is claimed by this milestone.

### ✅ WP-M1 — Win32 window and input

- Create/close/show/resize a Win32 window with DPI-aware client sizing and a deterministic event pump.
- Translate keyboard, text/IME, pointer, wheel, focus, and close events into backend-neutral snapshots.
- Cover repeated create/destroy, resize storms, minimize/restore, and shutdown while events are queued.

### ✅ WP-M2 — DX12 swapchain presentation

- Create, acquire, render to, resize, and present a DXGI swapchain without leaking DXGI/D3D12 types.
- Define backbuffer/fence ownership, frames in flight, vsync/tearing policy, color format, and present diagnostics.
- Handle occlusion, zero extent, surface loss, device removal, and failed resize without corrupting the active generation.

Windows acceptance evidence is produced by `window_presentation.contracts`: it creates a real Win32 window, acquires, clears, and presents four DX12 frames, resizes the swapchain, and asserts diagnostic counters. On 2026-09-28, the local x64 `windows-dx12-development` preset built the native D3D12 path with Vulkan disabled and passed this contract, recording the WP-M1/WP-M2 target-host evidence. Linux contract results or screenshots alone are insufficient.

### ✅ WP-M3 — Showcase and Editor integration

- Run `NexoraShowcase --mode=interactive --backend=dx12` with visible output and input.
- Provide reusable render surfaces for Scene/Game views without making Runtime depend on Editor.
- Preserve the deterministic Linux headless path and make fallback reasons visible rather than silent.

Delivered evidence: `NexoraShowcase --mode=interactive --backend=dx12` creates the reusable
Presentation-owned `RenderSurface`, pumps input, and presents until close; bounded frame runs are
available for automation. The same public owner is usable by Editor Scene/Game views without adding
an Editor dependency to Runtime. Auto fallback emits and records its reason, explicit DX12 failure
does not fall back, and non-Windows contract tests retain the deterministic headless gate. Actual
Windows/DX12 execution of the Showcase command remains target-host acceptance evidence rather than a Linux-cloud claim.

### ✅ WP-M4 — Additional platforms and hardening

- Add Vulkan window-system surfaces on supported Linux/Windows hosts and Metal presentation on macOS.
- Validate multi-window/multi-surface lifetime, HDR/color-space negotiation, fullscreen, hot-plug, and long-run resize/device-loss stress.
- Record target-host evidence separately; cross-compilation alone is not runtime validation.

Delivered evidence: Linux uses an X11 window implementation and Vulkan WSI swapchain; Windows can select Vulkan alongside DX12; macOS uses a Cocoa window and `CAMetalLayer`. Backend negotiation records the selected present mode and color space, while fullscreen, multi-surface lifetime, 2,048-cycle resize stress, zero extent, out-of-date, surface-loss, and device-loss paths are covered by the portable contract gate. These sources and cross-platform contracts complete the implementation scope; WP-M1/WP-M2 Windows/DX12 runtime acceptance is recorded, and ✅ Linux Showcase Vulkan composition passes non-skip Xvfb/lavapipe acceptance on 2026-10-02 (67/67 Development tests). Physical-display and other platform runtime acceptance remain explicitly separate target-host gates. See the [acceptance record](../../Apps/Showcase/evidence/V1-Phase-A-Linux-Vulkan-2026-10-02/acceptance.md).

## 5. Validation and Definition of Done

- Unit/contract tests cover ownership, event ordering, resize, zero extent, and failure injection.
- A Windows/DX12 runner proves real acquire/render/present and captures diagnostics; a screenshot is
  supplemental visual evidence, not the correctness oracle.
- Linux headless configure/build/test remains independent of desktop display availability.
- Windowed shutdown produces no live GPU resources, queued callbacks, or native handles.
- The Showcase and Editor consume only public window/presentation contracts.

✅ The Linux/Vulkan scene boundary also passes native indexed/depth/light/transform pixel acceptance and 600-frame normalized camera-input integration under Xvfb/lavapipe (70/70 Development tests). See the [Phase B acceptance record](../../Apps/Showcase/evidence/V1-Phase-B-Linux-Vulkan-2026-10-02/acceptance.md). Physical-display and other-platform gates remain separate.

## V1 Visual Showcase follow-through (2026-10-03)

✅ The Linux/Vulkan application now presents eight live Runtime room views, readable native GPU UI,
held keyboard/pointer controls and a 210-second guided tour under Xvfb/lavapipe. Full package tooling
includes content, deterministic ZIP/SHA-256 and isolated Linux shared-library resolution. These are
V1 Visual Showcase integration results; they do not replace this roadmap's existing platform gates
or establish the expanded Windows version's clean-machine graphical acceptance.

✅ Native SceneDrawData hardware instance records and bounded fence-owned uploads are implemented in Vulkan/DX12. Linux Vulkan pixel acceptance verifies independent translation/scale/tint; DX12 target-host execution remains pending.

✅ Native Vulkan/DX12 SceneDrawData draws now accept an in-bounds physical-pixel viewport for a docked Scene View. Portable bounds checks and Linux Vulkan pixel readback verify clipping; Editor integration and DX12 target-host viewport evidence remain open.

✅ Scene UV/RGBA8 material binding now has bounded immutable scene-only texture IDs, white fallback and fence-protected upload lifetime in Vulkan/DX12. Linux pixels verify sampler selection, cache reuse and resize resubmission; target-host DX12 material execution remains pending.

✅ Native-owner Offscreen → Main → UI → Present now uses fence-owned scene color and GPU copy in Vulkan/DX12, with duplicate acquire and pending-copy ordering rejected. Linux Vulkan pixel, interaction and synchronization-validation gates pass 75/75. The packaged Windows `accept-v1.ps1` records isolated-copy screenshots, checksums and native counters; the user will perform expanded physical-display/clean-host acceptance locally.

✅ Expanded Windows Full Shipping/DX12 isolated-copy graphical acceptance passes in [CI run 37053279518](https://github.com/jimlee1972/Nexora/actions/runs/37053279518): 14 checksums, 12 visible screenshots, sampled asset/plugin rejection, 339 native graph/copy/present frames and exit code 0. [Versioned Windows CI evidence](../../Apps/Showcase/evidence/Windows-V1-Native-Graph-CI-2026-10-03/acceptance.md). The hosted VM does not accept physical-display or independently provisioned clean-host gates; these remain the user’s local work.

✅ Windows DX12 local Development/Full and Shipping/Full native execution is recorded in
[Windows-V1-DX12-Local-2026-10-03](../../Apps/Showcase/evidence/Windows-V1-DX12-Local-2026-10-03/acceptance.md).
The GTX 960 developer-machine run covers eight rooms, primitive/texture/instance rendering,
native offscreen graph/UI/present, input/edit/reload/animation captures, Lab asset/plugin rejection
and the complete 210-second guided tour. The explicit Development preset and stock PowerShell 5
default-path fix make the documented local workflow reproducible. Clean-host and physical-display
operator attestations remain pending; this does not complete V1's final acceptance.
✅ The Windows local verifier now requires fresh exports, successful edited-scene snapshot round trip,
reset tour progress and the exact sampled M5/M6 rejection inputs/outputs. Source/package hashes use
pinned LF bytes. Baseline e4a140139189 passes Linux Development 77/77 and Shipping package CI;
physical-display and clean-host final gates remain pending.

✅ SceneDrawData now supports bounded index/instance mesh batches inside one Vulkan/DX12 depth
pass. Empty batches preserve the existing draw. Portable overflow/bounds checks and Vulkan distinct-
geometry pixels cover range offsets and invalid-then-valid recovery; DX12 execution remains a target gate.

Metal Showcase scene/instance/material/depth/copy and Cocoa control/Retina source were extended on 2026-10-04. ✅ macOS hosted Shipping/Full isolated packages now execute eight rooms through Metal scene/copy/UI/present and resize (96 frames; [record](../../Apps/Showcase/evidence/V1-Metal-Hosted-CI-2026-10-04/acceptance.md)). ✅ Native pixel/input/depth/lifecycle CTest passes macOS Development (73/73) and mimalloc (63/63); physical Mac visuals remain pending. Linux distribution regression remains accepted at 80/80 with no skipped native gates. See [distribution evidence](../../Apps/Showcase/evidence/V1-Distribution-Linux-2026-10-04/acceptance.md).

✅ Optional row-major affine SceneInstance matrices and common private model/inverse-transpose
packing are implemented for Vulkan/DX12/Metal ([ADR-0003](ADR-0003-Presentation-Affine-Instances.md)).
Portable tests check exact points, normals, override semantics and malformed input; Vulkan pixels
compare mirrored/sheared instances against independently baked geometry/normals. Legacy TRS,
empty identity, batch budgets and fence ownership remain compatible. Editor consumption and
physical-display/GPU acceptance remain separate.
