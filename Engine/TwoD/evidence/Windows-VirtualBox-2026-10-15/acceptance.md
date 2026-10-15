# Windows VirtualBox clean-host partial acceptance — 2026-10-15

**Result: PARTIAL; 2D-M7 NOT ACCEPTED.** Package prerequisites and CPU checks ran;
native DX12 presentation failed and Vulkan was unavailable. Milestone progress remains
**9/11 (82%), P0 6/8**. Beads parent `nexora-j4q` stays open; execution is `nexora-j4q.4`.
This is the user's requested stopping point; no further implementation or VM testing is included.

## Target and frozen package

The existing independently provisioned Windows 10 guest `Nexora-ZS-M5-CleanMachine-Win10`
ran on VirtualBox **7.2.20r175154**, with Windows **10.0.19045 x64**, two vCPUs and
6144 MiB RAM. Its VirtualBox WDDM driver was **7.2.20.25154**. Guest Additions were installed;
no `cmake`, `cl`, `zig`, `git` or `ninja` was found on the guest PATH. This PATH check is not
an exhaustive installed-software audit. No development toolchain was installed for this run.
The host GTX 960 is not the guest's physical GPU and does not certify physical-display acceptance.

The previously staged Monolithic Shipping `NexoraTwoDBenchmark.exe` and cooked Atlas/Clip
were copied into a separate guest directory. Runtime provenance is source
`ed698e7ed36db3a3763cc4fcd019924ff6a37e07`; executable SHA-256 is
`7bde35870dda93c7ddfacefb38ecc3827aa60c4b98433e83654bf5ec4da941ba`.
The guest verified all three package hashes against its manifest; exact values and raw
results are retained in [raw-results.md](raw-results.md).
This frozen executable predates the latest GPU-instancing addition on main. These observations
must not be transferred to that newer implementation, or to another rebuilt executable.

## Observed results

| Item | Observed result | Accepted scope |
| --- | --- | --- |
| Package hashes | ✅ EXE and both cooked assets match all three SHA-256 values | Transfer/integrity for this frozen package |
| Original prerequisites | Launch exit `-1073741515` / `0xC0000135`; `vcruntime140.dll`, `vcruntime140_1.dll`, `msvcp140.dll` absent | Original package did not run on the baseline guest |
| VC++ Runtime | ✅ Microsoft-signed x64 **14.51.36247.0**, installer exit 0; CPU launch succeeds afterward | Prerequisite installed explicitly; package did not supply it automatically |
| CPU mixed workloads | ✅ Exit 0 in both configurations; six cases each, four warm-up plus eight measured frames per case | CPU runtime, cooked asynchronous load/reload and mixed workload correctness |
| Corrupt cooked Atlas | ✅ Exit 1 with `invalid cooked asset` in both configurations | Expected bad-asset rejection, not an unrelated launch failure |
| Native DX12, 3D off / 64 MiB VRAM | Exit 1, `stress draw` | FAILED; no successful native presentation acceptance |
| Native DX12, 3D on / 128 MiB VRAM | Exit 1, `stress acquire` | FAILED; no successful native presentation acceptance |
| Vulkan | `vulkan-1.dll` absent; no Vulkan ICD registry entry observed; loader call throws `DllNotFoundException` | UNAVAILABLE; no Vulkan instance/device or presentation test passed |

The six CPU cases are 100, 1,000, 5,000 and 10,000 normal objects, 1,000 overlapping objects,
and 10,000 sparse objects. Their short VM runs are functional checks; CSV timings do not accept
minimum-PC performance, sustained 60 FPS, physical-GPU latency or the integrated product budget.
Native CSV files contain a header only and must not be counted as completed native workloads.

## DX12 and Vulkan limits

With 3D disabled, DXGI enumerated Microsoft Basic Render Driver, and the default-adapter
`D3D12CreateDevice` call at feature level 11_0 succeeded. That API success was a software path;
it did not establish VirtualBox GPU rendering or successful application draw/present.

With 3D enabled, explicit DXGI adapter probes returned:

```text
adapter=0 name=VirtualBox Graphics Adapter (WDDM) vendor=80EE flags=0
adapter=0 D3D12CreateDevice FL11_0 HRESULT=0x887A0004
adapter=1 name=Microsoft Basic Render Driver vendor=1414 flags=2
adapter=1 D3D12CreateDevice FL11_0 HRESULT=0x00000000
```

`0x887A0004` is DXGI_ERROR_UNSUPPORTED. The actual VirtualBox adapter did not create the
required DX12 device; Microsoft software device creation succeeded. Host-side 3D initialization
against GTX 960 and dxdiag feature-level listings do not override these per-adapter results.
The two application failures have different observed stages (`draw` versus `acquire`);
this acceptance run did not establish their internal root cause or implement a renderer fix.

