# Showcase Windows DX12 windowed compositing -- isolated-copy evidence

Result: PASS

**Tier: developer-machine isolated package copy. This is NOT an independently provisioned
clean-machine/VM acceptance.** See `ZS-M5-Windows-CleanMachine-2026-10-01` for that separate,
heavier tier (which only covers the headless path; `windowed_native_backend` is `NOT EXECUTED`
there).

## What this record covers

`Dx12Surface` previously inherited the base `ISurface::CompositeRgba8` default (`Unsupported`), so
`NexoraShowcase.exe --mode=interactive --backend=dx12` opened a window and cleared it but failed
outright once it tried to composite the clear-color/triangle/diagnostics frame onto the swapchain
backbuffer. Commit `c3b4070` implements `Dx12Surface::CompositeRgba8` (upload-heap ->
`CopyTextureRegion`, mirroring the existing `VulkanSurface` pattern).

This record verifies that fix one tier above "ran directly out of the build directory": the
`NexoraShowcasePackageDevelopment` package was copied to a fresh temporary directory outside the
build tree, every `SHA256SUMS` entry was re-verified against that staged copy, and the showcase was
launched from that verified copy in windowed interactive mode.

## Package

- Source commit: `c3b4070dea2ba44cd3ddf0b2ab8202022099dbd5`
- Profile: Development / dynamic gameplay
- Source package: `build/windows-dx12-zig-showcase/package/NexoraShowcase-Development`
- Checksums verified in the isolated copy: 16/16
- Host: `JIM-PC`, Windows 10 Home 10.0.19045 (same physical machine as the build -- not a separate
  VM)

## Launch

This is a manual windowed verification run, not the package's recorded headless smoke command:

    bin/NexoraShowcase.exe --mode=interactive --scene=hub --backend=dx12 --frames=300 \
      --gameplay-module=dynamic --gameplay-library=bin/NexoraZigGameplay.dll \
      --report=launch-report-windowed.json

- Exit status: 0
- `launch-report-windowed.json` status: PASS
- `backend`: dx12, `backend_fallback`: false
- `surface_acquires` / `surface_presents` / `composed_frames`: 300 / 300 / 300
- `clear_color` / `triangle` / `diagnostics_overlay`: true / true / true

## Screenshot

`showcase-window-isolated-copy.png` was captured with `user32.dll PrintWindow` targeted at the
`NexoraShowcase` window handle only -- not a full-desktop screen capture -- so it shows exactly the
rendered clear color, triangle, and M0-M12 diagnostics panel and nothing else on the machine.

## What this record does not establish

- Not a clean-machine/VM acceptance (no fresh OS install, no independent provisioning).
- Not a CI gate; this was a manual, one-off local run.
- Does not cover Linux/Vulkan or macOS/Metal windowed compositing, which remain open per the V1
  Visual Showcase roadmap.
