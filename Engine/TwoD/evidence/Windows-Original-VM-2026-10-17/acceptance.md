# Original Windows VM recovery verification (2026-10-17)

Task: `nexora-j4q.7.7`. This follows the preserved-original Welcome stall in
[the previous recovery record](../Windows-VM-Recovery-2026-10-17/acceptance.md).
The target is the **original** `Nexora-ZS-M5-CleanMachine-Win10`, UUID
`71571b18-806d-425b-ad19-712c9043663d`, not the recovery clone.

## Recovery and diagnostic limits

The user reported restarting the VM before handing it back. The first read-only check found
`running`, Guest Additions desktop level **3**, Explorer/DWM in Session 1, and Windows boot time
`2026-10-17T06:57:31.5000000Z`. The host log shows `CREATED -> POWERING_ON -> RUNNING`, rather
than loading the previous saved execution state. Both original snapshots were still present.
[Desktop screenshot after the user's restart](desktop-after-user-restart.png) supplements these
process/run-level observations; it does not by itself establish a successful acceptance cycle.

The guest records Kernel-Power event 41 at `06:58:28Z` during this boot. Earlier Winlogon
6005/6006/6004 events refer to TrustedInstaller/CreateSession, including an 89-second delay at
`03:24:41Z`; they precede the failed second boot and do not establish its cause. The recovery
does not prove a particular Windows service or VirtualBox defect was repaired. No profile,
registry/login policy, Guest Additions, VM hardware or host Hyper-V/security setting was changed.
The supported conclusion is recovery after the user's restart, subject to the repeat results below.

The previous stalled-state file `2026-10-17T05-33-47-194120300Z.sav` was already absent at the
first filesystem check after that restart. This run did not discard a saved state, restore/delete
a snapshot, reset or forcibly power off the VM. It cannot claim preservation of that absent RAM
execution image. The older snapshot RAM file and existing on-disk saves are checked separately.

The original VM was shut down with `shutdown.exe /s /t 0`, and `poweroff` was observed before
starting acceptance. Tests use a new GUID directory and new corruption copies. The damaged old
inventory remains preserved, with SHA-256
`f860ea6cfbfe8ddb3862a09c1b443f3273dac1a4757ce9e7a3b34d46f971ff10`.

## Package and reproduction

Player/Zig binaries remain frozen at source `63bb5f7a49e5ec9df0b285494e79ee046c61a6f7`, native
x64 MSVC 19.44.35228 Monolithic Shipping. ZIP SHA-256:
`d97b3d99e746da33c5703935a85a44552cd6af4c39366f00c1c0a957e3fc9778`.
This does not validate engine changes merged after that source revision. The previously installed
VC++ Runtime remains; this is an existing Windows 10 19045 VM, not a pristine machine.
Configuration remains 2 vCPU, 3072 MiB, VBoxSVGA, 3D/audio off, VirtualBox/Guest Additions 7.2.20.

```powershell
python Tools/TwoD/WindowsVmAcceptance.py --vm 71571b18-806d-425b-ad19-712c9043663d --username <guest-user> --password-file <external-password-file> --package-zip build/windows-pr620/package.zip --vboxmanage <VBoxManage.exe> --boot-timeout 600 --output <fresh-output>
python Tests/TwoD/WindowsVmAcceptanceTests.py
python Tests/TwoD/WindowsVmAcceptanceTests.py --guest-tests
ctest --test-dir build/windows-development -C Development -R 'twod.windows_vm_' --output-on-failure
```

Local regressions: **12/12 Python**, PowerShell guest UTF-8/NUL/immutable-publication/dual-pipe
regressions, and **2/2 Windows Development CTest** pass. No engine or acceptance-tool implementation
was changed in this follow-up. Hosted CI results are recorded on the resulting PR; a local Windows
run does not claim execution of Linux presets.

## Repeat acceptance and preservation

✅ [Aggregate raw report](vm-acceptance.json): run `run-f9ed938ac0244e209527ac095b8abfba`,
host exit **0**, status **PASS**, original VM UUID, **two cold boots / 14 process cases** and
two observed normal poweroffs. Both cycles used desktop Session 1; no deadline was waived.
Available host RAM before the boots was **6100 / 6186 MiB**, above the unchanged 4096 MiB gate.
The guest script's exact transferred-byte SHA-256 was
`559b2059e368d8409d5e221c4ca4ce4b918633c8c805e9974dfcc3cf8291e2ab`.

| Case | Cycle 1 | Cycle 2 |
| --- | --- | --- |
| UI first/restart | exit 0/0, visits loaded 0/1 | exit 0/0, visits loaded 2/3 |
| Durable save after cycle | visits 2 | visits 4 |
| Corrupt save | exit 1, damaged copy preserved | exit 1, damaged copy preserved |
| Unicode save path | exit 0, 3 native frames | exit 0, 3 native frames |
| Zig native | exit 0, 120 frames, 34 visible | exit 0, 120 frames, 34 visible |
| Corrupt Cooked | exit 1, hash/type/identity rejection | exit 1, same rejection |
| Normal window close | panel observed, exit 0, 3 native frames | panel observed, exit 0, 3 native frames |
| Teardown / VM | balanced allocations, zero Player, poweroff | balanced allocations, zero Player, poweroff |

✅ [Post-run preservation comparison](preservation.json) after a **third cold boot** to desktop
level 3, a read-only probe and another observed normal shutdown:

- All **16 pre-existing `.nxsav` files** found under the three recorded fixture/default-save
  roots remain present with identical lengths and SHA-256, including the user-scoped default save.
  The new run added seven files under its own GUID root; the post-run total is 23. This is a
  scoped save comparison, not a claim that every Windows filesystem byte is unchanged.
- Both snapshot UUIDs and the current-snapshot selection are unchanged:
  `e973ecf7-f2a7-415c-9753-19316d7d7e03` and `9d46e380-d1e3-46d5-8847-fe2bd889d4e8` (current).
  The complete machine-readable snapshot listing matches before/after.
- The older snapshot RAM file `2026-09-30T17-03-37-400972700Z.sav` remains 683,274,300 bytes,
  with unchanged SHA-256 `d997c4f821f8a2e5c2726671a67ab04feb3a972f99c45bebab3bde2ea83c7c18`.
  This is distinct from the absent failed-execution RAM file described above.
- The old damaged inventory hash is unchanged. The original VM is left **normally powered off**,
  with zero Player processes observed in the final desktop probe. The recovery clone was not run.

Local raw VBoxManage streams, before/after guest probes, command metadata and host logs remain
under ignored `build/welcome-20261017/`; the committed reports contain no credentials. The old
failed reports remain unchanged. This verifies the recovered original VM's bounded lifecycle,
not a permanent fix for an identified Windows/hypervisor root cause.

Physical controller/input, audible device output/recovery, formal CJK/DPI, pristine physical-PC
startup, integrated game schema/migration/UX and minimum-hardware performance remain open.
**M6/M7 NOT ACCEPTED; 9/11 (82%), P0 6/8 remain unchanged.**
