# ED-M4 graphical prefab isolation — Linux, 2026-10-10

Beads: `nexora-pmb.1.6` and `nexora-pmb.2.7`. The final integration builds on
accepted Main owner `e89c43e57e5052d24e835bcf9a71c5d28f75f79f`.

Graphical Linux Development configure/build succeeded. **242/242** tests passed,
zero skips, **648.72 s**. The full gate includes native prefab isolation (**20.33 s**),
actual 1x/2x prefab controls (**0.09 s**) and native additive tabs (**51.23 s**).
Three independent cold native additive-tab runs passed in **51.29 / 51.14 / 51.05 s**.
Minimal Monolithic Shipping configure/build succeeded (74 steps), with Editor stripped.
Documentation validation and its 16-test regression suite pass.

The additive-tab harness sends each physical modifier and main-key down/up transition
across rendered frames. This avoids compact chord modifier loss in ImGui's trickled
queue on the busy software-rendered host. It retains one-shot close/Undo, exact source
bytes, owned/reference membership, Save All, restart and read-only assertions; no
production input behavior or test timeout changes. A preceding failed full run is
excluded from this accepted gate.

```sh
cmake --preset linux-development -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON \
  -DNEXORA_ENABLE_CRYPTOGRAPHY=ON -DNEXORA_ENABLE_SLANG=ON -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON \
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

## Latest accepted Main integration

The two owned graphical-isolation/native-tabs commits were replayed onto accepted
Main `4e4c2984d1339cb80af83ff40a6735b6497cf6dd`, including the separately merged
native project-upgrade input correction. No prerequisite/foundation branch was merged.
Graphical Development rebuilt **327 steps**, and full CTest passed **242/242 in
638.43 s**, zero skips. Actual native prefab isolation passed in **20.38 s** and
phased native scene tabs in **50.91 s**. Minimal Monolithic Shipping passed **5 steps**,
with Editor excluded. Existing isolation/source-save/tab ownership contracts and
bilingual supporting roadmap scope remain unchanged. Fresh published-head hosted
checks and clean current Main remain required; complete milestones remain **0/8**.
