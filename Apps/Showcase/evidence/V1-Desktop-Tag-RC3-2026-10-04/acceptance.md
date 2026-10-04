# V1 desktop release-tag acceptance — 2026-10-04

✅ Test tag `v0.0.0-rc.3`, commit `0e6b29cc6fb7b6b15b8cc33324ead0bcec8ded44`,
completed [Build 37195056712](https://github.com/jimlee1972/Nexora/actions/runs/37195056712)
(16 jobs) and [Release 37195056792](https://github.com/jimlee1972/Nexora/actions/runs/37195056792)
(5 jobs). Both concluded success on attempt 2. The
[draft release](https://github.com/jimlee1972/Nexora/releases/tag/untagged-854ae580fd0d9c539fe7)
has 16 uploaded attachments and remains unpublished.

| Accepted output | Evidence |
| --- | --- |
| Windows DX12 and Vulkan Shipping/Full ZIPs | Both ZIP checksums verified; Vulkan is build/link/package evidence on this hosted runner |
| Linux x64 Shipping/Full ZIP | Checksum verified; isolated native interaction and screenshot acceptance PASS |
| macOS ARM64 Shipping/Full ZIP | Checksum verified; isolated eight-room Metal graph/resize smoke PASS, 96 frames |
| Windows native acceptance archive | DX12 PASS, 14 checksums, 24 screenshots, 1169 graph/copy/present frames; virtual Hyper-V display, no physical/clean-host attestation |
| Platform provenance, native archives and combined checksum manifest | All expected files uploaded; archive provenance names the exact tag/commit |
| Desktop CTest archive from the passing Build | Linux 80/80, macOS 73/73, Windows 72/72; all three logs retained, no skipped tests |

`release-assets.json` records GitHub's uploaded asset names, sizes and SHA-256 digests.
`bundle-verification.json` records downloaded workflow archive hashes, the four verified ZIP
sidecars, and native acceptance reports. `verification.json` records 14 uploaded files whose
digests match the downloaded workflow bytes, the three CTest log hashes and executed/pass counts,
and the combined checksum verification. `RELEASE-SHA256SUMS` was reconstructed from uploaded
asset digests in the workflow's exact ZIP/archive/provenance ordering; its SHA-256 matches the
uploaded manifest's digest. The Windows acceptance tarball's upload digest and workflow composition
are recorded; its individual reports/screenshots were inspected in the Windows workflow bundle.

The first tag Build attempt failed in the Linux Shipping native window startup before rendering.
The independently executing Release Linux job passed on the same commit. The single failed Build
job passed on a targeted rerun without a source change; the failed release Build gate and dependent
draft job then passed on rerun. `source-provenance.json` preserves these job/attempt identities and
the original failure artifact. This accepts the resulting workflow run while retaining the startup
failure; it does not claim that its root cause was fixed.

This verifies the expanded tag workflow and hosted desktop distribution. It does not certify a
physical or independently provisioned clean Mac, a full Mac keyboard/mouse/screenshot/tour run,
additional GPU/driver coverage, or publication of the draft. Native media/WebView retain their
explicit contract-only/unavailable scope. Full V1 acceptance remains open for those target-host
gates; this Linux workspace has no access to a physical Mac or its operator attestations.
