# ED Content keyboard navigation: Linux evidence

Date: 2026-10-08. Source parent: `98676718` (Content select-all and batch Delete).

Focused Content Up/Down and Home/End navigate the complete sorted current-folder search/type
asset row set, including clipped rows. Held keys repeat and boundaries clamp. Without a visible
selection, Down/Home choose the first row and Up/End choose the last. Empty results preserve the
previous selection. Shift navigation extends or shrinks an inclusive range around the retained
anchor; Shift-click shares the same model operation. The clipper explicitly includes a keyboard
target, and row submission scrolls it into view.

`ContentBrowserModel::SelectVisibleRange` rejects hidden/missing UUID endpoints atomically,
supports reverse intervals and builds one owning replacement selection with linear scans rather
than per-row Find/Select. Assets, content revision and mutation Undo remain unchanged. This is an
additive exported C++ method with unchanged public class layout and no stable C SDK changes.

UI state retains UUIDs and owning project/root/folder/revision values; no item borrow survives a
frame. Plain selection resets the anchor and Ctrl+A clears navigation state. Scope or UI-filter
changes invalidate it, and every operation checks current visible membership to handle external
filter/selection changes. Navigation remains available in read-only projects. Text/item ownership,
another panel, native blur, held mouse/asset drag, Game capture and blocking prompts retain their
existing gates. Ctrl/Alt/Super navigation variants are not claimed by the asset routes. Both plain
and Shift routes are registered before modifier transitions, including the first Shift press.
Rename and Delete consume the resulting selection through their existing ownership/transaction paths.

The extended `editor.content_keyboard_shortcuts` test uses real Window key/pointer/wheel events:

- 1x/2x DPI and macOS input behavior; first/last/no-selection/clamping, held Down repeat,
  Shift extension/shrink/reversal, rejected modifier variants and clipped endpoint reveal.
- Plain mouse selection, wheel scrolling and Shift-click extension/shrink with one retained anchor.
- Read-only navigation, empty results, hidden-filter anchors, folder/content-generation replacement,
  and F2 Rename handoff without asset/history mutation.
- Native blur, other-panel/text ownership, Rename/close/recovery and an actual asset drag.
- 100,000 model rows with a reverse 49,999-row matching range, hidden/missing endpoint rejection,
  preserved mutation Undo, and existing 50,000-item delete/Undo coverage.

This is functional scale coverage, not a timing benchmark, folder-row keyboard traversal,
semantic screen-reader bridge or complete graphical ED-M1/ED-M7 acceptance. Physical Linux,
Windows DPI/IME and macOS host evidence remain separate open gates.

Commands use `/workspace/.nexora/env.sh`. Full Linux tests use graphical shell ON, lavapipe and
Khronos core/synchronization validation, plus the pinned documentation dependencies:

```bash
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development -R 'editor.content_keyboard_shortcuts|editor.scene_file_input|editor.keyboard_rename_lifecycle'
ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping
```

Focused tests **3/3 passed**, **18.99 s**. The final full Linux gate passed **153/153**,
**357.85 s**, with zero failures/skips. Development configure/build and Minimal Monolithic
Shipping configure/build passed. Changed Markdown/bilingual documentation validation and
`git diff --check` passed. Contract docs and both roadmap languages are synchronized. Generated
tools, logs and build output remain uncommitted.
