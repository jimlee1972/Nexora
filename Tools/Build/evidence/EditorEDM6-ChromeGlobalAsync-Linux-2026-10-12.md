# ED-M6 Chrome global async duration import — Linux, 2026-10-12

Beads: `nexora-62u.2.8`. Original base: `45fe2e4007e5b7a2b3f1fbb73235b5d1c58b624c`.
Integrated base: `280d4175e46feb3d8741d2fc7a0220e2d4b4ae51`, after concurrent upstream
PR538–PR543 merges. The rebase preserved new prefab/Inspector/tool SDK contracts and restored
accepted Chrome/build-artifact contract sections omitted by those upstream documentation merges.
Global importer/test source hashes below are unchanged. Integration gate results are tracked
in PR537 and Beads before merge.

Integration repair Beads: `nexora-nqn.12`. Main 280d duplicated the native tool target/test
block (including fourteen fixtures), causing CMP0002 configure rejection. It also removed
four existing prefab implementation registrations and three accepted test registrations;
their source/API files remained intact. The public ImGui header lost PluginManager.h despite
its owning vectors/optional values requiring those types, producing missing-type errors.
Restore those source/include/test registrations and deduplicate the identical SDK block,
with all fixture modes/assertions/deadlines intact. Preserve all new prefab workflows and
restore accepted module contracts omitted in the upstream documentation merges. No module
dependency or persisted schema changes. Public C++ implementations remain available.

Development configure/build now pass and enumerate 237 tests. The five restored/unique SDK
and prefab tests are present and passed in the full suite. The first integrated full run passed 236/237 in 759.73 s; only the unchanged reflected
Inspector behavior failed, with the upstream driver saving Boolean byte 00 instead of expected 01.
The upstream test had removed accepted rendered-selection/hover/held/released handshakes and
replaced them with sleeps. Restore the exact accepted 45fe driver with all assertions/deadlines
intact; its focused rerun passed 1/1 in 15.01 s. A full 237-test repeat is required before merge,
and final results are recorded in PR537/Beads. Newly integrated prefab source/override controls
also passed in the initial full run. Linux Shipping Monolithic configure/build passed, testing OFF, Editor stripped:

```bash
cmake --preset linux-shipping -DX11_X11_LIB=/usr/lib/x86_64-linux-gnu/libX11.so.6 -DX11_Xext_LIB=/usr/lib/x86_64-linux-gnu/libXext.so.6 -DNEXORA_VULKAN_LIBRARY=/usr/lib/x86_64-linux-gnu/libvulkan.so.1
cmake --build --preset linux-shipping
```

These task-local SDK/runtime overrides match the Development environment and remain external.
The first Shipping configure lacked the Vulkan loader path; supplying the known system runtime
resolved that environment setup without changing product configuration.
Linux Development Modular, GCC 14.2, Dear ImGui enabled, Xvfb with Mesa lavapipe.
This accepts supporting cloud contracts/input, not physical display/GPU calibration or
complete ED-M6. Overall graphical milestone acceptance remains 0/8.

## Behavior and compatibility

Chrome nestable async b/e supports exactly one legacy `id` or one-key `id2.local/global`.
Legacy id and id2.global alias the same global namespace, with ends on any process/thread.
Selection uses the begin process/thread/name. Local IDs remain scoped to the selected process;
unrelated local values remain opaque bounded JSON metadata. Typed numeric/string IDs,
category and scope isolate identities; local/global namespaces remain separate.

All global records across processes participate in LIFO pairing, even nonselected begins,
so a foreign nested scope cannot accidentally close a selected parent. All participating
scopes must close. They share the existing 128-open budget with selected-process local
and selected-thread synchronous scopes. Global records on other processes, previously skipped,
now must validate/pair; malformed/unclosed foreign globals reject the whole import.
Ambiguous mixed id/id2, mixed local/global members, unknown/missing/invalid identities reject.
Existing X/B/E metadata admission remains unchanged. Legacy S/T/F and instant n stay skipped.

Starts retain the external microsecond clock and inclusive durations use milliseconds.
Newest600 retention/drop accounting follows selected begin order. Pending evicted begins
cannot overwrite newer ring occupants. No CPU execution, GPU, RSS, exclusive time or call tree
is inferred. Persisted schemas, stable C/Gameplay ABI and linkage boundaries are unchanged.

## Executed fixtures

`editor.chrome_cpu_trace` covers cross-process ends, legacy/explicit aliases in both directions,
foreign nested names and same-name begin-process selection, local/global collision separation,
independent completions mixed with X/B/E, typed ID/category/scope lanes, decoded Unicode aliases,
zero durations, unrelated local opaque metadata, malformed/unclosed/wrong-name/ID/backward records,
ambiguous identity objects, exact 128-global capacity and shared local/synchronous overflow.
A pending global begin evicted by 605 X intervals preserves all newer durations and six drops.
Real read-only project IO imports a legacy begin and explicit-global cross-process end while
preserving source bytes and prior owning captures. Existing IO/recovery/source policies remain.

`editor.chrome_trace_display` clicks actual controls at 1x and 2x, reads a real project file
with synchronous, X, local async and legacy/global async records, including a different-process,
different-thread end. The owning static view contains 1.25, 0, 0.5 and 0.75 ms intervals.
Existing malformed publication, independent static captures, live Clear, read-only requests,
close modal, project-switch and detached-scope protection remain exercised.

## Validation

Configure/build, direct Core/1x/2x fixtures, touched C++ format checks and documentation routing
16/16 tests passed. Original-base complete Linux gate passed **227/227**, zero failures/skips, in **719.93 s**:

```bash
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development
```

Both Chrome fixtures passed in the complete suite. Four touched C++ files passed clang-format;
five changed Markdown files passed UTF-8/link/bilingual checks. Fresh hosted CI results for
the immutable source head are recorded in the PR and Beads before merge.
Root README is unchanged. Legacy async pairing, other formats, exclusive/self time and physical
calibration remain open; no graphical milestone is declared complete.

## Source identities

- `Engine/Editor/src/ChromeTrace.cpp`: `746ffe37d3c2683395bdccfe79e267b9e098d6be3047358cc4b61e551d32df75`
- `Tests/Editor/ChromeTraceTests.cpp`: `043f2634e41c99c1ac401a132f57415c9e82fcfb40dcf9f6a55d0599664c3828`
- `Tests/EditorImGui/ChromeTraceDisplayTests.cpp`: `d43236f25185a213af9ffce1469d16bbc45e4b9e515ad2be9548357c97100e97`
