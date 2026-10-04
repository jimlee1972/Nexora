# V1 Windows Vulkan hosted software-driver acceptance — 2026-10-04

✅ Shipping/Full native Win32 Vulkan acceptance passes with checksum-verified Mesa lavapipe 26.2.4.
The exact driver DLL loaded by the process matches the recorded SHA-256. This extends Vulkan coverage
beyond the existing GTX 960 and Linux lavapipe records; it remains software-driver coverage, with
physical-display and clean-host attestations false.

| Gate | Result |
| --- | --- |
| Isolated package checksums | 14/14 |
| Native screenshots / interaction checks | 25 screenshots / 9 checks |
| Complete guided tour | 210.006 seconds, all seven steps |
| Native graph/copy/present execution | 41,737 frames; requested Vulkan, no backend fallback, zero CPU-composed frames |
| Driver identity | Mesa 26.2.4; selected `vulkan_lvp.dll` loaded from the expected path; archive and DLL hashes retained |
| ICD cleanup | Temporary hosted-runner registry entry removed after acceptance; original job log retained |

Source `bff62773d593b95de918f670795b8076dc89224c`, test tag `v0.0.0-rc.4`:
[Build 37208176403](https://github.com/jimlee1972/Nexora/actions/runs/37208176403) passes all 16 jobs,
[Release 37208176483](https://github.com/jimlee1972/Nexora/actions/runs/37208176483) passes all five jobs,
both on attempt 1. The [unpublished draft](https://github.com/jimlee1972/Nexora/releases/tag/untagged-a02a6dd81d23af98f447)
has 17 uploaded assets, including the separate `windows-vulkan-acceptance.tar.gz`.
`verification.json` records the downloaded Windows workflow bundle digest, five asset digests checked
against actual downloaded bytes, both Windows ZIP sidecars, and a reconstructed 12-entry checksum
manifest matching the uploaded manifest's digest. The Vulkan tarball's upload digest/inclusion are
verified through GitHub metadata; individual native files were inspected in the downloaded workflow
bundle. This does not claim direct byte verification of every release asset.

The PR's hosted acceptance also passes on merge source `ec5e758db6d56b06490b437b50f90c077efa17b8`
(38,851 frames, 210.002 seconds). Initial runs exposed `vkCreateInstance` result -9: the hosted
elevated loader ignored environment ICD overrides and used registry discovery. The fix registers only
the pinned ICD on disposable GitHub-hosted runners and removes exactly that entry in an always-run
cleanup. Local/self-hosted setup is rejected; driver bytes stay outside the package.

A separate branch-push Windows CTest encountered `editor.mesh_reimport` SegFault after 5.91 seconds;
its targeted retry and the tag/PR desktop suites passed. A follow-up fixture fix orders the borrowed
queue before the content session and polls cooperatively without increasing five-second deadlines.
A Linux fault-injection probe reproduces SIGSEGV with the old fixture and returns the expected
`injected waiter timeout` failure with the corrected fixture; `mesh-reimport-unwind.json` records
both exits and fixture identity. The full Linux gate remains 80/80 without skips.

**Mac remains incomplete and is deferred at the user's request.** Hosted Mac CI/package smoke is
separate from physical/clean-host operation, full interactive screenshots and the guided tour.
Additional physical GPU/driver coverage remains open; V1 final acceptance is not declared complete.
