# ED-M2 Hierarchy keyboard evidence — Linux, 2026-10-08

## Scope and baseline

Source parent: `1bfc7f46636c792dd0d2441e49ed17621f116757` (PR #426).
This slice connects keyboard navigation to the real SceneDocument selection and existing
Hierarchy expansion model. ED-M2 and full accessibility acceptance remain open.

## Delivered behavior

- Focused Up/Down and Home/End navigate all visible tree/filter rows, including clipped rows.
  Held keys repeat; endpoints clamp. Shift extends/shrinks an inclusive anchored interval;
  pointer Shift selection shares the anchor and moves the owning keyboard endpoint.
- Plain Right expands a closed parent, then enters its first child. Left collapses an expanded
  parent, otherwise selects its visible parent. These tree actions are inactive for flat filter
  results. Collapsed descendants are excluded from vertical navigation.
- Read-only projects retain selection and expansion, preserving World serialization, dirty state
  and Undo/Redo. No new persisted project file or module dependency is introduced.
- Application/panel focus, text input, Game capture, scene gestures, held pointer, drag payloads,
  Rename and blocking modals gate keyboard ownership. Plain and Shift vertical routes register
  together before modifier transitions. Ctrl/Alt/Super variants are not claimed.
- State retains owning generation-keyed cursor/anchor values and filter scope, never node/name
  borrows. Reload/detach/filter boundaries clear stale state; every operation rechecks visibility
  and current selection. Selection membership uses a call-local ID set to avoid scanning a large
  hidden selection once per fallback row. Expansion alone rebuilds the same-frame row snapshot.
- The clipper explicitly includes a chosen endpoint and scrolls it into view. Rename receives
  the selected endpoint; abandoned Inspector drafts cancel before keyboard selection publication.

## Acceptance

`HierarchyNavigationTests.cpp` sends real keys/pointer events through `ProcessEvents` at 1x,
2x and with macOS modifier behavior. Its 128-root fixture includes nested children and clipped
rows. Tests cover initial/clamped navigation, held repeat, Shift shrink/reversal, pointer/keyboard
range handoff, nested expand/enter/collapse/parent traversal, read-only, flat/empty filters,
Rename ownership, retained Redo and unchanged World, generation replacement and detach/reattach.
Separate gates cover application blur, another panel, active Inspector input, recovery and close;
a real Hierarchy drag rejects Home and Shift End while retaining selection and document bytes.
A native Scene release and Hierarchy Home share an input frame: the drag request and original
selection survive; after the frame-local request expires, Home navigation resumes. Existing
Hierarchy Select All, keyboard Rename lifecycle and native Scene tool suites also pass.

## Validation

- `cmake --preset linux-development` — passed.
- `cmake --build --preset linux-development` — passed.
- `ctest --preset linux-development -R '^editor.(hierarchy_keyboard_navigation|hierarchy_select_all|keyboard_rename_lifecycle|native_scene_tools)$' --output-on-failure`
  — 4/4 passed, 1.90 s.
- Full `ctest --preset linux-development` — 157/157 passed, 363.17 s, no failures or skips.
- Changed-document validator and `git diff --check` — passed.

The Development build includes EditorImGui and the native Vulkan shell. The full gate runs
Xvfb/lavapipe with Vulkan synchronization validation. No public Editor host layout or gameplay
ABI changes. Physical Windows/macOS hosts, screen-reader bridging and full ED-M0 through ED-M7
acceptance are not claimed.
