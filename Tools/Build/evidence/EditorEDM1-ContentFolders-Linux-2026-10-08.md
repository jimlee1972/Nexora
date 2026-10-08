# ED-M1 Content folder keyboard evidence — Linux, 2026-10-08

## Scope and baseline

Source parent: `86e1c0d5762ae9a8e7ad6363112941daa1a256f1` (PR #425).
This slice adds keyboard folder entry and bounded parent traversal to the existing graphical
Content panel. It does not complete ED-M1 or accessibility acceptance.

## Delivered behavior

- Real Tab input focuses an indexed child folder; Enter activates it without a pointer click.
  Single-click and double-click retain their existing folder behavior.
- Focused Alt+Up returns one breadcrumb per press and stops at the Content root. The Alt route
  is registered before the modifier transition; other modified Up routes are not claimed.
- Read-only inspection retains traversal. Filters, selection and mutation history survive;
  navigation resets scroll and invalidates asset cursor/anchor through the existing scope check.
- Text input, panel focus, application blur, drags, Game capture and blocking modals use the
  existing keyboard ownership gates. Folder iteration uses a frame-owned snapshot; entry is
  deferred until iteration completes.
- Enter on a focused folder suppresses the competing selected-scene Enter route. Returning to
  the parent and clicking the scene row restores ordinary Enter Open with an owning file request.

## Acceptance

`ContentShortcutTests.cpp` sends actual Tab, Enter and Alt+Up events through `ProcessEvents`
at 1x, 2x and with macOS modifier behavior. It verifies folder entry, parent return, Content-root
clamping, unrelated modified Up, read-only history, blur, another panel, search input and a
blocking close modal. Existing 100k asset/range/delete and ownership tests remain enabled.

`SceneFileTests.cpp` tests 1x/2x with a selected scene and a focused child folder: Enter changes
only the folder, emits no scene-file request and preserves the document and selection. After
parent traversal, scene-row click and Enter still emit the correct Open request.

## Validation

- `cmake --preset linux-development` — passed.
- `cmake --build --preset linux-development` — passed.
- `ctest --preset linux-development -R '^editor.(scene_file_input|content_keyboard_shortcuts)$' --output-on-failure`
  — 2/2 passed, 20.97 s; includes folder ownership, folder/scene conflict and existing scene workflows.
- Full `ctest --preset linux-development` — 156/156 passed, 360.56 s, no skips.
  The first full attempt passed 155/156 in 363.62 s: `showcase.linux_vulkan_interaction` failed
  because `courtyard-animated.png` did not change within its five-second comparison deadline.
  The complete gate was repeated without concurrent focused graphical tests; that Showcase test
  passed in 75.08 s. No Showcase code, thresholds or exclusions changed; the cause is unconfirmed.
- Changed-document validation and `git diff --check` — passed.

The cloud Development build enables EditorImGui and the native Vulkan shell; the full gate uses
Xvfb/lavapipe with Vulkan synchronization validation. No module dependency, public Editor host
layout or gameplay ABI changes. Windows/macOS physical hosts and screen-reader bridging were
not validated; ED-M0 through ED-M7 remain unchecked.
