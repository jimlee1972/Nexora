# Visual showcase release performance

Production source: `3047d5c68de939c6ead3bfceb8a47c5d6ccf3524`; build `3047d5c68de9`, Shipping/Full Monolithic. All nine measurements and the 100.7-second video use the same packaged executable identified in release-provenance.json.

Scope: native Vulkan / Xvfb / Mesa lavapipe software rasterizer. These are process/frame observations, not GTX 960 acceptance. No GPU timestamps exist. Each tier uses three sequential independent 360-frame runs, with 60 warm-up and 300 measured frames, VSync off / immediate presentation, 1280×720, shot 0, fixed effect time 0, active device and deterministic positions. Own builds/tests and recording were idle during measurement. The interrupted earlier packaging-validation attempt is excluded; only the nine complete share runs are retained.

| Quality | Average FPS range | P95 frame ms range | P99 frame ms range | Average process CPU ms range | Max resident MiB |
| --- | --- | --- | --- | --- | --- |
| Basic | 27.45–28.20 | 39.00–41.96 | 41.52–53.81 | 113.53–116.55 | 136.7 |
| Standard | 17.56–17.86 | 62.56–65.85 | 66.92–71.59 | 217.50–220.74 | 155.9 |
| High | 12.99–13.11 | 84.69–88.53 | 91.73–98.99 | 300.66–302.15 | 228.5 |

CPU time is process-wide user+kernel across all threads and may exceed wall frame time. Peak resident memory is process memory, not GPU VRAM. Raw baseline/run JSON retains CPU/host, sampling scope, effective settings and native counters.

| Quality | Shadow map | Foliage quads | Active motes | IBL | Bloom intensity / radius px |
| --- | --- | --- | --- | --- | --- |
| Basic | 512 | 32 | 24 | Off | Off |
| Standard | 1024 | 64 | 48 | On | 0.15 / 12 |
| High | 2048 | 128 | 96 | On | 0.18 / 20 |

The sky changes from 24 lit material batches to one emission-only batch; sky and motes are excluded from the shadow prepass. Alpha edges, wind and HDR/color transfer remain shared across native adapters. Native GPU fixtures verify visible emission and a lit receiver for non-casters. Shadow instance counters now describe actual per-batch work; their old aggregate-instance semantics are not comparable. Older snapshots used different motion/activation and geometry, so no FPS improvement ratio is claimed against those baselines.

The candidate GTX 960 1280×720 / 60 FPS budget still needs confirmation and physical measurements. VIS-M6 remains unaccepted until target DX12/Vulkan visual review and the confirmed hardware budget pass.
