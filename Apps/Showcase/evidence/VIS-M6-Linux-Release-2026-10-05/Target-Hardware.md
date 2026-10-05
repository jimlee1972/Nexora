# Remaining target-hardware acceptance

All engine/showcase implementation work is delivered. VIS-M3 still needs the hero/material images
reviewed on the actual Windows target, and VIS-M6 needs physical DX12/Vulkan visuals plus the
confirmed performance budget. The cloud host only exposes lavapipe software Vulkan. Earlier
GTX 960 evidence predates these effects and cannot accept this version.

Use checksum-verified Windows DX12 and Vulkan Shipping/Full packages from the final PR #329 CI
artifacts. Retain their own launch build IDs and binary hashes; platform binaries differ. The
retained Linux ZIP/video/benchmarks are one release at source 3047d5c68de939c6ead3bfceb8a47c5d6ccf3524.

On the physical machine, run each package's script separately:

```powershell
./accept-v1.ps1 -Backend dx12 -PhysicalDisplay -EvidenceDirectory ./physical-dx12
./accept-v1.ps1 -Backend vulkan -PhysicalDisplay -EvidenceDirectory ./physical-vulkan
```

The physical flag is an operator attestation; the script also rejects a software rasterizer.
Review the wide/material/motion shots, effect comparisons, quality changes, shadow edges and
pause/replay on the actual display. Record approval/rejection explicitly; a success JSON alone
does not approve the art. Record OS, GPU, driver, resolution and physical display configuration.

With other builds/apps/recording idle, collect each quality three times on both backends:

```powershell
foreach ($quality in 'basic','standard','high') {
  foreach ($repeat in 1..3) {
    & ./bin/NexoraShowcase.exe --scene=courtyard --backend=dx12 --quality=$quality `
      --pause-animation --activate-device --clean-view --vsync=off --frames=360 `
      --no-reload --gameplay-module=static --report="physical-dx12/$quality-$repeat.json"
    if ($LASTEXITCODE -ne 0) { throw 'Native benchmark failed' }
  }
}
```

Repeat in the Vulkan package with `--backend=vulkan` and `physical-vulkan/...` outputs. Each
report must be PASS, use the requested native backend with no fallback/software rasterizer,
have 300 measured samples after 60 warm-up frames, and record the actual quality and immediate
present mode. Retain FPS, P95/P99, process CPU and memory; GPU timing remains unavailable until
GPU timestamps exist. Confirm or revise the candidate GTX 960 / 1280×720 / 60 FPS budget, then
compare against that confirmed budget. No candidate budget is implicitly accepted.

Finally run `./bin/NexoraShowcase.exe --tour=visual --backend=dx12 --quality=standard --clean-view`
and the Vulkan equivalent for native viewing. The route ends automatically after 100 seconds;
interactive T starts it, Space pauses camera/effects, R replays, C explores and Q changes quality.
No additional source implementation or approval flow is needed to perform these checks once a
suitable machine or its acceptance evidence is available. Full physical Metal remains deferred.
