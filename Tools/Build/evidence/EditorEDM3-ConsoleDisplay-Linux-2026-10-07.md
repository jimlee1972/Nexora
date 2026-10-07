# ED Console display controls: Linux evidence

Date: 2026-10-07. Parent: `b3742eddb6dce0d84388c94641ff4253ed6a19ad`.

The Console previously refreshed its owning ingress snapshot every drawn frame. Continued logs
and producer eviction could replace the messages being inspected; there was no display-only
pause or clear action. Pause display now retains one owning snapshot, while admission/eviction
continues unchanged. Text/severity filters operate on that copy. Resume releases its records
and reads current ingress. Clear view hides every sequence present at the click, including
filtered records and live messages received while paused. It preserves original ingress records
and cumulative drops; messages admitted after the clear watermark remain eligible for display.

The copy is bounded by the source's configured record capacity and is replaced on each pause,
not appended to. The source address is retained only as an identity key, never dereferenced
between drawing calls. A different/null ingress resets pause/clear state; callers detach with a
null binding before reusing the same source storage for a new instance. No public SDK or Runtime
Console API/layout changed, and the controls perform no project IO or scene-history mutation.

`editor.console_display` drives the actual ImGui controls with public Window pointer/button
events at 1x and 2x DPI. It freezes two records, writes 50 background-producer records, then
checks that the frozen records still filter correctly after ingress eviction. It checks Clear
while paused, new-message admission, Resume, cumulative drops, source/null rebinding, unavailable
control reset and 32 pause/resume/clear cycles. This is automated UI-contract evidence, not
physical-device, installed-IME or full ED-M3 acceptance.

Environment/isolated tools match
[import admission](EditorEDM1-ImportAdmission-Linux-2026-10-07.md). Final commands:

```bash
cmake --preset linux-development
cmake --build --preset linux-development --parallel 4
ctest --preset linux-development -R 'editor.console_display|editor.imgui_contract|editor.imgui_context_lifetime'
ctest --preset linux-development
```

Configure/build passed with the cached isolated X11/Vulkan sysroot, graphical shell and Slang
2026.18 enabled. Focused tests: **3/3 passed**, 2.51 seconds. Full CTest: **121/121 passed**, none
skipped, 203.46 seconds. Lavapipe and Khronos core/synchronization validation used the same
VK_DRIVER_FILES/VK_LAYER_PATH/VK_INSTANCE_LAYERS/VK_LAYER_ENABLES settings as import admission.
`git diff --check` passed. The new target uses existing EditorImGui dependencies; no module/linkage
boundary changed. Generated output and local setup remain uncommitted.

Source SHA-256:

- `Engine/EditorImGui/src/EditorImGui.cpp`: `88fb7097ba4d7a2cc1b258095b677457e3949ebf8efbfb0244a83d9175aa9028`
- `Engine/EditorImGui/src/EditorImGuiTestAccess.h`: `9f8bf8045c8633b1a6d4c46fc882bb23d90113e65907a74305f1ba90d68b71df`
- `Tests/EditorImGui/ConsoleDisplayTests.cpp`: `58414ec113194fb56fd82c61db5f54f1c26552eaf92278653aaed4d910d64aeb`
- `Tests/EditorImGui/CMakeLists.txt`: `b50566133a3182f5c85ff73aa7bc8f4ae267b89a95162ca5257b996196238ddd`

Broader runtime/build log routing, native debugging and full graphical milestone acceptance
remain open. Hosted target checks are separate evidence and must pass before merging.
