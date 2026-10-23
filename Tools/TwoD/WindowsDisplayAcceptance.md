# SP-01 Windows display acceptance

Status: **NOT RUN / NOT ACCEPTED** on the current Linux implementation host. Cross-compilation
or a passing headless consumer is not evidence of Windows native rendering or physical input.
Run this procedure against the exact PR head; retain commit, tool versions, GPU/backend, Windows
build, display resolution/scaling, report JSON, console output, screenshots and operator results.

## Build and package

Use an x64 Visual Studio developer PowerShell, Python and Zig 0.14.0. Supply your local Zig path:

```powershell
cmake --preset windows-development -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON `
  -DNEXORA_ENABLE_NATIVE_BACKENDS=ON -DNEXORA_ENABLE_VULKAN_BACKEND=OFF `
  -DNEXORA_ZIG_EXECUTABLE=C:/Tools/zig/zig.exe
cmake --build --preset windows-development --target NexoraTwoDPlayer NexoraZigDisplayBuild
New-Item -ItemType Directory -Force display-source/bin | Out-Null
Copy-Item build/windows-development/Gameplay/Zig/NexoraZigDisplay.dll display-source/bin/
```

Save `display-source/game.json` (the module is a Zig-only verification fixture):

```json
{
  "schema": 1,
  "id": "com.nexora.display.acceptance",
  "title": "SP-01 Zig display acceptance",
  "scene": "Main",
  "module": "bin/NexoraZigDisplay.dll",
  "audio": "off",
  "assets": [],
  "display": {"width": 1920, "height": 1080, "mode": "windowed", "resizable": true}
}
```

Run the development Player directly for native acceptance:

```powershell
build/windows-development/Apps/TwoDPlayer/NexoraTwoDPlayer.exe `
  --run-2d-project display-source/game.json --backend dx12 --report display-report.json `
  2> display-console.log
```

The unbounded project mode creates a native surface. The module requests borderless desktop
mode, then restores the saved window, and presents a yellow world-coordinate marker plus an A
button. Clicking A toggles UI 100/150% and logs activation. It does not add game rules to C++.
Close the native window normally to publish the report. Retain failed runs/output too.

To inspect a relocated package, use ExportGame.py with `--allow-development` and a fresh output;
its inventory must remain explicitly marked as a development test copy. Complete clean-host
Shipping distribution is SP-07 and cannot be inferred from this development procedure.

## Required observations

| Scenario | Required result |
| --- | --- |
| Start at Windows 100,125,150,175% scaling | Actual initial client pixels and DPI are correct without first moving/resizing; UI and pointer align |
| Desktop/work area smaller than requested 1080p | Decorated outer rectangle fits work area, remains centered and keeps client aspect within integer rounding |
| Native mode requests | Borderless covers the monitor desktop; restoring returns original window size/location/style; failures are explicit |
| Manual resize: 1920x1080, 2520x1080, 1024x768, 3441x1081 | Viewport centered; narrow windows have top/bottom bars, excessive width has side bars; no stretched world |
| UI 100/150% with every DPI | A button's visible bounds match hover/click; a press/release produces one log entry and toggles size once |
| World marker and bars | Marker stays at the same camera-space location; Zig inverse transform matches; bar points produce no world mapping |
| Minimize/restore and repeated mode switches | Zero extent is not rendered or divided by; restored frame has current extent/DPI and no stale geometry |

For repeated mode requests restart the fixture, and include a borderless startup descriptor run.
For the fixed-size variant set `allow_resize` in the Zig fixture to 0, rebuild
`NexoraZigDisplayBuild`, copy the rebuilt DLL, and repeat with descriptor `resizable:false`.
That variant requests resizable=0 on both transitions; restore must preserve the fixed style.
Use the normal allow_resize=1 variant for the manual drag-resize matrix. Move between monitors if available and record availability;
single-monitor evidence must not claim cross-monitor acceptance. Keep this gate open until actual
operator evidence is committed. This does not validate SP-06 focus/save/power behavior or SP-07 performance.
