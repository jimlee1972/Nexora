# Hosted Vulkan sanitizer attribution and ICD ordering — 2026-10-20

Source baseline: `5cb842ac9f4dfee4c3c8f4467ac17e476368b319` (main).
Initial scope: failure-only build tooling for `nexora-0ua.1.17`, which blocks the independent
[Content folder clipping PR656](https://github.com/jimlee1972/Nexora/pull/656).
The later same-process attribution and CI mitigation are recorded below. Mesa unload cleanup
remains open. This does not accept ED-M7 or any physical milestone.

## Retained failure and control

At PR656 head `d3ae6752de5b2de7ce863305352257a97570d26d`, source workflow
[39361834486](https://github.com/jimlee1972/Nexora/actions/runs/39361834486) failed the mandatory
ASan/UBSan gate. Original job118429712868 and its sole bounded retry118440591024 both used
Ubuntu image `20261018.339.1`. Scene/PBR/upload-reuse/swapchain reported 240/240/608/2304-byte
LSan leaks through `vkEnumeratePhysicalDevices`; this preset excludes the changed ImGui module.
The retry registered204 tests:193 passed,7 existing backend skips,4 failures (209.65s).
TSan was skipped after ASan failure. Effective checks36/38 passed; no merge was requested.
No further retry of that unchanged source is planned.

Identical-head PR workflow
[39361837886](https://github.com/jimlee1972/Nexora/actions/runs/39361837886), contract118430002221,
passed on image `20261012.334.1`: all four native cases actually passed; ASan197 passed/7 backend
skips/0 failures (360.49s), TSan193/193/0 skips (393.51s). This correlation does not establish
allocation ownership or prove a runner-image cause.

Earlier outside-repository controls matched failed libc BuildID
`a4a7992a8e66555c8141ab2a08a8465ff6e0ea65` and Vulkan-loader BuildID
`32e77fd13d177aaa6ceeba4862c3407d9246e422`. A deliberate128-byte leak is detected, but isolated
real Xlib Vulkan controls and four current-source native programs pass (4/4,0 skips,26.57s).
Compiler/kernel/X11/hardware/ICD-discovery differences remain. The isolated Mesa sysroot is not
a captured hosted driver search environment. These controls cannot replace required CI.

## Diagnostic contract

The original `ctest --preset linux-sanitizers` command and failure outcome are unchanged.
Only after its failure, `DiagnoseVulkanSanitizers.py` copies the original `LastTest.log`, records
public system ICD/layer identity fields and a fixed graphics/runtime package-version list, then
invokes the existing native scene helper once in a separate evidence directory.

Only the native child receives `LD_DEBUG=files`, a separate `LD_DEBUG_OUTPUT` path and
`VK_LOADER_DEBUG=error,warn,driver,layer`. Mapping bases and unload records can associate leaked
PCs with a driver even after `dlclose`. The existing helper/native30-second deadline is unchanged;
the diagnostic parent is bounded to45 seconds and its workflow step to2 minutes. The native
return code is preserved; a successful extra invocation cannot replace the failed original gate.

There is no ICD/layer selection, sanitizer-option change, suppression, whole environment dump,
credential capture or new dependency. Full diagnostic files are retained for7 days. Public
inventory, the bounded native output and mapping/unload lines also appear in the job log, so
attribution does not depend solely on artifact download. The full original suite log is retained
in the artifact rather than reprinted. Passing original gates do not run this collector.

## Diagnostics-only baseline validation

- `cmake --preset linux-development`: passed.
- `CMAKE_BUILD_PARALLEL_LEVEL=4 cmake --build --preset linux-development`: passed.
- `CTEST_PARALLEL_LEVEL=4 ctest --preset linux-development --output-on-failure`:
  285 registered,273 passed,12 native environment skips,0 failures (362.93s).
  100k lifecycle257.15s under unchanged300s deadline; deep hierarchy105.77s under unchanged180s.
- `python3 Tools/Build/tests/test_vulkan_sanitizer_diagnostics.py -v`:3/3 passed. Real child
  processes check quoted paths with spaces, native-only tracing, existing driver/ASan environment
  conservation, private-sentinel absence from output/artifacts, original failure-log conservation,
  successful-probe nonreplacement and native failure/unavailable exit-code propagation.
- Existing documentation routing tests:16/16 passed. Workflow parses with PyYAML; the original
  mandatory sanitizer command has no `continue-on-error`. `git diff --check` passed.
- An actual native Vulkan scene run through the collector and existing isolated Ubuntu-runtime
  wrapper returned0, retained its pixel capture and passed the original native helper. One
  50,251-byte loader trace contains Vulkan loader, Mesa lavapipe and mapping bases. This verifies
  collection mechanics locally; it does not reproduce or remediate hosted leaks. The final
  additional job-log projection is covered by the process tests; the full trace is unchanged.

Local Development is Modular with graphical Editor/2D enabled and Slang disabled. No public API,
linkage boundary, native test/helper, preset, original deadline or production source changed.
Shipping is not rerun for this tooling-only slice. Windows/macOS acceptance is not claimed locally.
Root README and both Editor milestone acceptance counts remain unchanged at0/8.

## Hosted same-process attribution

Diagnostic PR659 source `d1c75a42791baf357ea6d86b894f2ec62f2568d6` retains36/38 effective
successful checks. Source run39376772533/contract118476716440 passes on image20261012.334.1:
all four native cases actually pass, ASan197 passed/7 existing skips (375.82s), TSan193/193,
zero skips (392.69s). PR run39376811715/contract118476900818 fails on image20261018.339.1:
ASan193 passed/7 skips/4 failures (369.93s), again240/240/608/2304 bytes. Its additional native
scene probe reproduces240 bytes and exits1; the diagnostic artifact uploads successfully.
The original failure and dependent summary118489814036 remain retained. No retry was requested.

The failing probe uses `libvulkan_lvp.so` with llvmpipe/LLVM20.1.2. Its historical mapping base
`0x7f45d0400000`, size`0xb8c100`, associates the unknown/mislabelled frames with lavapipe:
direct realloc PC at`+0x150de3`, and two56-byte calloc PCs at`+0x45048d`/`+0x4504b1`.
Those exact call sites match disassembly of the SHA-verified official Ubuntu Mesa25.2.8 package.
The apparent `libubsan` attribution is stale after lavapipe unload; it is not the allocation owner.
Diagnostic-section SHA256:
`a962bb7f606fa353c9716f39892be39da8ae897ff5b6d4f44a72d9a93e342400`.

The failed discovery order is virtio,gfxstream,radeon,asahi,lavapipe,nouveau,intel,intel_hasvk.
It destroys the lavapipe link map. Local lavapipe-first traces instead activate NODELETE for
lavapipe. Both Asahi and lavapipe export GNU UNIQUE SPIR-V symbols, including
`spvTargetEnvNameMap`, so the first owner changes module retention. Same complete-set order
controls reproduce the retention difference without replacing any driver or changing ASan.

## Controlled order regression and mitigation

Fresh Ubuntu GCC13.3 native binaries, matched Ubuntu libc/loader/sanitizer/Mesa runtimes, and the
same eight descriptors give these results on the local Intel Xeon8573C:

| Complete ICD order | Scene | PBR | Upload reuse | Swapchain | Total |
| --- | --- | --- | --- | --- | --- |
| Failing runner order |112-byte leak|112-byte leak|224-byte leak|pass|1/4 pass,0 skips,42.35s|
| Same eight, lavapipe first |pass|pass|pass|pass|4/4 pass,0 skips,29.54s|

All three retained PPM captures are byte-identical across those orders:

- Scene:`ac98537bee226d4a7a632ea38f217aa6112e415518a98e925864677a65434d8e`.
- PBR:`76a3c4d7fdc7da2a525d9ce3580360331ad2c639cbc13f8bde6c3a9c782fe435`.
- Upload:`22ef58c7fb687f0285fe202038360b834daa6c083fbf54dd70383f7667a61881`.

Red-control log SHA256:`366eebce89368fd723ab82b5d1a6ccbb54e6e5b1cb947cbb118bb4b67661e6dd`.
Green-control log SHA256:`1c23b6941f8a3eb30166b6c5b4dbc0b09269024cab4b3906a9f3e507ed3bdd59`.
The local Intel host does not cover Mesa's AMD-Zen L3-affinity allocation path. The verified
Mesa25.2.8 source has its128-byte-mask realloc/global assignment in `src/util/u_cpu_detect.c`;
source archive SHA256:`097842f3e49d996868b38688db87b006f7d4541e93ce86d2f341d8b3e7be7c93`.
This offers a hardware-specific explanation for the additional hosted128-byte allocations,
without claiming that all internal allocation sites are symbolized or repaired.

`PrepareVulkanICDOrder.py` now deterministically retains the complete normal Linux/XDG manifest
population, lavapipe first and all others sorted by path. Existing selectors/filters, malformed
identities and ambiguous lavapipe descriptors fail closed. Only ASan CTest and its failure
collector receive that complete list; the existing native deadlines, sanitizer options and
default desktop/Xvfb/Shipping discovery remain intact. This stabilizes the process-retained
driver configuration. It does not repair Mesa unload cleanup or establish default-order
driver-unload memory cleanliness. Parent `nexora-0ua.1.17` remains open; CI mitigation child
`nexora-0ua.1.17.1` records the narrower acceptance.

## Real native leak-detection positive control

`NEXORA_BUILD_VULKAN_SANITIZER_CONTROL=ON` adds a separate Linux-native executable from the same
scene source. Its private define requires ASan and deliberately loses17 bytes after all29 scene
submissions, pixel/error checks and ordered teardown. The four original targets keep their
assertions. The verifier requires a completed scene/capture, nonzero native exit and exactly one
17-byte leak attributed to `InjectVulkanSanitizerLeak`; disabled detection, unrelated faults,
additional errors and unavailable native execution fail rather than skip.

Initial explicit-Ubuntu-loader validation detected17 bytes but mislabelled the executable as
the loader, so the strict symbol check rejected it (4/5; not counted as acceptance). With normal
executable loading, Ubuntu GCC13.3 binaries and verified Ubuntu ASan/Mesa libraries on the local
Debian libc host, all five actual gates pass, zero skips,38.32s: scene3.69s, positive control3.24s,
PBR13.25s, upload8.10s, swapchain10.03s. The positive control's native process exits1 with the
exact17-byte leak and correct source symbol; its verifier passes. No checks were relaxed.
Normal-loader log SHA256:`25efc399a9913845952b3a44e573974b8dc307e98851b77ebcb21cf16a84e365`.

The CI requires all four original gates plus this real control to be registered before the full
ASan suite executes. Registration increases by one only in the opt-in ASan configuration;
ordinary Development and Shipping do not include the fault executable. Its JSON, native report
and capture are retained separately from ordinary acceptance evidence.

Final combined local Development configure/build/CTest passes285 registered/273 passed/12
existing native environment skips/0 failures (335.34s); lifecycle245.24s and deep80.35s under
unchanged deadlines. Shipping configure/build passes with testing/Editor disabled. The three
feature source/test digests stay unchanged, including55/2048 observed folder submissions at1x/2x.
Python ordering/control/diagnostic contracts pass10/10; GitHub-output preservation and205-test
ASan registration pass. Source formatting, changed-document validation and documentation
routing16/16 pass. No production Presentation API or source preset changed; the existing native
scene source gains only the private opt-in fault path for its separate executable.

Exact-head hosted results are recorded with PR656.
All applicable published-head checks must succeed before merge. Driver unload cleanup, full
process-memory/production budgets and physical acceptance remain open; milestones stay0/8.
