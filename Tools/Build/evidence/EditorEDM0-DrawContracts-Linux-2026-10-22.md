# ED-M0 ImGui draw-command and evidence validation — Linux, 2026-10-22

This supporting change does not accept ED-M0. Physical Linux display, Windows per-monitor DPI,
installed IME and supported-backend visual/glyph evidence remain open.

## Source and scope

The audited baseline is main `3f942f9b47ccdde4d7add52a7e2814852e2398a5`, including merged PR #664.
The accompanying patch fixes two independently delegated goals:

- `nexora-nqn.18`: advertise `RendererHasVtxOffset`, honor reset-state callbacks, and reject
  unsupported application callbacks before either renderer mutates resources or submits a packet.
  Public-RHI rendering returns zero; native rendering returns `InvalidDescriptor`. Callback payloads
  are never invoked. Native flattened packets own their encoder state, so reset markers are no-ops.
- `nexora-nqn.19`: treat cyclic artifact symlinks as invalid evidence on Python versions where
  `Path.resolve()` raises `RuntimeError`; preserve the submitted evidence and return `INCOMPLETE`.

Before the renderer fix, an independent portable host probe generated 68,000 vertices in one
16-bit-index draw list and aborted at ImGui's vertex-limit assertion. Enabling the capability flag
made the same probe succeed with a second command at `VtxOffset=65532`. A callback probe also
confirmed the old silent omission: two draws, zero callback calls, zero validation errors.

The new portable regression constructs actual ImGui draw lists beyond that boundary. A forwarding
trace around the production validation device checks each indexed draw's command-local offsets,
cross-list buffer byte offsets, index bounds, reset bindings and repeated rendering. Mixed geometry
and callback-only packets must reject without device calls, metric changes or callback invocation.
This proves the public-RHI command contract, not native GPU visual acceptance.

## Environment and commands

Debian 13, GCC 14.2.0, CMake 4.4.4, Ninja 1.13.2, Python 3.12.14 and Slang 2026.18.
Development used graphical shell and Slang ON, Zig gameplay OFF, Xvfb/X11 and Mesa lavapipe.
Local dependencies were extracted outside the repository. `CMAKE_PREFIX_PATH`, `LD_LIBRARY_PATH`,
`VK_DRIVER_FILES` and `VK_LAYER_PATH` selected that sysroot/driver. A writable, task-owned
`MESA_SHADER_CACHE_DIR` was supplied because the default home cache directory is not writable.
The test executor allowed the local sockets required by Xvfb; no physical display was attached.

```bash
cmake --preset linux-development
cmake --build --preset linux-development -j 4
ctest --preset linux-development
ctest --preset linux-development --rerun-failed --output-on-failure

cmake --preset linux-shipping
cmake --build --preset linux-shipping -j 2

cmake -S . -B build/editor-imgui-off
ctest --test-dir build/editor-imgui-off -R '^build.editor_target_evidence$' --output-on-failure
```

The separate OFF build reused its existing graphical-OFF cache. Its generated build has no
`NexoraDearImGui` target or populated ImGui source directory. Shipping is the Minimal engine build
with Editor disabled, not a Shipping graphical Editor.

## Actual results

| Check | Result |
| --- | --- |
| Development configure/build | Passed; 551 incremental build steps after refreshing main |
| Shipping configure/build | Passed; 9 incremental steps |
| Focused renderer/evidence CTest | 2/2 passed, 2.82 s |
| Evidence admission regressions | 14/14 passed, including loop rejection and unchanged input |
| Evidence CTest with graphical OFF | 1/1 passed, 1.23 s |
| Complete Development CTest | 363 executed: 362 passed, 1 failed, 0 skipped; 1153.52 s |
| Failed-test rerun, unchanged source | 1/1 passed; 32 Game 2D cycles, 69.91 s |
| Documentation routing regressions | 16/16 passed |

The complete suite failed `editor.linux_native_game_twod` at cycle 2 while waiting for two frozen
Atlases to reach the Game canvas. Its source, actions, deadlines and pixel/data assertions were not
changed by this patch. The subsequent unchanged-source rerun passed all 32 cycles. This is an
intermittent result, not an established cause or fix; the existing `nexora-nqn.15` investigation
retains ownership of native X11 workflow work. Do not describe this as a clean first-pass suite.
Read-only inspection of the failed process log found no cycle-2 2D viewport observation; the popup's
reported 2D-option coordinates changed between adjacent observations before it disappeared.
The fixture does not record the actual click position/time or selection acknowledgement. Atlas
resolution happens after a viewport exists, so this log does not establish an atlas-upload failure
or an input/driver cause, nor does it exclude indirect timing effects from other changes.
The original complete-suite log, LastTest/failed-test records and failed process log were retained
outside the repository. An older acceptance JSON in that fixture's artifact directory was not used
as evidence for the failed run; artifact timestamps and process results distinguish attempts.

SHA-256 checks confirmed all 5,857 recorded source/test/tool/configuration entries remained unchanged
through the complete suite. Both 1x/2x 1,000-operation authoring replays, native surface lifetime,
display acceptance, context lifetime, the large Hierarchy lifecycle and the new renderer regression
passed in that suite.

An earlier socket-restricted invocation was intentionally interrupted after Xvfb could not bind its
local socket. It is retained as an aborted environmental attempt, not part of the complete result
above. The restarted suite actually executed the Vulkan and graphical gates.

Only touched C++ ranges and the new test helper were formatted with clang-format 23.1.3.
Changed Markdown source/links and bilingual pairing, plus `git diff --check`, were reviewed.
Windows/macOS CI and physical-host acceptance are separate from these local results.
