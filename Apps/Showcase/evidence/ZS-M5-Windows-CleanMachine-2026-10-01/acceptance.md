# ZS-M5 Windows clean-machine acceptance

Result: PASS

This record covers the independently provisioned Windows target machine created from the supplied ISO. The package was copied to the guest package root, all SHA256SUMS entries were verified inside the guest, and the exact command recorded by manifests/build.json was executed from that root.

## Target

- VM: Nexora-ZS-M5-CleanMachine-Win10
- Guest: nexora-zs-m5 / Microsoft Windows 10 version 10.0.19045.3803
- VirtualBox: 7.2.20r175154
- Base snapshot: ZS-M5-Clean-Windows10-Base-NoMedia (9d46e380-d1e3-46d5-8847-fe2bd889d4e8)
- ISO: E:DownloadsWindows10.iso
- ISO SHA-256: c548feffb69bf0e9839fc96706058d8406f276f2822e22f41422840bb68c5cb6

## Package and launch

- Source commit: 631f506463a2765dd84a0423b0811650ef8206e6
- Profile: Development / dynamic gameplay
- Package root: C:NexoraShowcase-Development
- Checksums verified in guest: 16/16
- Exit status: 0
- launch-report status: PASS
- Report build id: 631f506463a2

Exact command from manifests/build.json:

    bin/NexoraShowcase.exe --headless --scene=tour --frames=1 --no-reload --gameplay-module=dynamic --gameplay-library=bin/NexoraZigGameplay.dll --report=launch-report.json

The committed launch-report.json records headless validation PASS, engine/module lifecycle success, scene evidence, and zero validation errors. Windowed native backend execution is explicitly NOT EXECUTED by this record.

The machine-readable details are in acceptance.json; copied build.json, content.json, SHA256SUMS, and launch-report.json are retained beside it.
