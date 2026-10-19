# Windows PS4 input and scoped M7 acceptance — 2026-10-19

## Result and limits

**PASS for the current physical developer-host PS4 USB slice; M7 remains PARTIAL / NOT ACCEPTED.**
Source starts at remote main `826b27be`; the later documentation-only main update `edea455f`
is synchronized without changing tested runtime/fixture sources. The existing Windows 10 FX-6100 / GTX 960 developer PC
was used. No clean machine, new driver, remapper, controller emulator or virtual input was used.
The attached Sony DUALSHOCK 4 USB device identifies as VID 054c / PID 09cc. Native logs show the
actual DirectInput slot and model profile. XInput support is retained; no XInput device is required
for this acceptance. Other models/connections and simultaneous mixed-controller configurations
are not established by this single-device run.

The [source/binary hashes](source-and-binary-sha256.json) identify the exact runtime, decoder,
fixture and packaged Shipping Player used below. Raw generic report markers such as
`physical_windows: NOT RUN` and `audible_acceptance: NOT VERIFIED` are unchanged; target-host and
human observations supplement them instead of rewriting them.

## Physical observations

| Check | Evidence | Accepted scope |
| --- | --- | --- |
| Hardware DX12 / Vulkan | [DX12 report](final-dx12.json), [Vulkan report](final-vulkan.json), [DX12 log](final-dx12.log), [Vulkan log](final-vulkan.log) | 120 native frames each, NVIDIA vendor 4318 / device 5121, DirectInput PS4 slot 0, successful audio submission, final save and balanced teardown; no additional human Vulkan gameplay claim |
| Initial acquisition | Both native logs | No idle L2/R2 half-press after deferring the first snapshot until the input frame following acquisition; both trigger minima/maxima remain zero in the neutral runs |
| Left stick / triggers / UI | [First human run](manual-report.json), [module log](manual-stderr.log), [human replies](human-observations.json) | 3,794 native frames, 200 gamepad movement frames, left X -1..1, both triggers 0..1, face/D-pad input, six UI cancels, six bus changes; user confirms movement/release, navigation/Cross/Circle, held-input Alt-Tab, keyboard switching, USB removal/reconnection and normal X close |
| Right stick | [Second human run](manual-02-report.json), [module log](manual-02-stderr.log) | 1,784 native frames; RX and RY each -1..1, four UI cancels; user confirms normal operation |
| Focus / restart / shutdown | Both manual reports/logs | First run records a focus loss and zero unfocused movement; 11 keyboard movement frames. Both processes exit normally with balanced allocations. Second independent process loads position 0.138389 and rebound key F saved by the first |
| Music playback | Second module log and human reply | One play, pause and stop; user replies normal. Endpoint/connection was not specified. One successful output open and zero pump failures do **not** prove endpoint loss, backend reopening or default-device migration |
| Engine storage / corrupted data | [First process](storage-first.stderr), [resume](storage-resume.stderr), [corruption](storage-corrupt.stderr) | Final Shipping headless fixture restores position 17.25 and rebound F in another process, rejects corrupt save without replacing its bytes, and preserves immutable package hashes |
| Launcher isolation | [Launcher record](launcher-isolation.json) | Run.cmd exits 0 from an unrelated Unicode CWD with Windows-System32-only PATH and SDK variables removed; installed developer PC, headless three frames, not clean-host/double-click acceptance |

The second manual request included SAVE/move/OPEN, but its SAVE/OPEN counters stayed zero.
No new manual SAVE/OPEN pass is inferred from the general reply. Automated Shipping storage
checks above and the earlier [2026-10-17 human SAVE/OPEN evidence](../Windows-Host-M6M7-2026-10-17/acceptance.md)
remain distinct. That earlier record also retains human Chinese UI/audio and 100/150/175% DPI
acceptance; this run does not claim another DPI-setting operation or new screenshots.

## Implementation and regressions

Windows owns DirectInput interfaces on the adapter thread and polls without exclusive access.
Attached controllers are rediscovered at most once per second; failed reads release input state,
and reacquisition never blocks in a retry loop. The existing four slots are shared with XInput;
`IG_` DirectInput paths are excluded to avoid duplicate XInput submission. Sony 05c4/09cc profiles
map face buttons, POV, sticks and independent triggers to existing Windows gameplay/UI controls;
other joystick layouts are rebindable. No C/Zig ABI layout changes or third-party dependency is added.

