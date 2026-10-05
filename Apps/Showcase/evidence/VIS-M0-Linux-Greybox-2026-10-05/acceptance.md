# VIS-M0 engineering greybox baseline — 2026-10-05

✅ Linux Development configure/build and CTest: **84/84 passed, no skips**.
✅ Native Vulkan/lavapipe under Xvfb: three distinct fixed shot captures, byte-identical wide-shot
replay, diagnostic-free UI hiding/restoration, nine scene visits, native graph/resize and shutdown.
✅ Representative crystal placeholder loaded from the active cooked/bundled mesh generation.

This is a first VIS-M0 implementation slice. All seven VIS milestones remain unaccepted; final
free-model/texture adoption, art review, initial hardware performance measurements and Windows
DX12/Vulkan visual acceptance remain open. Close-up and motion labels describe camera framing;
PBR, emission, shadows and wind are not delivered here. Software Vulkan is not GTX 960 evidence.

Build provenance: Development/Modular, base commit `a6f104ed4ba28423e6c56a25010676022a502e60`,
plus the uncommitted implementation identified exactly by `source-hashes.json`. The embedded
BuildInfo ID is the base commit, not a claim that that commit contains this change. Captures are
1280×720 Xvfb images from the native executable; CPU/GPU performance timings are not claimed.
The retained report is from the interaction run after visiting all rooms; shot=1 is the final
camera-selection state, while the PNG filenames identify the captured fixed shots.

Commands (temporary tools on PATH; PYTHONPATH=/tmp/nexora-build-tools,
LD_LIBRARY_PATH=/tmp/nexora-sysroot/usr/lib/x86_64-linux-gnu,
VK_ICD_FILENAMES=/tmp/nexora-sysroot/usr/share/vulkan/icd.d/lvp_icd.json,
ZIG_GLOBAL_CACHE_DIR=/tmp/nexora-zig-cache):

```bash
# Initial local setup: X11 headers/libraries and tools extracted under /tmp, no repository changes.
cmake --preset linux-development -DCMAKE_PREFIX_PATH=/tmp/nexora-sysroot/usr -DCMAKE_CXX_FLAGS=-I/tmp/nexora-sysroot/usr/include -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON -DNEXORA_BUILD_SHOWCASE=ON
cmake --preset linux-development
cmake --build --preset linux-development -j 4
NEXORA_SHOWCASE_EVIDENCE_DIR=/tmp/nexora-courtyard-evidence ctest --preset linux-development
```

The first setup attempt lacked X11 headers; the first build lacked a writable Zig global cache.
The first CTest run lacked clang++ and skipped native display gates due to sandbox socket access.
After installing temporary tools and enabling local display access, the full gate above passed.

No linkage/module/public ABI boundary changed, so Shipping was not required. Windows, macOS,
Android and iOS were not run in this Linux environment.

繁中：本次交付為庭院工程灰盒、三個固定鏡頭與無診斷 UI 的原生基線畫面。Linux
Development 84/84 無 skip；代表性占位網格沿用匯入／cook／bundle／runtime 流程。
免費最終素材、美術品質、硬體效能及目標平台驗收仍待完成；不宣稱 VIS-M0 完整驗收。