Vulkan was unavailable in the inspected guest. No Vulkan SDK, software ICD or replacement graphics
driver was installed. The tested 2D package was also built with Vulkan disabled, so installing a
loader alone would not establish native Vulkan support for this package. No Vulkan PASS is claimed.

## Commands and prerequisite provenance

Run from the copied package directory; use separate report paths for each configuration:

```powershell
./NexoraTwoDBenchmark.exe --atlas assets/atlas.nxcook --clip assets/clip.nxcook --frames 8 --report C:/NexoraAcceptance20261015/runtime-installed-cpu.csv
./NexoraTwoDBenchmark.exe --atlas C:/NexoraAcceptance20261015/corrupt.nxcook --frames 4 --report C:/NexoraAcceptance20261015/runtime-installed-corrupt.csv
./NexoraTwoDBenchmark.exe --native --atlas assets/atlas.nxcook --clip assets/clip.nxcook --frames 8 --report C:/NexoraAcceptance20261015/runtime-installed-native.csv
```

The corrupt fixture flips the final byte of a copy of the admitted Atlas. The second pass uses
`three-d-on-*` report names. The harness retains stdout/stderr, checks a bounded process wait,
and records actual exit codes; the raw result records retain exact arguments and elapsed times.
SHA-256 checks cover the executable and both original cooked files before each phase.

The [official Microsoft x64 VC++ Redistributable](https://aka.ms/vc14/vc_redist.x64.exe)
was downloaded on the host and its Authenticode signature verified as valid, signed by
Microsoft Corporation, before transfer and installation in the guest with
`/install /quiet /norestart`. Installer SHA-256:
`843068991daaa1f73ad9f6239bce4d0f6a07a51f18c37ea2a867e9beca71295c`.
It returned exit 0; observed `vcruntime140.dll` file version was **14.51.36247.0**.
No installer binary, development library or credentials are committed here.

## Unaccepted scope and handoff

M7 remains **NOT ACCEPTED**, with a partial VM prerequisite/CPU slice executed. It is no longer
accurate to label all clean-Windows work NOT RUN. Successful native draw/present, actual
keyboard/controller use, UI/text visual acceptance, audible playback, VM Studio save/reopen,
and display/DPI transitions remain unverified. The benchmark's clean process exit does not
accept the full product lifecycle/shutdown gate. M6 physical minimum-PC acceptance also remains open.
The older Showcase VM acceptance is a different package and does not accept this 2D M7 gate.

After exporting the primary evidence, Windows shut down normally. The temporary 3D-on/128 MiB
configuration was restored to **3D off / 64 MiB**, and the VM was left **powered off**.
The VC++ Runtime installation and guest test files were retained; no snapshot was restored.
A future run must account for that installed prerequisite or deliberately select a documented baseline.

Original OS/GPU inventory, dxdiag, package reports, installation logs and probe outputs remain
in the local session artifact directory. This record versions curated raw text in
[raw-results.md](raw-results.md), omitting machine-specific paths, installers and full logs.
The final optional second-pass inventory/dxdiag/default-probe copies were interrupted by normal
shutdown and remain inside the guest; they are not counted as exported evidence. The explicit
second-pass adapter probe and both package test result sets were exported successfully.

This update changes documentation only. Markdown/link/bilingual validation applies; no new CMake
configure, build or CTest run is claimed, and the recorded guest tests are not Linux Cloud evidence.

## 繁體中文交接摘要

**本次為部分驗收，2D-M7 尚未通過，進度保持 9/11（82%）、P0 6/8。**
✅ 三個套件 SHA-256 一致；✅ 安裝官方 VC++ Runtime 後，兩輪各六組 CPU 負載、
Cooked 非同步載入／Reload 通過；✅ 損毀 Atlas 以 `invalid cooked asset` 正確拒絕。
原始乾淨 guest 因缺少 VC++ Runtime，以 `0xC0000135` 啟動失敗，不能宣稱原封裝直接通過。

DX12：3D 關閉時僅 Microsoft 軟體 renderer 可建立裝置，程式在 `stress draw` 失敗；
3D 開啟後 VirtualBox adapter 的 `D3D12CreateDevice` 回傳 `0x887A0004`，
程式在 `stress acquire` 失敗。Vulkan loader／ICD 未找到，封裝本身亦未啟用 Vulkan，
未執行 Vulkan 原生驗收。這些觀察不代表實體 GPU／顯示器或效能 gate 通過。

受測執行檔為上述既有凍結版本，不能推定最新 main 的 GPU Instancing 已驗證。
實際輸入／手把、音效、UI／文字畫面、VM Studio 存檔／重開、顯示／DPI 及成功原生呈現
仍待驗收。VM 已正常關機並恢復 3D 關閉／64 MiB；保留已安裝的 Runtime 與 guest 證據。
依使用者要求在此告一段落，不繼續新增實作或執行 VM 測試。