The initial physical smoke exposed half-pressed triggers from the initial DirectInput cache.
The final 120-frame neutral runs above verify the acquisition fix on this device. The new system
`DINPUT8.dll` dependency is admitted by ExportGame's OS-import list; it is provided by Windows,
not copied from an arbitrary developer directory.

Copied-report tests use the real GameplayInput provider to cover PS4 and generic mappings,
diagonal/neutral POV, full-sample clearing, focus/disconnect recovery, invalid ranges, slot isolation
and owner-thread rejection. They remain separate from native physical-device evidence.

## Validation commands and results

- `cmake --build build/windows-development --config Development --parallel 2`: full MSVC build PASS.
- `ctest --test-dir build/windows-development -C Development -R '^(runtime.gameplay_|twod\.|build.module_graph)' --output-on-failure`: **27/27 PASS**, zero skips, 99.45 seconds; [JUnit](windows-final-junit.xml).
- `cmake --build build/windows-showcase-shipping --config Shipping --target NexoraTwoDPlayer NexoraTwoDAssetCompiler NexoraTwoDAudioProbe --parallel 2`: PASS, Monolithic Shipping with DX12/Vulkan enabled.
- Independent fixture: MSVC `cl /LD /MD /O2 /DNDEBUG /EHsc /std:c++20 /utf-8 /W4 /WX /I Engine/API/include Tests/TwoD/HostModule.cpp`: PASS; output and object paths are under `build/windows-ps4-m7-20261019/`.
- `BakeWindowsFont.ps1 -TextFile <fixture-corpus> -Output <fresh-font.nxfont>` under Windows PowerShell 5: PASS, Microsoft JhengHei corpus includes the diagnostic labels.
- `HostAcceptance.py --app <Shipping Player> --module <fixture DLL> --shipping --redist <matching VC143 CRT> --real-font <font> --output <fresh>`: PASS storage/corruption/immutability; `--stage-only` creates the physical-run package.
- Packaged Player: `--run-2d-project <package/game.nx2game> --native --backend dx12|vulkan --frames 120 --save-directory <fresh> --report <fresh.json>`: PASS for both hardware backends. Manual DX12 runs omit `--frames` and use fresh report paths with a dedicated test-save directory.
- clang-format 19.1.5 `--dry-run --Werror` on touched C++ files, Markdown link/bilingual checks and `git diff --check`: PASS. Root README is unchanged.

The first local CTest attempt lacked dumpbin in PATH; both export tests pass in the final complete
MSVC environment. Shipping configurations do not generate the test-only HostFixture CMake target,
so the public-C-ABI fixture is compiled independently using the command above. Neither setup
failure is treated as runtime acceptance or suppressed in the final checks.

Full hosted Linux Development configure/build/CTest and Linux Shipping remain required on the
final PR head before merge. They have not run on this Windows executor. Their result belongs to
the final PR/CI record; Windows measurements above do not substitute for Linux validation.

## M7 scope and remaining gates

The user explicitly narrowed M7 to standalone 2D Runtime/Shipping; see the synchronized
[English roadmap](../../../../Roadmap/en/2D_Roadmap.md#m7-runtimeshipping-acceptance-scope-revised-2026-10-19)
and [Traditional Chinese roadmap](../../../../Roadmap/zh-TW/2D_Roadmap.md#m7-runtimeshipping-驗收範圍2026-10-19-修訂).

- Any physical joystick is sufficient; this PS4 USB run supplies the current controller evidence.
- Current single-monitor DPI is 100/150/175%. **200% is removed from requirements and pending work**;
  cross-monitor testing applies only to a declared multi-monitor target.
- Editor Game View acquisition/Play/Stop belongs to Editor acceptance; production gameplay,
  content and save-schema migration belong to game projects; minimum hardware, performance,
  integrated 10k UI/VFX, allocator/VRAM and soaks belong to M6. None blocks M7.
- **Independent clean Windows host gate remains OPEN.** Developer-PC isolation and existing VM
  runs are not a pristine hardware target.
- The user additionally confirms all is normal and was tested previously. Existing human
  Chinese UI/audio/supported-DPI acceptance is reused, not repeated. Untriggered endpoint-loss/
  reopen/default-migration paths remain outside the evidence's compatibility claim; they do not
  introduce another mandatory local fault-injection run. Available current-host acceptance is complete.

M7 is not marked complete; overall acceptance stays **9/11 (82%), P0 6/8**. M6 status is unchanged.
