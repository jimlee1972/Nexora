# Windows DX12 local developer-machine acceptance

✅ Development/Full (Modular, dynamic Zig) and Shipping/Full (Monolithic, static Zig) pass
the packaged stock Windows PowerShell 5 verifier on the local GTX 960 developer machine.
This is visible native DX12 execution, separate from headless and Linux/CI results.

| Profile | Package checksums | Client PNGs | Native graph/copy/present frames | Native instances | Exit |
| --- | ---: | ---: | ---: | ---: | ---: |
| Development/Full | 22 | 24 | 1970 | 2723 | 0 |
| Shipping/Full | 14 | 25 | 15050 | 21263 | 0 |

- ✅ Full Windows Development CTest: 68/68, including V1 contracts, Showcase probes/rooms, ABI and native backend contracts.
- ✅ Hardware rasterizer; no backend fallback or CPU image composition.
- ✅ Offscreen, Main, UI, Present: four completed callbacks and three logical transition requests.
- ✅ Eight rooms, instances, sampled checker, quad, triangle, resize and ordered shutdown.
- ✅ Win32 pointer orbit/wheel and held character input, F1/F2, Modify/Undo/Play/F5/reloaded Play,
  locale, crouch/teleport, animation blend, lifecycle/pressure and pause/replay captures.
- ✅ Live M5 empty-asset and M6 actual dynamic-library ABI rejection export JSON and Markdown.
- ✅ Shipping tour completes all seven steps at 210.008 seconds; final step index 6, paused.
- Shipping screenshots were reviewed as a contact sheet and full-size Hub/Rendering/Lab,
  overlay-hidden, modified-scene and completed-tour captures. Development images are retained.
  Controls/captures supplement CTest and Linux pixel tests; no DX12 pixel-assertion suite is claimed.

Exact local commands (VS bundled CMake/CTest; Zig global cache under build/zig-global-cache):

~~~powershell
cmake --preset windows-showcase-development
cmake --build --preset windows-showcase-development --parallel 4
ctest --preset windows-showcase-development
cmake --build --preset windows-showcase-development --target NexoraShowcasePackageDevelopmentEvidence --parallel 2
powershell.exe -NoProfile -ExecutionPolicy Bypass -File build/windows-showcase-development/package/NexoraShowcase-Development/accept-v1.ps1 -EvidenceDirectory build/windows-showcase-development/artifacts/v1-local -ExpectedBuildId 3aa12995c09c
cmake --preset windows-showcase-shipping -G "Visual Studio 17 2022" -A x64 -DCMAKE_CONFIGURATION_TYPES=Shipping
cmake --build --preset windows-showcase-shipping --target NexoraShowcasePackageShippingEvidence --parallel 4
powershell.exe -NoProfile -ExecutionPolicy Bypass -File build/windows-showcase-shipping/package/NexoraShowcase-Shipping/accept-v1.ps1 -EvidenceDirectory build/windows-showcase-shipping/artifacts/v1-local-full-tour -ExpectedBuildId 3aa12995c09c -CompleteGuidedTour
~~~

Application source/build ID: 3aa12995c09cf2f36dced04b51a5fd79980f3b98 / 3aa12995c09c. C++ application source
is unchanged by this delivery. Final verifier, presets, executable and ZIP hashes are recorded in
source-provenance.json. The packaged-script invocation exposed a PowerShell 5 empty parameter-default
path; package-root initialization now happens in the body and both stock invocations above pass.
Linux preset gates will execute in the delivery PR's Linux Actions job; this local record does not
claim Codex Cloud Linux execution.

**V1 final acceptance remains PENDING.** Physical-display and independently provisioned clean-host
operator attestations were not supplied; both fields remain false in each acceptance JSON.
A separate target must run the package with the applicable -PhysicalDisplay -CleanHost flags.
Native audio/video/WebView adapters retain their contract-only/unavailable scope.

✅ 本地 GTX 960 的 Development/Full 與 Shipping/Full 通過 Windows DX12 可見驗收；
Development CTest 68/68、共 49 張截圖、JSON／Markdown 與完整 210 秒導覽已保存。
此為開發主機證據；實體顯示與獨立乾淨主機操作聲明尚未提供，V1 最終驗收保持待完成。
