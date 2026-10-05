# ED-M0 Linux baseline and acceptance gaps

Date: 2026-10-05 (Asia/Taipei). Validated runtime source:
`8a14619` (the changes accompanying this record are documentation only).

## Observed results

| Configuration | Result |
| --- | --- |
| Linux Development, graphical shell OFF | Configure/build passed; CTest **71/71 passed**, none skipped |
| Linux Development, graphical shell ON, Slang ON | Configure/build passed; CTest **115/115 passed**, none skipped |
| Linux Shipping, default Minimal/Monolithic engine | Configure/build passed; testing is OFF by preset |
| OFF dependency isolation | No `build/linux-development/_deps/nexora_imgui-src` existed before configuring ON |
| ON dependency revision | Dear ImGui `v1.91.9b-docking`, commit `4806a1924ff6181180bf5e4b8b79ab4394118875` |
| Display test registration | `ctest -N -R '^editor\.linux_display_acceptance$'` registered exactly one test |
| Native display/recovery test | `editor.linux_display_acceptance` passed in 19.92 seconds |

Host: Debian GNU/Linux 13.6, x86_64; GCC 14.2.0, Clang 19.1.7, CMake 4.4.4,
Ninja 1.13.2, Slang 2026.18. Display: X11 through Xvfb 21.1.16. RHI: Vulkan through
Mesa 25.0.7 lavapipe (software rasterizer). No physical display or Windows host was available.
Tool packages were extracted into the workspace because system directories are read-only;
no build output, dependency sources, tool binaries, or local overrides are part of the patch.

The default OFF configure, build and test ran first. After that completed, ON was configured
in the same preset build directory with Slang enabled, built and tested. Shipping used its
separate preset directory. The environment provided the extracted X11 headers/libraries through
`CMAKE_PREFIX_PATH`, tools through `PATH`, and the lavapipe ICD through `VK_ICD_FILENAMES`.
The first CTest attempt exposed missing Clang and an undiscovered Vulkan ICD; installing Clang
and selecting the actual local ICD resolved them before the complete successful reruns.

Commands (after the workspace tool environment was loaded):

```bash
cmake --preset linux-development -DCMAKE_MAKE_PROGRAM=/workspace/Nexora/work/ed-m0/python-tools/bin/ninja -DX11_X11_LIB=/workspace/Nexora/work/ed-m0/sysroot/usr/lib/x86_64-linux-gnu/libX11.so
cmake --build --preset linux-development -j 4
ctest --preset linux-development

cmake --preset linux-shipping
cmake --build --preset linux-shipping -j 4

cmake --preset linux-development -DNEXORA_ENABLE_SLANG=ON -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON
cmake --build --preset linux-development -j 4
ctest --preset linux-development -N -R '^editor\.linux_display_acceptance$'
ctest --preset linux-development
```

The OFF gate took 14.47 seconds and ON took 180.65 seconds. Supporting automated checks include
module graph, Window/Presentation and Renderer contracts, ImGui input/docking/layout/texture/
retirement/DPI/512-frame soak contracts, Unicode Rename, native Vulkan rendering, and process
recovery. The Linux display test fsyncs a seeded journal, SIGKILLs the real Editor, verifies the
committed workspace and journal survive, reacquires the writer lease, and separately exercises
keyboard Recover and Discard. It does not inject a crash into an in-progress write transaction.

## Acceptance decision

✅ **WP0 baseline and reproducibility audit is complete**: both feature configurations and Shipping
have results, the pinned dependency and OFF isolation are checked, and the focused plan maps
later acceptance criteria to automation or target-host observations.

**ED-M0 remains open.** The following required evidence is blocked by unavailable target hosts:

- Physical-display Linux visual/input/docking/focus/resize/minimize/recovery observation.
- Windows 100/125/150/200% text and hit targets, per-monitor moves, and actual IME composition,
  cancellation, exactly-once committed text and candidate placement at multiple DPI values.
- Native GPU validation and supported-backend host parity beyond this software Vulkan run.

The [English focused plan](../../../Roadmap/en/Editor_ImGui_Integration_Plan.md) and
[Traditional Chinese focused plan](../../../Roadmap/zh-TW/Editor_ImGui_Integration_Plan.md)
contain the package-by-package evidence map and operator checklist. Windows-only callback tests
are supporting automation and cannot replace installed-IME interaction. macOS/Metal is unverified.
Current Shipping policy disables the Editor; a Shipping graphical Editor is unsupported and
requires a separate product/build-policy decision. Shipping engine success is not UI evidence.

## 繁體中文

✅ WP0 基準與可重現性盤點完成：同一 runtime source 的 Development graphical OFF **71/71**、
ON 加 Slang **115/115** 全部通過且無 skipped，Shipping engine configure/build 通過。
已核對 OFF 未抓 ImGui、ON 的 pinned tag/revision，以及後續 test／target-host gate 對照。
顯示與 recovery 使用 Xvfb／Mesa lavapipe，沒有實體顯示器，也未執行 Windows/macOS 驗證。

ED-M0 維持 open：實體 Linux 操作、Windows 多 DPI／跨螢幕／真實 IME，以及所需 native GPU／
backend host parity 證據仍待補齊。Shipping 強制關閉 Editor，不能以 engine build 成功代替 UI 驗收。
本變更只保存證據與驗收交接，未改引擎行為，也沒有把後續 graphical milestone 標為完成。
