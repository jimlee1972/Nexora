# VIS-M0 adopted assets and initial performance — 2026-10-05

✅ VIS-M0 accepted within its baseline scope: confirmed art direction, explicit source/license/
replacement inventory, native courtyard composition and three fixed shots, adopted representative
meshes/atlas through Import → Cook → Bundle → Runtime, retained screenshots and initial performance.
Final materials, lighting, hero-image art and environmental motion remain VIS-M1–M4 work.

✅ Linux Development full gate: **87/87 passed, no skips**. Converter tests reject invalid GLB
headers, transformed/external-buffer content, out-of-budget accessors, bad layouts, non-triangle
geometry and corrupt/truncated atlas data. Pinned source conversion matches the generated header.
✅ Native Vulkan/lavapipe screenshot mode, three fixed shots and exact wide-shot replay pass.
✅ Development package/checksums/isolated startup pass; source assets, license and inventory ship.

Baseline: three sequential 360-frame fixed-wide-camera runs, 60 warm-up frames and 300 samples per
run, 1280x720, requested and negotiated immediate presentation, no UI, no FPS cap. Average FPS:
78.033, 77.157, 75.328. P95 frame ms: 14.478, 14.246, 15.052; P99: 17.813, 15.506, 17.112.
CPU fields are process-wide user+kernel deltas, including all threads and software GPU work;
peak resident bytes are OS observations. GPU timings and display refresh metadata are unavailable.
These are Mesa lavapipe software results; **the GTX 960 budget is not accepted**. Benchmarking was
repeated after other test/build work stopped; exploratory runs concurrent with CTest are excluded.

Commands used with temporary tool/sysroot paths documented in the earlier greybox record:

```bash
cmake --preset linux-development
cmake --build --preset linux-development -j 4
ctest --preset linux-development
python3 Tests/Showcase/LinuxCourtyardBenchmark.py build/linux-development/Apps/Showcase/NexoraShowcase --output /tmp/nexora-courtyard-adopted-benchmark
cmake --build --preset linux-development --target NexoraShowcasePackageDevelopmentEvidence
```

Source provenance: base `e6593703a0ef163e958d449123e16050be86bc18` plus the uncommitted implementation
identified by `source-hashes.json`; embedded BuildInfo reports that base, not the future merge.
Adopted original hashes/license/revision are in `Content/Showcase/Courtyard/assets.json`. The
1024x1024 palette source is retained unchanged; the GPU baseline uses deterministic 64x64 area
filtering. It is not a PBR detail map or general glTF importer. Windows/macOS/mobile execution
was not performed locally; CI checks remain separately observable.

繁中：VIS-M0 的方向、素材盤點、灰盒配置、三個鏡頭、代表性素材流程、原生截圖與初始
效能基線已完成。Linux Development 87/87 無 skip，Development 套件校驗及隔離啟動通過。
正式材質、光影、主視覺與環境動態仍屬 VIS-M1～M4；軟體渲染不代表 GTX 960 達標。
