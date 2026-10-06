# VIS-M6 quality benchmark: native CI acceptance

✅ The measurement tool and nine-run delivery are verified. VIS remains 5/7: final reference-art review and physical DX12/Vulkan performance acceptance remain open.

Production/tool evidence freeze: `2bfdee501726137f3f799c8cb915d79ab176a918` (PR #388). [Build 1755](https://github.com/jimlee1972/Nexora/actions/runs/37500666964) passes all 17 jobs. Hosted Development tests: Linux 143/143 (346.08 s), Windows 126/126 (162.62 s), macOS 125/125 (100.77 s). Windows DX12/Vulkan isolated packages and guided tours, Linux/macOS Shipping packages, Editor native validation, mimalloc and ASan/UBSan/TSan pass. These hosted jobs do not grant physical-display acceptance.

The Shipping/Monolithic Linux executable completes Basic/Standard/High sequentially, three processes per quality, 360 native frames each, 60 warm-up frames discarded and 300 samples retained: 3,240 native frames and 2,700 timing samples. Final tool result is `MEASURED`, nine runs, `hardware_budget_accepted=false`. The application uses the fixed activated wide shot, animation frozen at zero, clean UI, 1280×720 and immediate/VSync-off presentation. All fallback, lifecycle/frame-count, sample-window, metric, build and scene-consistency checks pass.

[Immutable uploaded artifact](https://github.com/jimlee1972/Nexora/actions/runs/37500666964/artifacts/11430643156): `showcase-linux-showcase-shipping-evidence`, ID `11430643156`, 116,902,920 bytes, SHA-256 `fcf6a57187b4526c52c0b1ae09cbfed194728bd5e061853df68aaa630f6f7b8d`. It retains the Shipping package and `quality-benchmark/benchmark.json`, `benchmark.md`, nine raw JSON reports and stdout/stderr logs. The matrix records individual FPS, average frame time, P95/P99, process-wide CPU time, peak resident memory, executable SHA-256 and each report SHA-256. GPU timestamps, GPU driver identity and display refresh are unavailable; no frame timing is relabeled as GPU timing.

Exact Linux validation:

```bash
cmake --preset linux-development -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON -DNEXORA_ENABLE_SLANG=ON -DNEXORA_ENABLE_VULKAN_BACKEND=ON -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON
cmake --build --preset linux-development
ctest --preset linux-development -V --output-log build/linux-development/development-tests.log
cmake --preset linux-showcase-shipping
cmake --build --preset linux-showcase-shipping --target NexoraShowcasePackageShippingEvidence --parallel 4
python3 Tools/Package/VerifyShowcaseRelease.py --package build/linux-showcase-shipping/package/NexoraShowcase-Shipping --evidence-directory build/linux-showcase-shipping/artifacts/release-native
python3 Tools/Package/BenchmarkShowcase.py build/linux-showcase-shipping/package/NexoraShowcase-Shipping/bin/NexoraShowcase --backend vulkan --virtual-display --output build/linux-showcase-shipping/artifacts/quality-benchmark
```

Policy validation: `python3 Tools/Package/TestBenchmarkShowcase.py` and `python3 -O Tools/Package/TestBenchmarkShowcase.py`, five tests each, pass locally and in CTest. All nine retained VIS-M6 baseline reports also satisfy the policy. The attached executor lacks CMake/compiler and its Git proxy is unavailable; full build and native evidence above ran on the named GitHub Actions runners.

CI cost follow-up: the full software-Vulkan matrix ran from 17:08:41 to 17:29:19 UTC (about 20 minutes). Branches now use one 160-frame process per quality (60 warm-up + 100 samples), while release tags and direct tool invocation retain the full nine-run defaults. This only changes CI sampling cost; it does not alter the engine, profiler, checks, tiers, native verifier or immutable full evidence above. Current-head branch-smoke CI is recorded in PR #388 before merge.
