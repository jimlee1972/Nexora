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
- ✅ Latest-package isolated Windows VM: **two cold boots / 14 cases PASS**, durable visits
  **0→2→4**, real process exits, balanced teardown, zero residual Players and normal poweroff.
- ✅ Latest-package Host launcher/input/SAVE/OPEN/independent resume **PASS**: 205 native
  frames, 26 keyboard movement frames, two mouse presses, one save and one open, position
  `1.979274` restored by a separate process, exit 0, balanced teardown and zero Players.
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
5. A full isolated clone was created from existing clean snapshot
   `9d46e380-d1e3-46d5-8847-fe2bd889d4e8`, without restoring either original VM.
   New target `8ddd045c-7f0d-4103-a888-b26455504d33` has an independent VDI,
   3072 MiB RAM and network cable disconnected. Its baseline passed with zero old saves,
   no system loader, zero Players and normal poweroff. The first full attempt failed
   `Get-ScheduledTask` with CIM `0x80070002`; read-only collection found no worker phases
   or native cases. The failure and normal poweroff are retained.
6. The native Task Scheduler COM path then completed both full cold boots on that isolated
   target. Run `run-28e2141c5e4f4170bbf96d4ae640e146` admits the exact same ZIP, uses guest
   script SHA-256 `858c86db4340c09bbb8fe44db344faa173e7056c7de7d4d15923cda2cc9079c3`,
   and passes all seven cases each round, including Zig's 120 native frames / 34 sprites.
   Both rounds report WARP vendor/device `5140/140`, not hardware GPU acceptance.
7. The separate Host launcher then passed from unrelated Unicode working directory with
   system-only PATH and development environment variables cleared. Its inventory remains
   unchanged; no system loader was installed. Baseline zero old saves remains zero old saves
   with eight new scoped saves. The Chinese UI screenshot is readable at the VM's 100% scale.
   Audio output is unavailable in this VM (`audio_open_successes=0`); audibility is not accepted.
   Final isolated state is normal poweroff, 3072 MiB, network disconnected. A subsequent
   read-only check confirms both older VMs remain saved and original snapshots unchanged.

No force-off, saved-state discard, snapshot restore, host security change or deadline
increase was used. The earlier read-only collection finished with normal poweroff;
the later Recovery Zig diagnostic ended with a preserved saved state and did not satisfy
normal-poweroff acceptance. Its final old-save after-hashes remain uncollected. The successful
two-boot gate uses the separately identified isolated target; it does not relabel either older VM.

The pre-clone read-only infrastructure receipt confirms both older VMs are `saved`, both retain
3072 MiB RAM, and the original VM's snapshot listing matches the pre-refresh baseline.
The isolated target starts with zero old saves; this is not another 23-save preservation run.
The historical 23-save comparison remains attached only to its original accepted package.

`package-byte-equivalence.json` confirms all 25 UI/Zig and 14 Host transport members match
the corresponding full-distribution ZIP members byte-for-byte. Thus all successful VM slices
exercise the delivered package bytes, despite using separate transport ZIPs.

## Executed entrypoints and retained files

On this Windows executor, the ignored local helpers were invoked as:

```powershell
cmd /c build\m7-20261031\Build-Shipping.cmd
cmd /c build\m7-20261031\Test-Shipping.cmd
cmd /c build\m7-20261031\Stage-Packages.cmd
python Tools/TwoD/WindowsVmAcceptance.py --vm 86f280ed-50e8-4926-a761-2ce1659fcc74 --username nexora --password-file <existing-private-password-file> --package-zip build/m7-20261031/stage-01/package.zip --vboxmanage D:/Oracle/VirtualBox/VBoxManage.exe --frontend gui --boot-timeout 600 --guest-timeout 300 --output build/m7-20261031/vm-two-boot-02
python Tools/TwoD/WindowsVmAcceptance.py --vm 8ddd045c-7f0d-4103-a888-b26455504d33 --username nexora --password-file <existing-private-password-file> --package-zip build/m7-20261031/stage-01/package.zip --vboxmanage D:/Oracle/VirtualBox/VBoxManage.exe --frontend gui --boot-timeout 600 --guest-timeout 300 --output build/m7-20261031/isolated/vm-two-boot-02
python build/m7-20261031/isolated/Host-VM.py
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
Source fingerprints distinguish the tested Windows CRLF checkout bytes from canonical Git
LF bytes and record each Git blob ID; the normalization relation is verified. Raw receipts
are preserved with scoped Git attributes, so their manifest hashes also match published blobs.

## Acceptance-tool regression checks

The hidden worker now configures noninteractive preferences before hashing, uses intrinsic
.NET streaming SHA-256, and records immutable started/hash-verified/script-parsed receipts.
These diagnose the boundary before native cases; they do not replace native completion.
The 300-second guest, four-minute task and native process deadlines remain unchanged.
Native COM registration replaces the failing CIM task-name query. It uses create-only tasks,
InteractiveToken / Limited credentials, no stored password and the same `PT4M` execution limit;
registered settings and real task results are retained. Only stopped owned tasks are deleted.

- `python Tests/TwoD/WindowsVmAcceptanceTests.py`: **12 passed** on Windows.
- `python Tests/TwoD/WindowsVmAcceptanceTests.py --guest-tests`: real PowerShell child exit,
  known SHA-256 vector, changed-script rejection, immutable phases, strict UTF-8/NUL and
  concurrent full-pipe regressions **PASS**.
- Actual developer Task Scheduler negative smoke: three worker phases, incomplete fixture
  rejected with both parent/task exit **1**, `PT4M`, logon type **3**, run level **0**;
  separate native query confirms the completed owned task was removed. This is a scheduler
  regression check, not VM or game acceptance.
- At receipt creation, final-head hosted Linux Development/Shipping and cross-platform CI
  was pending. The final exact-head results are recorded in [PR #752](https://github.com/jimlee1972/Nexora/pull/752)
  and Beads `nexora-j4q.7.15`. No Linux validation is claimed from this Windows executor.

Existing PS4 USB, audio and supported 100/150/175% DPI evidence is retained with its existing
scope. This refresh adds no physical device, default audio endpoint recovery, hardware VM,
clean-host or performance acceptance.
