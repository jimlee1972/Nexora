# VIS-M6 native device provenance — 2026-10-07

✅ Selected-device observations and benchmark consistency checks are delivered. VIS remains **5/7**: final reference-art approval and physical DX12/Vulkan GPU performance/visual acceptance remain open.

Source freeze: `46eac3a50f8379fba868af895e63bc0b72d64a4b`. [Build 1778](https://github.com/jimlee1972/Nexora/actions/runs/37553327195) completes **18/18 jobs**, including the required CI result gate. This is hosted-runner evidence, not physical-display or target-GPU approval.

## Observed native quality matrix

The Linux Shipping/Monolithic executable runs Basic, Standard and High in separate native Vulkan processes, one 160-frame repeat per quality. Each discards 60 warm-up frames and retains 100 measured samples: **480 native frames / 300 measured samples** in total. The frozen activated wide shot uses 1280×720, animation paused at zero and VSync off; fallback and incomplete/paced samples are rejected.

The successful collector prints these actual observations:

```json
{"status":"MEASURED","runs":3,"device_identity":{"name":"llvmpipe (LLVM 20.1.2, 256 bits)","vendor_id":65541,"device_id":0,"driver_version":"104865800","driver_version_format":"vulkan.raw"},"driver_identity":"vulkan.raw:104865800","hardware_budget_accepted":false}
```

The original console observation is in [Linux Shipping job 112573854002](https://github.com/jimlee1972/Nexora/actions/runs/37553327195/job/112573854002). Raw per-process JSON, stdout/stderr, executable/report SHA-256, `benchmark.json` and `benchmark.md` are retained under `artifacts/quality-benchmark` in the Linux Shipping artifact. `physical_display_verified=false`; software-driver timing does not accept the hardware budget. Direct invocation and release tags retain the existing three 360-frame repeats per tier; this branch gate is the bounded smoke matrix.

## Validation

| Gate | Result |
| --- | --- |
| Linux Development configure/build/full CTest | 145/145; 346.56 seconds |
| Windows Development configure/build/full CTest | 128/128; 132.88 seconds |
| macOS Development configure/build/full CTest | 127/127; 91.50 seconds |
| Evidence policy, ordinary and optimized Python | 7/7 in each mode, locally and on all desktop CI hosts |
| Linux native Vulkan scene and report acceptance | Pass; selected device/IDs/raw-driver format required, copied name retained through drain |
| Windows native DX12 contract | Pass; selected name/IDs required after resize and name retained through drain |
| Metal native scene fixture | Pass (5.28 seconds); selected name required, unavailable IDs/driver remain explicit |
| Windows DX12 and Vulkan Shipping isolated-copy visual gates | Pass; Vulkan hosted software-driver screenshots and full guided tour retained |
| Linux Shipping native package and three-quality matrix | Pass; actual selected-device/driver observation above |
| ASan/UBSan | 73/73; 58.84 seconds |
| TSan | 70/70; 37.14 seconds |
| Editor Xvfb, core/sync fixtures, feature-off/minimal builds, mimalloc and mobile configure/Zig object gates | Pass |

Exact Linux desktop gate commands used by Build 1778:

```sh
cmake --preset linux-development -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON -DNEXORA_ENABLE_SLANG=ON -DNEXORA_ENABLE_VULKAN_BACKEND=ON -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON
cmake --build --preset linux-development
ctest --preset linux-development -V --output-log build/linux-development/development-tests.log
```

Exact Linux quality capture:

```sh
python3 Tools/Package/BenchmarkShowcase.py \
  build/linux-showcase-shipping/package/NexoraShowcase-Shipping/bin/NexoraShowcase \
  --backend vulkan --virtual-display --frames 160 --repeats 1 \
  --output build/linux-showcase-shipping/artifacts/quality-benchmark
```

Policy cases cover malformed identities, missing historical observations, changed observed devices/drivers, decimal-format/range errors, full 64-bit DXGI precision, unavailable driver queries and literal Markdown rendering. Existing fallback, scene drift, missing samples, stale report, pacing and mixed-build checks remain enforced.

## Immutable artifacts

GitHub reports these archive digests for the source freeze above; the links retain packages and native evidence:

- [showcase-windows-shipping-evidence](https://github.com/jimlee1972/Nexora/actions/runs/37553327195/artifacts/11454097519), 105066964 bytes, `sha256:3336c46881bb902c573ea3e3a79e9c627900be0188037dc3f95c10538fcc2475`.
- [showcase-windows-vulkan-package](https://github.com/jimlee1972/Nexora/actions/runs/37553327195/artifacts/11453933314), 104039021 bytes, `sha256:576e49c222736a3a63106af3d3ed7babcef190fabce58d181cd9eb8ff6015527`.
- [showcase-linux-showcase-shipping-evidence](https://github.com/jimlee1972/Nexora/actions/runs/37553327195/artifacts/11453883850), 116798355 bytes, `sha256:5296f5fbefd81434aa39c0a37ffc5529176924e8f04b75a0a7f4b66650819b44`.

## Contract and acceptance limits

The bounded, copied `SurfaceDeviceInfo` is backend-neutral diagnostics data, with no borrowed/native handles or new module dependencies. Vulkan observes `VkPhysicalDeviceProperties` and labels the vendor-specific raw 32-bit driver value. DX12 observes DXGI IDs and a 64-bit UMD version only after a successful query. Metal observes its `MTLDevice` name, with IDs/driver unavailable. Empty names and unavailable fields serialize as JSON null; headless reports do not acquire a fabricated device identity.

The C++ diagnostics type requires consumers to rebuild; stable C/Gameplay ABIs remain unchanged. Driver values serialize as decimal strings, preserving DXGI precision. Benchmark consistency compares the actual observed fields, without inventing data for older reports. GPU timestamps and display refresh are still unavailable. The Metal fixture establishes this bounded native contract, not final showcase parity or physical acceptance. No art/quality/render-setting changes or milestone promotion are included.
