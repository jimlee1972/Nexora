# Windows VM recovery and Player lifecycle (2026-10-17)

Task: `nexora-j4q.7.6`. This continues the
[PR #623 host/partial VM evidence](../Windows-MSVC-PR620-2026-10-17/acceptance.md), without
rewriting that run's failed/saved-state result. The Player binaries remain frozen at source
`63bb5f7a49e5ec9df0b285494e79ee046c61a6f7` (MSVC 19.44.35228 x64, Monolithic Shipping).
This change repairs the acceptance tooling, not the engine/rendering binaries.

✅ **The full recovery clone passed two cold-boot cycles (14/14 process cases), including two
observed normal poweroffs.** The original VM remains saved with an unresolved Windows login
stall. This accepts the recovered VM fixture/transport slice, not the original guest's login,
a pristine host or complete M6/M7.

## Findings and repair

- The old guest UI inventory really contains **806 NUL bytes**, not incorrectly decoded JSON.
  [Direct byte/hash probe](damaged-inventory.json) agrees with the independently copied file.
  The original ZIP and Player EXE still match their prior hashes. The damaged inventory was
  preserved; stripping NUL or accepting a partial inventory would conceal actual damage.
- Windows PowerShell can interpret native stderr CLIXML progress as error records even when
  VBoxManage exits zero. The host now captures separate raw byte streams and fails on real
  nonzero exits/timeouts. A regression emits malformed CLIXML and non-UTF-8 bytes, then separately
  verifies a nonzero exit cannot be turned into success by a PASS prefix.
- [Guest session probe](desktop-probe.json) shows Guest Control in **Session 0**, while Explorer
  and DWM belong to Session 1. Both [headless](rejected-session0-headless.json) and
  [GUI](rejected-session0-gui.json) early runs rejected `ui-first` with `surface acquire`, and
  cleaned up to normal poweroff. Changing the frontend alone did not fix native presentation.
  The corrected runner waits for desktop level 3, then uses a unique InteractiveToken task in
  the existing login and requires its actual exit code plus a positive desktop-session report.
  The [VirtualBox API definition](https://github.com/VirtualBox/virtualbox/blob/master/src/VBox/Main/idl/VirtualBox.xidl)
  distinguishes userland level 2 from desktop level 3. No login policy or stored credentials change.
- The original 6144 MiB guest exceeded available host headroom (about 4.9 GiB before resume),
  leaving only about 0.5 GiB available. After a normal guest shutdown, the existing VM was changed
  offline to **3072 MiB**, preserving 2 vCPU, VBoxSVGA, 3D off and audio off. The new tool checks
  configured VM RAM plus a 1024 MiB host reserve before every boot. A first successful native
  cycle exposed that poweroff precedes RAM reclamation; a bounded 60-second availability wait
  now handles that delay without lowering the reserve or retrying uncertain guest work.
- A repeat exposed a close-test race: a window handle plus a fixed five-second delay can precede
  DX12 shader initialization, producing exit 0 with zero native frames. That run correctly failed.
  The corrected close case waits for two pixels of the independent fixture's known blue panel
  in the owned window's desktop coordinates before `CloseMainWindow`, then still requires an
  actual native-frame report, exit 0 and balanced allocations. No fixed sleep proves presentation.
- The next first cycle passed all seven cases, including observed panel pixels and normal
  poweroff, but its second boot exceeded the five-minute desktop deadline. ACPI cleanup also
  timed out. A follow-up live probe showed `running`, and a screenshot showed Windows Welcome.
  This exposed a reporting defect: an earlier cycle's `poweroff` had survived in the FAILED
  summary. The tool now clears that state before each boot and reports `unknown` on cleanup
  failure, with the last observed state separately. A regression requires this fail-closed result.
  [Raw FAILED summary](failed-original-second-boot.json) intentionally retains that old reporting
  bug: its `final_vm_state: poweroff` is **not trustworthy**. The subsequent live state was running,
  with this [Welcome screenshot](original-welcome-stall.png); no aggregate PASS was published.
- A further ten-minute observation still found only userland level 2. Ctrl-Alt-Delete/Escape and
  a successful Save State/resume did not restore the desktop. The original guest was left saved.
  A **full current-state clone**, `86f280ed-50e8-4926-a761-2ce1659fcc74`, was created for diagnosis;
  only that new clone's duplicated RAM state was discarded for a fresh cold boot. The original
  VM `71571b18-806d-425b-ad19-712c9043663d`, its saved RAM, disks, snapshots and old saves remain intact.
  This is not a pristine baseline or a claim that the original Windows login was repaired.
- One OS lock per VM UUID prevents cooperating acceptance processes from overlapping. Each run
  gets a fresh GUID root, copied/hash-verified script and original ZIP; no shared script is
  overwritten. Strict UTF-8/NUL checks, per-file flush and inventory SHA-256 verification precede
  execution. Both child pipes drain asynchronously, and reports publish through flushed temporary
  files without replacement. Negative cases mutate only fresh copies. Existing/default saves,
  old staging, snapshots and the prior damaged inventory remain untouched.

The exact hypervisor-level cause of the old abort cannot be recovered from these observations.
Resource pressure, stream handling, shared staging and desktop readiness were verified workflow
defects/risks and are addressed explicitly; this is not a claim to have patched VirtualBox itself.

## Scope

Existing Windows 10 19045 VM, VirtualBox/Guest Additions 7.2.20, 2 vCPU, 3072 MiB, VBoxSVGA,
3D disabled and audio disabled. The previously installed VC++ Runtime remains; no snapshot was
restored or original saved state discarded. The original saved guest was initially resumed and
shut down normally, then later saved again after its second-boot login stalled.
The tool does not install developer tools, enable auto-login, change graphics/audio, disable
host Hyper-V/security features, force poweroff or delete old saves. Guest PATH is restricted to
Windows system locations. DX12 here is a VM/software-rendering correctness slice.

ZIP SHA-256: `d97b3d99e746da33c5703935a85a44552cd6af4c39366f00c1c0a957e3fc9778`.
Player SHA-256: `054d50b1343402c09adfc50f800cd3157596986d66a5a238888ca1f3c0559912`.
The ZIP contains the two original `UI-Shipping` and `ZigGame-Shipping-Fixture` inventories and
their closed set of hashed files, including matching CRT files.

## Reproduction and validation

From the repo root, supply an existing VM/login and an external password file:

```powershell
python Tools/TwoD/WindowsVmAcceptance.py --vm 86f280ed-50e8-4926-a761-2ce1659fcc74 --username <guest-user> --password-file <external-password-file> --package-zip build/windows-pr620/package.zip --vboxmanage <VBoxManage.exe> --boot-timeout 600 --output <fresh-output>
python Tests/TwoD/WindowsVmAcceptanceTests.py
python Tests/TwoD/WindowsVmAcceptanceTests.py --guest-tests
ctest --test-dir build/windows-development -C Development -R 'twod.windows_vm_' --output-on-failure
```

Local regressions: 12 Python tests pass; the PowerShell 5 guest regression passes strict
UTF-8/NUL handling, immutable report publication and a real child writing 200,000 characters
to **each** pipe before exit 7. Registered Windows Development CTest: **2/2 pass**.
The Shipping preset has testing disabled and is not counted as a CTest pass. The initial existing
Development cache needed Vulkan disabled because no local Vulkan SDK library was available;
reconfigure with `-DNEXORA_ENABLE_2D=ON -DNEXORA_ENABLE_VULKAN_BACKEND=OFF` succeeded.
No full local Windows Development suite or Linux preset is claimed on this Windows host.

## Accepted recovery-clone result

[Aggregate raw report](vm-acceptance.json): `run-0af43bc34e6048b791d969c2538d2012`, host process
exit **0**, status **PASS**, final VM state **poweroff**. Both cycles use desktop Session 1 and
the same immutable package and durable primary save. Guest script SHA-256 is
`021e14c72ec0e47b2d7bb23dc9f03e0c77d385878c23f97e29f6ed6ed30fbf0d`.
Available host RAM before each boot was **6372 / 6900 MiB**, above the unchanged 4096 MiB threshold.
The configured boot/state deadline was 600 seconds; no timeout was waived or guest work retried.

| Case | Cycle 1 | Cycle 2 |
| --- | --- | --- |
| UI first/restart | exits 0/0, visits 0 then 1 | exits 0/0, visits 2 then 3 |
| Primary save across cold boot | final visits 2 | final visits 4 |
| Corrupt save | exit 1, corrupt bytes preserved | exit 1, corrupt bytes preserved |
| Unicode save directory | exit 0, 3 native frames | exit 0, 3 native frames |
| Zig native | exit 0, 120 frames, 34 visible | exit 0, 120 frames, 34 visible |
| Corrupt Cooked | exit 1, expected hash/type/identity error | exit 1, same expected error |
| Normal window close | panel pixels observed, exit 0, 6 native frames | panel pixels observed, exit 0, 6 native frames |
| Teardown / VM | allocations balanced, 0 Player, normal poweroff | allocations balanced, 0 Player, normal poweroff |

The recovery VM is `Nexora-J4Q-7-6-Recovery-20261017`, a **full current-state clone** rather than
a baseline snapshot restore or new OS installation. It retains the original guest disk contents
and prerequisites; only the clone's duplicated RAM state was discarded before this cold-boot run.
It is left normally powered off and is the reproducible acceptance target for subsequent runs.
The original VM `Nexora-ZS-M5-CleanMachine-Win10` remains **saved**, retaining its failed-login
execution state for separate diagnosis. Its login/hypervisor root cause is still unresolved.
That preserved-original follow-up is **nexora-j4q.7.7**; it is not closed by this clone result.
No original save, snapshot, disk or saved RAM state was discarded, and no VM was forcibly powered off.

Full hosted Linux configure/build/CTest and the applicable CI matrix must pass on the PR head
before merge; their results belong to the PR checks, not to this local Windows observation.

Physical controller/input, audible device output/removal/recovery, formal CJK typography and
100/150/200%/multi-monitor DPI, pristine physical-PC startup, integrated game schema/migration/UX,
and sustained minimum-hardware performance remain open. M6/M7 remain **NOT ACCEPTED**;
progress remains **9/11 (82%), P0 6/8**. This frozen binary run does not validate later engine changes.
