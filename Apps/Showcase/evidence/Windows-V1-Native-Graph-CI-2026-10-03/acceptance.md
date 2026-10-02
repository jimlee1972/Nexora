# Windows hosted-CI native graph acceptance

✅ Full Shipping/Monolithic package and stock Windows PowerShell 5 isolated-copy acceptance pass
in [CI run 37053279518](https://github.com/jimlee1972/Nexora/actions/runs/37053279518),
[artifact 11247343194](https://github.com/jimlee1972/Nexora/actions/runs/37053279518/artifacts/11247343194).

- 14 package checksum records verified in the isolated temporary copy.
- Eight room views, quad, triangle, Validation Lab and resize: 12 unobscured client PNG captures.
- Sampled M5 asset error and M6 real plugin ABI rejection pass.
- DX12 selected without backend fallback or CPU composition; 339 native graph/acquire/offscreen/copy/present frames.
- Four completed callbacks in Offscreen, Main, UI, Present order; three logical transition requests.
- Native texture uploads, two resize generations, isolated Engine module resolution, process exit code 0.
- Build ID `c05caaf14044` identifies the PR merge tested by Actions. The head commit and archive hash are recorded in source-provenance.json.

Commands run by the Windows CI job:

```powershell
cmake --preset windows-showcase-shipping
cmake --build --preset windows-showcase-shipping --target NexoraShowcasePackageShippingEvidence --parallel 4
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools/Package/AcceptShowcaseWindows.ps1 -PackageRoot build/windows-showcase-shipping/package/NexoraShowcase-Shipping -EvidenceDirectory build/windows-showcase-shipping/artifacts/showcase-windows-v1 -ExpectedBuildId ($env:GITHUB_SHA.Substring(0, 12)) -AllowUnavailableDisplay
```

The hosted VM reports Microsoft Hyper-V Video. This is actual Windows/DX12 execution evidence,
not physical-display or independently provisioned clean-host acceptance. Both fields remain false
in acceptance.json. The user will perform those gates locally. No Linux-cloud execution is claimed
for these Windows commands. CI screenshot review confirms the Showcase is unobscured.

✅ All jobs in this revision’s Build run pass, including Windows/Linux/macOS Development,
Full Windows package/native verifier, mimalloc and ASan/UBSan/TSan contract gates.
Android/iOS jobs remain configure/Zig-object smoke scopes, not device runtime acceptance.
