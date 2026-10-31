# Windows M7 Shipping refresh — 2026-10-31

## Verdict and artifact identity

The refreshed package is frozen at main build cut
`67db45d23e631b0e05b42c23716245a34ae996d5`. This accepts only the original M7 fixture
contracts, not the newer HD2D features, M6 performance/soaks, SP or SDK work.

- ✅ Windows dual-backend Monolithic Shipping build, closed package inventories,
  matching app-local CRT and Vulkan loader, and independent UI/Zig/Host contract staging.
- ✅ Three consumers of the actual Shipping static libraries retain assertions and exit 0.
- ✅ Developer-machine native DX12 UI: 168 frames, both panel samples RGB `(102,170,239)`,
  balanced teardown and exit 0. This is developer GPU evidence, not clean-host acceptance.
- ✅ The same packaged Zig fixture completes 120 native frames on the developer GPU,
  with exit 0 in 6.85 seconds. This is a diagnostic comparison, not VM acceptance.
- Latest-package two-cold-boot Windows VM acceptance: **NOT PASSED**; retained failures below.
- Independent clean physical Windows host: **NOT RUN**. M7 remains **NOT ACCEPTED**.

The original roadmap remains **9/11 (82%), P0 6/8**. The successful 2026-10-28 original-VM
run remains historical evidence for its original package; it is not transferred to these new bytes.

| Artifact | SHA-256 |
| --- | --- |
| Full `Nexora-M7-Shipping-20261031.zip` (3,926,599 bytes) | `fb2b89428dc9a8a584344e01d5dac2977cacea93e58814d8db3b7bb17260d10a` |
| UI/Zig VM transport ZIP | `4b4b02be67f145f1a470d7e903a4f9d35c3e1e85e5a479797718a29aaafddcbd` |
| `NexoraTwoDPlayer.exe` | `09055710e6cf4e0e3c1d30079cade0555b9228712070b05e6101ef1d179e0b50` |
| App-local `vulkan-1.dll` | `843de97fad335ceaf2f22c689aa5f68f01050e765b2de230b0be9bcff3c339ce` |

Local full artifact: `build/m7-20261031/Nexora-M7-Shipping-20261031.zip`.
The full ZIP contains the same checked UI/Zig fixture bytes and the independently checked Host
fixture. No loader or development tool was installed into either VM.

## Retained VM results

The original VM `71571b18-806d-425b-ad19-712c9043663d` could not finish its saved-state
resume within the desktop deadline. Its state was saved again without discarding state,
resetting, restoring a snapshot or changing its two existing snapshots.

The existing Recovery VM `86f280ed-50e8-4926-a761-2ce1659fcc74` is a different target,
with 3072 MiB RAM, VBoxSVGA and 3D disabled. Its baseline records 23 existing saves,
no snapshots and no system Vulkan loader. This is software DX12/WARP evidence only.

1. Intermediate source `8c4aa989` failed with scheduled-task result `267014`, without native
   case completion. Host disk exhaustion also interrupted its original cleanup logging.
   After clearing only owned regenerable build caches, normal ACPI poweroff was verified.
   Its original failed report remains unchanged.
2. Source `67db45d2`, headless attempt: `WHvSetupPartition` returned
   `0x800705aa` / `VERR_NEM_VM_CREATE_FAILED`, before any guest cases. Later observation
   confirmed poweroff; the original report's `unknown` final state is preserved.
3. Source `67db45d2`, GUI attempt: Guest Control PowerShell exceeded its 300-second deadline.
   The complete report has zero completed cycles and remains **FAILED**. Normal ACPI
   shutdown completed. A separate read-only boot collected the worker receipts and logs.
   All three worker phases exist; UI first/restart/Unicode stdout contains native PASS
   reports, and corruption stderr exists. Those partial logs do not establish the missing
   real case exits, seven-case cycle, Zig completion or two-boot acceptance.
4. A separate, bounded Zig-only diagnostic also exceeded the 300-second Guest Control
   deadline (330-second host timeout). Its follow-up copy failed while starting a guest
   session. No Zig completion report was recovered, so no runtime result is inferred.
   Normal ACPI shutdown then exceeded 180 seconds. The Recovery VM was safely saved
   (`savestate` exit 0, final state `saved`); it was not forced off or retried.

No force-off, saved-state discard, snapshot restore, host security change or deadline
increase was used. The earlier read-only collection finished with normal poweroff;
the later Zig diagnostic ended with a preserved saved state, which does not satisfy
the required normal-poweroff gate. Latest-package Host interaction and final old-save
hash comparison are also pending. The original-VM and Recovery-VM states are separate.

The final read-only infrastructure receipt confirms both VMs are `saved`, both retain
3072 MiB RAM, and the original VM's snapshot listing matches the pre-refresh baseline.
No latest-package old-save preservation PASS is claimed without its pending after-hashes.

## Executed entrypoints and retained files

On this Windows executor, the ignored local helpers were invoked as:

```powershell
cmd /c build\m7-20261031\Build-Shipping.cmd
cmd /c build\m7-20261031\Test-Shipping.cmd
cmd /c build\m7-20261031\Stage-Packages.cmd
python Tools/TwoD/WindowsVmAcceptance.py --vm 86f280ed-50e8-4926-a761-2ce1659fcc74 --username nexora --password-file <existing-private-password-file> --package-zip build/m7-20261031/stage-01/package.zip --vboxmanage D:/Oracle/VirtualBox/VBoxManage.exe --frontend gui --boot-timeout 600 --guest-timeout 300 --output build/m7-20261031/vm-two-boot-02
```

The build uses the `windows-showcase-vulkan-shipping` preset with both native backends,
2D enabled, graphical Editor disabled, Shipping-only configuration and explicit Zig/Vulkan
import-library paths. The matching loader is supplied through the existing exporter
`--vulkan-loader` contract. These local helper paths are retained for the local artifact,
not shipped into the guest; see the [portable procedure](../../../../Tools/TwoD/WindowsAcceptance.md).

The accompanying inventories verify every full-ZIP fixture member; configuration/build/stage
logs and Shipping consumer exits record local results. Partial GUI case logs and worker phase
receipts are explicitly diagnostic. `final-vm-states.json` records the later saved states;
the earlier failed reports are preserved rather than rewritten. `source-manifest.json` and
`evidence-sha256.json` bind the source, harness and retained files.

## Acceptance-tool regression checks

The hidden worker now configures noninteractive preferences before hashing, uses intrinsic
.NET streaming SHA-256, and records immutable started/hash-verified/script-parsed receipts.
These diagnose the boundary before native cases; they do not replace native completion.
The 300-second guest, four-minute task and native process deadlines remain unchanged.

- `python Tests/TwoD/WindowsVmAcceptanceTests.py`: **12 passed** on Windows.
- `python Tests/TwoD/WindowsVmAcceptanceTests.py --guest-tests`: real PowerShell child exit,
  known SHA-256 vector, changed-script rejection, immutable phases, strict UTF-8/NUL and
  concurrent full-pipe regressions **PASS**.
- Hosted full Linux Development/Shipping and cross-platform CI: pending for PR #752.
  No Linux validation is claimed from this Windows executor.

Existing PS4 USB, audio and supported 100/150/175% DPI evidence is retained with its existing
scope. This refresh adds no physical device, default audio endpoint recovery, hardware VM,
clean-host or performance acceptance.
