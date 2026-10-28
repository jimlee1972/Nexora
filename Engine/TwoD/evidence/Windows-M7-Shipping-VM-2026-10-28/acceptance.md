# Windows M7 Shipping package and VM acceptance — 2026-10-28

## Scope and current verdict

The original M0–M10 continuation excludes M6 performance/soaks, SP features, SDK development
and production game content. Base: remote main `0c718ad0fd90b82eba6d853269d73c09476d0cd1`.
The exact tested source and package bytes are identified by the accompanying SHA-256 records.
The earlier failed 2026-10-19 package is preserved; it is not relabeled or edited.

**New dual-backend Shipping package, local contracts and two VM cold boots: PASS.**
**Supplementary Host keyboard/mouse, SAVE/OPEN, resume and old-save preservation: PASS.**
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

At probe-fix head `cc6459c3ca4924af856668a10b5ccd68a3a54c30`, the Linux Shipping isolated-copy
Showcase interaction exceeded its existing 90-second deadline (run `40724838319`, job
`122947235483`). `ci-prior-failure.json` preserves that failed check and its uploaded artifact hash.
No cause is assigned and no deadline is increased. Final-head CI must pass before merge;
the earlier source-repair success is not substituted for a failed or unfinished final check.

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

## Original VM two-cold-boot acceptance — 2026-10-29

`vm-two-boot.json` records the successful fresh run `run-3380a624b2c74503b92bcfdee2295e71`.
Both boots used the original **3072 MiB** RAM, unchanged **1024 MiB** host reserve, 600-second
desktop boot deadline and 300-second guest deadline. Measured host availability before each boot
was **4279 / 5019 MiB**. Neither the failed 2 GiB attempt nor diagnostic boots count as accepted cycles.

Each boot passes the same seven cases: UI startup, separate-process restart, corrupt-save rejection,
Unicode save path, real Zig DLL/native Cooked scene, corrupt-Cooked rejection and presented-panel
normal window close. Expected exits are 0 for success and 1 for the two negative cases. The primary
visit counter survives normal shutdown and cold boot **0→2→4**; corrupt save bytes remain unchanged.
The Zig scene reports **34 visible Sprites and 120 native frames** per boot. Success cases report
balanced allocations; both cycle reports have **0 residual Players**, and both boots end `poweroff`.
Presentation is API 1 / Microsoft software DX12 (vendor 5140, device 140), not physical GPU acceptance.

```powershell
python Tools/TwoD/WindowsVmAcceptance.py --vm 71571b18-806d-425b-ad19-712c9043663d `
  --username nexora --password-file $VmPasswordFile `
  --package-zip build/m7-20261028/stage-03/package.zip `
  --vboxmanage D:/Oracle/VirtualBox/VBoxManage.exe --frontend headless `
  --boot-timeout 600 --guest-timeout 300 --output build/m7-20261028/vm-two-boot-02
```

This is the executed command with only the existing credential-file path replaced by a variable.
Use a new output directory for any new run; preserve old evidence and saves. Raw Guest Control
stdout/stderr and operation records remain under the local output directory. The public report
contains the exact per-case stdout/stderr, real exits and source/package hashes.

## Host fixture interaction and preservation — 2026-10-29

`host-vm.json` records a separate normal cold boot of the same VM and the same inventoried Host
fixture bytes. `host-input-guest.ps1` is the exact executed guest procedure; it uses the matching
`WindowsVmGuestAcceptance.ps1` as `definitions.ps1` in its fresh run directory. The host wrapper
transfers those files and the Host ZIP, verifies their hashes and invokes the script through
the existing InteractiveToken desktop, without changing security or storing a scheduler password.

The exported `Run.cmd` starts from an unrelated Unicode working directory, without `--frames`
or `--native`, using only system PATH entries and its own app-local dependencies. Automated
keyboard/mouse input produces **129 native frames, 1 key press, 2 mouse presses, 21 keyboard
movement frames, SAVE 1 and OPEN 1**. Position `1.812691` is saved on normal close and loaded by
a separate headless process; launcher and resumed process exit 0. Allocation balance, inventory
hashes and **0 residual Players** pass. `host-native.png` shows the Chinese UI and saved position;
the screenshot alone is not the acceptance criterion. Audio open successes are 0 on the audio-disabled
VM, and no physical input, audible output or clean hardware acceptance is claimed.

`preservation.json` and the before/after catalogs prove **all 23 pre-existing save files unchanged**
by size and SHA-256. Seven new saves under the formal VM run are isolated from them. Both historical
snapshots remain identical. Final VM state is `poweroff`, original RAM **3072 MiB** is restored,
2 vCPUs / 64 MiB VRAM / VBoxSVGA / 3D off / audio off match the original settings, and
`C:\Windows\System32\vulkan-1.dll` remains absent. No snapshot was restored/deleted, no old save
was replaced, no developer tool or system loader was installed, and no security policy was changed.
`local-artifact-sha256.json` identifies the preserved local reports, raw output logs and transport ZIPs.

The initial read-only save-catalog diagnostic used the guest's CP950 console. Its original bytes
were retained and decoded only after an exact encode/decode roundtrip with no replacement
characters; `baseline-console-encoding.json` records both raw and normalized UTF-8 hashes.
This console-only normalization does not change the strict UTF-8/non-NUL admission rules for
package inventories or acceptance reports. The latter are produced as durable UTF-8 files.

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
The complete local distribution is `Nexora-M7-Shipping-20261028.zip`, SHA-256
`f44bcc4c58569edabec2611f77c300a61bcc765213e62e31454bdcd903cc12c4` (3,751,713 bytes).
`distribution.json` verifies every UI/Zig/Host member against the individually tested staged bytes.
Its full-distribution hash differs from the runner's UI/Zig-only transport ZIP hash by design.

## Target gates and preservation

The original Windows VM UUID is `71571b18-806d-425b-ad19-712c9043663d`, configured with 3072 MiB,
VBoxSVGA, 3D/audio disabled. Both historical snapshots were recorded before this run.
The initial preflight correctly refused to start the VM while available RAM was below guest RAM
plus the unchanged 1024 MiB reserve. No budget or deadline was relaxed.
One unsuccessful attempt temporarily used 2048 MiB while retaining that reserve. The two accepted
boots and Host interaction use the original 3072 MiB configuration; the failed attempt remains FAILED.

VM software rendering cannot accept physical GPU performance, audible output or physical devices.
Existing 2026-10-19 PS4 USB/hardware DX12/Vulkan/Chinese UI/audio/100–150–175% DPI evidence is retained
within its original scope; changed paths require their own regression evidence.
The independent hardware clean-host gate remains mandatory and requires the same package hash,
offline startup without Editor/Zig/Python/MSVC developer tools, hardware DX12, basic interaction
and normal shutdown. No independently accessible clean hardware host is configured in this session.

Beads: package `nexora-j4q.7.12.2`, two-boot VM `nexora-j4q.7.13`, clean hardware
`nexora-j4q.7.14`, evidence delivery `nexora-j4q.7.15`; parent M7 remains open.
