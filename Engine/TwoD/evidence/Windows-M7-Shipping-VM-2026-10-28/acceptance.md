# Windows M7 Shipping package and VM acceptance — 2026-10-28

## Scope and current verdict

The original M0–M10 continuation excludes M6 performance/soaks, SP features, SDK development
and production game content. Base: remote main `0c718ad0fd90b82eba6d853269d73c09476d0cd1`.
The exact tested source and package bytes are identified by the accompanying SHA-256 records.
The earlier failed 2026-10-19 package is preserved; it is not relabeled or edited.

**New dual-backend Shipping package and local contracts: PASS. VM acceptance: pending.**
**Independent clean hardware Windows host: NOT RUN. M7 remains PARTIAL / NOT ACCEPTED.**
Overall M0–M10 acceptance remains **9/11 (82%), P0 6/8**. M6 is unchanged.

## Build blockers repaired

The initial current-main MSVC build failed because the source-aware extraction overload was
defined twice, stale code treated optional source UUIDs as direct UUIDs, and `SpriteGeometry`
was defined inside an unrelated anonymous namespace. Keep the existing source-aware adapter
in `SpriteAsset.cpp`, with the historical three-argument extraction in `Scene.cpp`.

The first actual Zig export then failed during module initialization: legacy ABI accessors
compared an empty optional with a zero UUID and incorrectly rejected source-free Sprites.
The same comparison existed in Editor document admission. Both now test source presence;
present source references remain unsupported there, and present zero UUIDs remain invalid.
The existing roundtrip tests now assert absence rather than equality to a present zero UUID.
No C/Zig ABI, source schema, module dependency or ownership boundary is expanded.

## Executed Windows validation

- Latest-main MSVC x64 Monolithic Shipping with `NEXORA_ENABLE_2D=ON`, native DX12 and Vulkan,
  Full profile, graphical Editor off: Player, asset compiler, audio probe and Zig DLL build PASS.
- Independent ProjectModule/HostModule consume only public C ABI headers; `/MD /O2 /W4 /WX`: PASS.
- Existing `SpriteAssetTests.cpp`, `TwoDTests.cpp` and `GameplayBridgeTests.cpp` compiled as
  consumers of the actual Shipping static libraries: **3/3 PASS**. Test consumers retain assertions;
  the production libraries retain Shipping defines/optimization. This is not a full Windows CTest run.
- `python Tests/TwoD/WindowsVmAcceptanceTests.py`: **12/12 PASS**.
- `python Tests/TwoD/WindowsVmAcceptanceTests.py --guest-tests`: Windows PowerShell UTF-8/NUL,
  immutable evidence and dual-pipe regression PASS.
- `ZigProjectAcceptance.py` with `--shipping --vc-redist <matching-VC143-CRT> --vulkan-loader
  <explicit-loader>`: actual separate Zig DLL, dependency closure and Cooked corruption rejection PASS.
- `HostAcceptance.py` with `--shipping --real-font <fresh-Microsoft-JhengHei-atlas> --redist
  <matching-VC143-CRT> --vulkan-loader <explicit-loader>`: fresh process, state restore, corrupt-save
  byte preservation and package immutability PASS. The font is baked from the current fixture corpus.
- clang-format 19.1.5 on touched C++ and `git diff --check`: PASS.

PR #730 source-repair head `e7cf1d6b5d7ff21546c6a70c91b7367c511413fd` passed the full hosted
Linux Development configure/build/CTest gate (**476/476**, 1821.05 seconds), Linux Shipping,
Windows/macOS desktop and the complete PR Build matrix (run `40713158154`). The push Build
matrix (`40713012951`) and Windows SDK candidate workflow (`40713158117`) also passed.
These are hosted CI results; no Linux gate ran on this Windows executor. Later commits require
their own exact-head checks. Initial compiler, test setup and font invocation failures remain in
ignored local logs.

## Presented-panel regression

The current Player encodes the fixture's linear RGBA8 `(34,102,221)` into sRGB `(102,170,239)`.
The older VM probe compared the displayed pixels to the unencoded values and rejected a correct
frame. Update only the expected color, retaining the two client-space samples and tolerance 12.
Developer-hardware DX12 capture confirms both samples before and after: the old probe rejects
48 native frames; the corrected probe accepts 176 frames. Both runs exit 0 with balanced allocations.
The guest regression also rejects the old unencoded color, black and invalid pixels. These captures
verify the probe against actual Shipping output, not VM or independent clean-host acceptance.

Read-only transport diagnosis on the original VM found pipeline, Console and unquoted-argument
PowerShell commands all successful (10.62–24.39 seconds). It does not establish a Console or quoting
defect. The first formal two-boot attempt timed out after 300 seconds before producing a cycle
report; its guest directory contains the staged ZIP and script only. Keep that attempt FAILED.
The diagnostic boot ended through normal ACPI shutdown. No deadline or exit check was relaxed.

## Package and distribution

UI, independent Zig and Host fixtures contain matching VC143 CRT and explicit app-local Vulkan
Loader **1.4.309.0**. All files, including its upstream license/provenance, are hash-inventoried.
The loader is not installed into the VM and does not provide a Vulkan ICD/GPU driver.
Upstream license: <https://github.com/KhronosGroup/Vulkan-Loader/blob/v1.4.309/LICENSE.txt>.

Local reproducible commands and artifacts are under `build/m7-20261028/`:

```powershell
cmd /c build\m7-20261028\Build-Shipping.cmd
cmd /c build\m7-20261028\Test-Shipping.cmd
cmd /c build\m7-20261028\Stage-Packages.cmd
```

The successful fresh export is `stage-03/`; earlier incomplete staging directories remain preserved.
`stage-03/package.zip` contains the UI/Zig fixtures for the existing two-cold-boot VM runner.
`stage-03/host-contracts/package/` supplies the separate Chinese UI/input/service fixture.

## Target gates and preservation

The original Windows VM UUID is `71571b18-806d-425b-ad19-712c9043663d`, configured with 3072 MiB,
VBoxSVGA, 3D/audio disabled. Both historical snapshots were recorded before this run.
The initial preflight correctly refused to start the VM while available RAM was below guest RAM
plus the unchanged 1024 MiB reserve. No budget or deadline was relaxed.

VM software rendering cannot accept physical GPU performance, audible output or physical devices.
Existing 2026-10-19 PS4 USB/hardware DX12/Vulkan/Chinese UI/audio/100–150–175% DPI evidence is retained
within its original scope; changed paths require their own regression evidence.
The independent hardware clean-host gate remains mandatory and requires the same package hash,
offline startup without Editor/Zig/Python/MSVC developer tools, hardware DX12, basic interaction
and normal shutdown. No independently accessible clean hardware host is configured in this session.

Beads: package `nexora-j4q.7.12.2`, two-boot VM `nexora-j4q.7.13`, clean hardware
`nexora-j4q.7.14`, evidence delivery `nexora-j4q.7.15`; parent M7 remains open.
