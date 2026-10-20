# Hosted Vulkan sanitizer attribution diagnostics — 2026-10-20

Source baseline: `5cb842ac9f4dfee4c3c8f4467ac17e476368b319` (main).
Scope: failure-only build tooling for `nexora-0ua.1.17`, which blocks the independent
[Content folder clipping PR656](https://github.com/jimlee1972/Nexora/pull/656).
The root cause remains unresolved. This does not accept ED-M7 or any physical milestone.

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

## Local validation

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

## Remaining acceptance

Observe a real failing hosted runner's inventory, native diagnostic and mapping/unload sequence;
attribute allocations before proposing a correction. Preserve original failures and coverage.
Any actual fix and PR delivery still require all applicable checks to succeed at the published
head. This diagnostic source and local pass are not proof of a root fix or merge eligibility.
