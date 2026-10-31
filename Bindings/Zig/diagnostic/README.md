# DEV ONLY HD2D Zig diagnostic

These synthetic sprites, linear red emission mask and companion links are reproducible with
`python Tools/TwoD/PrepareHD2DDiagnostic.py --compiler <NexoraTwoDAssetCompiler> --check`.
The Noto raster font is reused under the accompanying OFL/NOTICE. It is test output, not formal
Onmyoji art. Keep both notices in any redistributed SDK or diagnostic package.

Use `nx.hd2d_diagnostic.State.start(host, .{})`, `.update(delta_seconds)` and `.stop()` in the
owning module lifecycle. The default interactive mode has no automatic transitions or capture
holds. Its eight buttons toggle effects, 100/75/50 percent strength, contour, native shadow map,
24/28-metre camera with rotation/mirroring, pause, UI 100/150 percent and actual asset reload.
Keyboard/controller UI navigation uses the ordinary Player bindings. UI scale changes leave the
camera unchanged. The red hazard outline stays visible independently of decorative strength.

The validation wrapper enables 12 phases across 144 frames, two real mask/base unload/reload
cycles and two 1.5-second window-capture holds. It is not a frame-rate test. Native timestamp
measurements identify completed GPU work; unsupported timing is explicit. A headless/old host
uses ordinary 2D without claiming native effects. The native shadow-map button displays GPU
angular distances; an inactive chosen slot is magenta. White HUD remains outside effects.

Build `NexoraZigHD2D_diagnosticBuild` for interactive use or `NexoraZigHD2D_validationBuild` for
regression. Stage a new persistent package:

```sh
python Tools/TwoD/StageHD2D.py --app <Player> --module <Zig-library> --output <new-directory>
```

Development copies require `--allow-development` and remain explicitly marked in the inventory.
Append `--gpu-timing` to the Player command for native per-pass observations. Shipping exports
must use the matching Shipping Player. No Editor, engine source or tools are required to run an
already exported package. CTest `gameplay.zig_hd2d_native` preserves each attempt, four window
sizes, reports and Linux native captures/synthetic pointer evidence. It does not establish
physical keyboard/controller/DPI, formal game art or target-machine performance acceptance.

Linux capture automation waits for two actual Zig-sampled focused neutral updates at the target
pointer location before pressing, and a sampled button-down after UI processing before release.
This validation-only handshake keeps lifecycle suppression and the exactly-one action/camera
isolation checks intact; interactive controls do not contain sleeps or synthetic input.

Native moving Sprite poses use the last successfully submitted light/caster time, matching the
next-boundary effects snapshot, including after zero-size suspension. UI reload requests execute
at the following update's start rather than replacing instances after their poses were prepared.
Fallback still uses its current unlit time; this does not change the engine publication contract.

Each native regression case retains a 90-second end-to-end watchdog. The four-case CTest suite
has 420 seconds (four case bounds plus export/teardown), rather than an inconsistent 180-second
aggregate. An isolated Windows Shipping consumer took about 53 seconds for one 144-frame native
run. These watchdogs are not product FPS gates; pixel tolerances and target-PC gates are unchanged.

macOS staging currently supports explicit Development diagnostics only: use the native `.dylib`
and `--allow-development`. Staging needs `otool`, `install_name_tool` and `codesign`; copied
dylibs use loader-relative imports and ad-hoc signatures, all covered by the inventory. This
does not provide signed/notarized macOS Shipping distribution. Runtime execution needs the
exported files, not these staging tools or the engine build tree.
