# Windows/MSVC Player acceptance — 2026-10-17

**Result: verified developer-host slice; VM observations are partial. M6/M7 remain NOT ACCEPTED.**
This run validates the general Player delivered by PR #620 on source
`63bb5f7a` (main after PR #621). No engine/runtime source is changed by this acceptance PR.
The documentation repair distinguishes Visual Studio's `bin/Shipping/` layout from Ninja's `bin/`.
The repository-root README milestone summary is unchanged: 9/11 (82%), P0 6/8.

## Host and build identity

Physical developer host: Windows 10 build 19045, AMD FX-6100, NVIDIA GTX 960 (driver 32.0.15.8180). Visual Studio
Community 2022 17.14.35 supplies MSVC **19.44.35228 x64**, Windows SDK 10.0.26100.0 and CMake.
The build uses **Monolithic Shipping**, native DX12 enabled, Vulkan and graphical Editor disabled,
and repository-pinned Zig 0.14.0. The four requested targets built successfully with exit 0.
The independent C ABI fixture was also compiled with MSVC, without linking an engine DLL.
Changed-Markdown link/bilingual validation, `git diff --check` and all 16
`python Tools/Build/TestDocumentationCI.py` tests passed.

Both exports are genuine Shipping copies (`development_test_copy=false`), with the selected
x64 Microsoft.VC143.CRT **14.44.35112** copied by the exporter. Retained
[UI inventory](ui-inventory.json), [Zig inventory](zig-inventory.json) and [scope](scope.json)
identify the exact binaries/assets. No executables, font raster, CRT DLLs or build output are committed.
Local outputs remain in `build/windows-pr620/UI-Shipping/` and
`build/windows-pr620/zig-project/ZigGame-Shipping-Fixture/`.

## Results

| Check | Observed result | Acceptance boundary |
| --- | --- | --- |
| Native MSVC Shipping build | ✅ Player, Asset Compiler, Audio Probe and real Zig shared module | Four targets; not a full Development build/CTest run |
| Independent project contracts | ✅ UI-only module, Unicode source/package/save paths, process restart, immutable package, invalid descriptor/path and corrupt-save rejection | `ProjectAcceptance.py`, exit 0 |
| Separate Zig project export | ✅ Shipping closure, 60 headless frames, 34 visible sprites, balanced allocations and corrupt Cooked rejection | [Original host JSON](host-zig-project.json); fixture content, not a game |
| Physical-host DX12 UI | ✅ Three native frames, six UI draws, balanced allocations and Stop callback | [Original host JSON](host-native.json); panel submission, not human UI/font review |
| `Run.cmd` | ✅ Launched from an unrelated temporary working directory; three native frames, exit 0 | CWD handling and argument forwarding; no double-click/human close claim |
| Windows GDI font contracts | ✅ Raster alpha/metrics, immutable publication, missing-glyph and non-BMP rejection | `FontBakeAcceptance.py`, exit 0 |
| Traditional Chinese corpus | ✅ Microsoft JhengHei bakes the bounded corpus; exported atlas admitted by native Player | Does not prove rendered Chinese glyph appearance, font redistribution rights or DPI |
| Host waveOut probe | ✅ 48,001 frames submitted, close/reopen and closed-output rejection, exit 0 | Human audibility and hot unplug/default-device switching **NOT VERIFIED** |
| Physical XInput | Four slots disconnected | Actual controller test **NOT RUN** |
| VM package checks | Six individual process cases reached their expected results, including UI restart and 120 native Zig frames; both final inventories matched | Guest Control/VM aborted later; **not a complete successful VM harness/lifecycle gate** |
| VM no-audio case | ✅ `--expect-unavailable` reports UNAVAILABLE and exits 0 | Audio disabled in existing VM; not audible playback |

[Raw observations](raw-results.md) retain host output, guest stdout/stderr, environment checks and
VirtualBox errors. Generic reports retain `physical_windows: NOT RUN` and
`audible_acceptance: NOT VERIFIED`; host metadata supplements their scope and does not rewrite them.
The host CJK corpus was `遊戲開始 儲存 讀取 音效 設定 取消 確認 中文標點，。！？ 0123456789`.
The UI-only fixture draws a panel, so admitting its font asset does not prove glyph raster display.

## Windows guest and harness limitations

Existing `Nexora-ZS-M5-CleanMachine-Win10`: Windows 10 19045, VirtualBox 7.2.20, 2 vCPU,
6144 MiB, VBoxSVGA, 3D off, audio off. The prior VC++ Runtime remains installed. No snapshot was
restored and no graphics/audio configuration was changed. With PATH restricted to Windows system
locations, `cl`, `cmake`, `zig`, `python`, `git` and `ninja` were not found; Vulkan loader was absent.
This PATH probe is not an exhaustive installed-software audit. VM DX12 observations use the
3D-disabled/software-rendering environment and do not accept hardware DX12 or minimum-PC performance.

Only the exported Player/module/cooked assets/font/CRT and launcher were transferred. They ran from
`C:/NexoraAcceptance620/Unicode 封裝/`. The independent module first logged `visits loaded=0`, then
`visits loaded=1` in a new process. Corrupt-save rejection left the test slot unchanged; a separate
Chinese save directory succeeded. Zig submitted 120 native frames, 120 instanced draws and 4,080
instances, with 34 visible sprites. Corrupt Cooked input was rejected with
`project cooked hash/type/identity`. These are program observations, not physical keyboard input.

The first PowerShell harness could not reliably retrieve `Start-Process` ExitCode. A revised
.NET Process runner reached all six expected process results and final immutable-package checks.
Guest Control then had CLIXML/timeout/RPC failures and the VM became `aborted` before a reliable
whole-harness final JSON/exit could be retained. A separate read-only probe had observed matching
package hashes and zero Player processes; that earlier observation does not prove clean VM shutdown.
The root cause of the VirtualBox failure is unresolved. The same VM was restarted without a snapshot restore; Guest Additions initially remained unready,
and a later read-only status probe reported zero Player processes. A separate recovery inventory
parse failed (`ConvertFrom-Json`), so post-restart package integrity was not re-established.
ACPI did not establish poweroff and the recovered framebuffer was abnormal. A reversible
VirtualBox Save State completed successfully; the VM is left **saved**, not cleanly shut down.
Do not promote these observations to a completed clean-host or long-running lifecycle gate.
All retries use fresh GUID save directories; no existing default save was deleted. The original
first-run default save is retained. No controller, audio or DPI workaround changes VM configuration.

## Reproduction commands

In a Visual Studio x64 developer shell at the repository root:

```powershell
cmake --preset windows-showcase-shipping -G "Visual Studio 17 2022" -A x64 -DCMAKE_CONFIGURATION_TYPES=Shipping -DNEXORA_BUILD_SHOWCASE=OFF -DNEXORA_ENABLE_2D=ON -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=OFF -DNEXORA_ENABLE_VULKAN_BACKEND=OFF -DNEXORA_ZIG_EXECUTABLE=G:/proj/Nexora/.tools/zig-win-x86_64-0.14.0/zig.exe
cmake --build --preset windows-showcase-shipping --target NexoraTwoDPlayer NexoraTwoDAssetCompiler NexoraTwoDAudioProbe NexoraZigTwoDSharedBuild --parallel 2
cl /nologo /LD /EHsc /std:c++20 /IEngine/API/include Tests/TwoD/ProjectModule.cpp /Febuild/windows-pr620/NexoraTwoDProjectFixture.dll /Fobuild/windows-pr620/ProjectModule.obj /link /IMPLIB:build/windows-pr620/ProjectModule.lib
python Tests/TwoD/FontBakeAcceptance.py
python Tests/TwoD/ProjectAcceptance.py --app build/windows-showcase-shipping/bin/Shipping/NexoraTwoDPlayer.exe --module build/windows-pr620/NexoraTwoDProjectFixture.dll --redist <matching-x64-CRT-directory>
python Tests/TwoD/ZigProjectAcceptance.py --app build/windows-showcase-shipping/bin/Shipping/NexoraTwoDPlayer.exe --module build/windows-showcase-shipping/Gameplay/Zig/NexoraZigTwoD.dll --compiler build/windows-showcase-shipping/bin/Shipping/NexoraTwoDAssetCompiler.exe --output <fresh-output> --shipping --vc-redist <matching-x64-CRT-directory>
NexoraTwoDAudioProbe.exe --frames 48000
```

The last command uses the newly built `bin/Shipping/` executable. Export the independent fixture
with a new descriptor (`scene=Main`, `audio=off`) and the baked font through `ExportGame.py`, using
that same CRT directory. Native runs add `--native --frames 3`; Zig uses `--native --frames 120`.
Keep a fresh dedicated save directory for destructive corruption fixtures. Retain errors and actual
exit codes. Do not infer success from a screenshot, a log prefix or successful compilation alone.

The unresolved VM follow-up is **nexora-j4q.7.6**; this host acceptance slice is **nexora-j4q.7.5**.

## Remaining gates

No full Windows Development CTest or Linux configure/build/CTest gate ran in this Windows session.
This PR changes documentation/evidence only; prior CI results are not relabeled as a fresh local run.
A future implementation change still requires the repository's full Linux gate.

Physical controller, human keyboard/rebinding/focus behavior in this new project, human audible
playback, device removal/default-switch recovery, formal Chinese UI/font appearance and
100%/150%/200%/multi-monitor DPI, integrated game save schema/migration and failure UX, reliable VM
lifecycle, clean physical hardware PC startup, and sustained minimum-PC performance remain open.
No complete M6/M7 checkbox or milestone count is advanced.

## 繁體中文摘要

✅ 本機 MSVC Shipping 四個目標、獨立專案／Zig 匯出、Unicode 路徑、存檔重啟／損壞拒絕、
DX12 面板提交、Run.cmd 與 Windows 字型烘焙均已驗證。waveOut 成功提交並關閉／重開裝置，
但未有人確認聽音。VM 個別案例與雜湊留有正向證據，後續 Guest Control／VM 異常中止，
完整 VM 生命週期仍未通過。實體手把、人工輸入／聲音、中文外觀／DPI、乾淨實體主機與最低
硬體效能皆未驗收；M6／M7、9/11（82%）、P0 6/8 不變。
