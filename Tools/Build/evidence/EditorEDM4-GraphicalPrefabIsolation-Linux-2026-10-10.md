# ED-M4 graphical prefab isolation — Linux, 2026-10-10

Beads: `nexora-pmb.1.6`. Source builds on isolated owner
`0a0f62881856713cbdc2f7b6b4639ac5156cbe04`, document save and stable asset foundation.

Graphical Linux Development configure/build succeeded (240 steps in the preceding
final source rebuild; the full gate's build required no further work).
**239/239** tests passed, zero skips, **622.38 s**.
Minimal Monolithic Shipping configure/build succeeded (5 steps), with Editor stripped.
The final focused graph/owner/native gate passed **3/3** in **21.65 s**;
native isolation took **20.43 s**. The full gate additionally ran the GUI control test.

```sh
cmake --preset linux-development -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON \
  -DNEXORA_ENABLE_SLANG=ON -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON \
  -DNEXORA_BUILD_SHOWCASE=ON -DNEXORA_BUILD_PROJECT_PLAYER=ON \
  -DNEXORA_ENABLE_PROJECT_PLAYER_NATIVE=ON
cmake --build --preset linux-development -j4
ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping -j4
```

Actual ImGui controls at 1x/2x exercise hierarchy selection, Enter-only name and
position edits, Undo/Redo, save requests, dirty-close cancellation, stale scope
invalidation, caller/close guards and read-only inspection. The native Xvfb Editor
creates a prefab from an actual saved scene, publishes revisions 1–4, exercises
Undo/Redo through saves, creates/saves a variant, and reopens it in a read-only
process. Stable node/field IDs, authored values and unavailable opaque bytes survive;
the original scene and base asset remain unchanged by variant creation/publication.
An actual window-close request while the variant is dirty opens confirmation;
Escape keeps the process/draft and does not publish. Read-only editing/save attempts
preserve every project file. Vulkan validation diagnostics are rejected by the harness.

The native application owns the separate World/document. Frame drawing emits scoped
requests and performs no filesystem IO. Writer/recovery/external-change/Play/focus
and modal guards are rechecked by the host and core. Hiding the window keeps the
session; dirty replacement/close needs an explicit discard choice. Save and Exit
requires an explicit prefab save before retrying. No prefab crash journal, nested
materialization, override/revert/apply/rebase or complete component authoring is
claimed. Public C++ consumers rebuild; stable C ABI/module graph are unchanged.
Full Editor milestones remain **0/8**. Current-head hosted gates remain required.
