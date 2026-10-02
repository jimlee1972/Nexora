# Windows DX12 local developer-machine acceptance

✅ Development/Full (Modular, dynamic Zig) and Shipping/Full (Monolithic, static Zig) pass
the packaged stock Windows PowerShell 5 verifier on the local GTX 960 developer machine.
This is visible native DX12 execution, separate from headless and Linux/CI results.

| Profile | Package checksums | Client PNGs | Native graph/copy/present frames | Native instances | Exit |
| --- | ---: | ---: | ---: | ---: | ---: |
| Development/Full | 22 | 24 | 2528 | 3452 | 0 |
| Shipping/Full | 14 | 25 | 15231 | 21399 | 0 |

- ✅ Full Windows Development CTest: 68/68, including V1 contracts, Showcase probes/rooms, ABI and native backend contracts.
- ✅ Hardware rasterizer; no backend fallback or CPU image composition.
- ✅ Offscreen, Main, UI, Present: four completed callbacks and three logical transition requests.
- ✅ Eight rooms, instances, sampled checker, quad, triangle, resize and ordered shutdown.
- ✅ Win32 pointer orbit/wheel and held character input, F1/F2, Modify/Undo/Play/F5/reloaded Play,
  locale, crouch/teleport, animation blend, lifecycle/pressure and pause/replay captures.
- ✅ Live M5 empty-asset and M6 actual dynamic-library ABI rejection export JSON and Markdown.
- ✅ Shipping tour completes all seven steps at 210.002 seconds; final step index 6, paused.
- Shipping screenshots were reviewed as a contact sheet and full-size Hub/Rendering/Lab,
  overlay-hidden, modified-scene and completed-tour captures. Development images are retained.
  Controls/captures supplement CTest and Linux pixel tests; no DX12 pixel-assertion suite is claimed.

Exact local commands (VS bundled CMake/CTest; Zig global cache under build/zig-global-cache):

~~~powershell
cmake --preset windows-showcase-development
cmake --build --preset windows-showcase-development --parallel 4
ctest --preset windows-showcase-development
cmake --build --preset windows-showcase-development --target NexoraShowcasePackageDevelopmentEvidence --parallel 4
powershell.exe -NoProfile -ExecutionPolicy Bypass -File build/windows-showcase-development/package/NexoraShowcase-Development/accept-v1.ps1 -EvidenceDirectory build/windows-showcase-development/artifacts/v1-local-reviewed -ExpectedBuildId e4a140139189
cmake --preset windows-showcase-shipping -G "Visual Studio 17 2022" -A x64 -DCMAKE_CONFIGURATION_TYPES=Shipping
cmake --build --preset windows-showcase-shipping --target NexoraShowcasePackageShippingEvidence --parallel 4
powershell.exe -NoProfile -ExecutionPolicy Bypass -File build/windows-showcase-shipping/package/NexoraShowcase-Shipping/accept-v1.ps1 -EvidenceDirectory build/windows-showcase-shipping/artifacts/v1-local-full-tour-reviewed -ExpectedBuildId e4a140139189 -CompleteGuidedTour
~~~

Application source/build ID: e4a1401391896eaeb244c1efae7a91b873dc7cbd / e4a140139189. Application C++
is unchanged from baseline 3aa12995c09cf2f36dced04b51a5fd79980f3b98; rebuilt BuildInfo identifies e4a140139189. Final verifier, presets, executable and ZIP hashes are recorded in
source-provenance.json. The packaged-script invocation exposed a PowerShell 5 empty parameter-default
path; package-root initialization now happens in the body and both stock invocations above pass.
The application baseline e4a140139189 passes all 13 jobs in [Build run 37058473283](https://github.com/jimlee1972/Nexora/actions/runs/37058473283).
Linux Actions runs cmake --preset linux-development (Zig/Slang/Vulkan enabled), cmake --build --preset
linux-development and ctest --preset linux-development: 77/77 PASS. The build-contract job runs
cmake --preset linux-shipping (Zig enabled), cmake --build --preset linux-shipping and the Shipping
package-evidence target: PASS; ASan/UBSan 60/60 and TSan 59/59 also PASS. Exact observed command/log
excerpts and job conclusions are in ci-baseline.json. These are GitHub Actions Linux results;
no local Windows or Codex Cloud Linux execution is claimed. The strengthened verifier has the
separate local results above; its final PR revision is gated by a fresh Actions run before merge.

Fresh sampled-state evidence:

- ✅ F5 preserves the edited Editor World x=0.50 through snapshot round trip, clears Undo to 0
  and reports successful scene snapshot reload in scene-reload-state.json.
- ✅ tour-before-replay.json and tour-replayed.json prove elapsed progress resets to step 0
  and less than one second after R, then pauses.
- ✅ Every state export removes earlier JSON/Markdown first. Final M5 input.error_case=1 and
  M6 input.error_case=3 are observed; plugin loaded/registered remain false for ABI rejection.
- ✅ Verifier/preset UTF-8 LF bytes are pinned by repository attributes; recorded SHA-256 values
  match canonical Git files and the packaged verifier. Package/ZIP hashes reflect this final rerun.
- build.log retains the initial full-build output; reviewed-package-build.log records the final
  e4a140139189 package rebuild and ctest.log records the repeated 68/68 Windows gate.

**V1 final acceptance remains PENDING.** Physical-display and independently provisioned clean-host
operator attestations were not supplied; both fields remain false in each acceptance JSON.
A separate target must run the package with the applicable -PhysicalDisplay -CleanHost flags.
Native audio/video/WebView adapters retain their contract-only/unavailable scope.

✅ 本地 GTX 960 的 Development/Full 與 Shipping/Full 通過 Windows DX12 可見驗收；
Development CTest 68/68、共 49 張截圖、JSON／Markdown 與完整 210 秒導覽已保存。
此為開發主機證據；實體顯示與獨立乾淨主機操作聲明尚未提供，V1 最終驗收保持待完成。
