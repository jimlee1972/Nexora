# V1 Metal hosted package acceptance — 2026-10-04

✅ macOS ARM64 Shipping/Full compilation, package relocation/ad-hoc signing,
checksum-verified isolated headless launch and eight-room native Metal smoke passed on
GitHub Actions [Build 37192940930](https://github.com/jimlee1972/Nexora/actions/runs/37192940930),
job `111408777243`, head `eb8719b108039823b3271a2f23a210ee4c564245`.

Commands executed on the macOS runner:

```sh
cmake --preset macos-showcase-shipping
cmake --build --preset macos-showcase-shipping --target NexoraShowcasePackageShippingEvidence --parallel 4
python3 Tools/Package/VerifyShowcaseRelease.py --package build/macos-showcase-shipping/package/NexoraShowcase-Shipping --evidence-directory build/macos-showcase-shipping/artifacts/release-native --allow-unavailable-display
```

Native acceptance is **PASS**, not UNSUPPORTED. Hub, Rendering, Scene, Input, Gameplay,
Presentation, Streaming and Shipping each execute 12 native graph frames through
`Offscreen → Main → UI → Present`, with Metal scene draws, offscreen draws, GPU copies and
presents, no backend fallback, no CPU-composed frames, and at least one resize generation.
The total is 96 native graph frames. The isolated package also completed headless verification.

`eight-room-summary.json` preserves the original windowed and lifecycle evidence and SHA-256
of each complete report. `source-provenance.json` identifies the downloadable CI artifact
containing those original reports, package, manifest and logs. The downloaded archive was
checked in this Linux session; the Mac commands ran on GitHub's macOS runner.

✅ The independent `window_presentation.metal_scene` also passes on macOS, including synthetic
Cocoa key/pointer translation, sampled texels, instancing, depth-order invariance, scene/copy/UI
ordering, descriptor rejection, frame-slot reuse, resize/suspension and teardown. The mimalloc
configuration is **63/63 CTest PASS**, with the Metal gate executed in 1.41 seconds and no skip.
Source head: `606bcf827bda8fc15674f262c363c8a1160cfeee`; [Build 37193636581](https://github.com/jimlee1972/Nexora/actions/runs/37193636581),
job `111410847239`. `ctest-macos-mimalloc.txt` retains the CTest log. The first input assertion
failed because AppKit's initial dequeue had not completed before synthetic events were posted;
initializing the queue before posting fixed the failure.

✅ Standard macOS Development with Zig/Slang enabled is also **73/73 CTest PASS**, including
`window_presentation.metal_scene` (2.33 seconds); job `111410847266`, same Build/head as above.
`ctest-macos-development.txt` preserves its original CTest log. `metal-scene.png` is a lossless PNG
conversion of the original GPU-readback PPM, showing the two sampled-texture instances. It is an
offscreen test capture, not a screenshot of the full application or physical monitor.

The Development package verifier then caught the macOS `/var` → `/private/var` temporary path
alias during library evidence serialization. The staged root now resolves before use; the new
Linux/Mac alias regression reproduces the old failure and passes with that correction. The PR's
latest Build remains the authority for complete Development package acceptance.

This accepts hosted native graph/report smoke and the separate native pixel/input gate. Screenshots of the full application, physical-display
attestation, a clean Mac host, the complete tour and expanded release-tag execution remain
separate gates. Full V1 parity is not certified. Audio/video/WebView retain their explicit
contract-only/unavailable scope.
