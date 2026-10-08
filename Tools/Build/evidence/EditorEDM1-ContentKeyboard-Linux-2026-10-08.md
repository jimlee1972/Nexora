# ED Content keyboard selection/deletion: Linux evidence

Date: 2026-10-08. Source parent: `034e1928` (merged Profiler import and telemetry consent).

Content Ctrl+A (Cmd+A with macOS behavior) now selects the complete current-folder query/type
result, including clipped rows. Empty results clear selection; read-only projects may select.
ContentBrowserModel shares one matching predicate between visible count, visible rows and full
selection, and builds the replacement UUID set in one scan without per-ID Find calls. Selection
changes no revision, asset data or mutation Undo.

Unmodified Delete stages the current owning UUID selection and calls ProjectContentSession's
existing source/sidecar trash transaction after row rendering. One Content Undo restores the
whole batch and prior selection. Invalid duplicate/missing IDs reject the complete model batch;
portable deletion compacts survivors using a UUID set, and filesystem move planning uses a local
UUID index while preserving request order. Existing path validation, rollback and stable-identity
behavior remain unchanged. Scene gestures and Inspector drafts are cancelled before deletion.
A same-frame F2 Rename cancels staged Delete publication rather than deleting the dialog's target.

Keyboard commands require the focused Content panel, native application focus, no active text/item,
no asset drag or held mouse button, and no context/rename/file/recovery/Play-review/close popup.
Game capture also blocks these commands. Workspace write access gates Delete, context Reimport/Delete
and Content Undo. No UI code retains ContentItem pointers across frames; the model and session own
selection/transaction state. Module dependencies and stable C SDK remain unchanged; SelectVisible
is an additive C++ method with unchanged public class layout.

The dedicated `editor.content_keyboard_shortcuts` test covers:

- 100,000 model rows: case-insensitive type/query selection, preserved revision/Undo selection,
  duplicate/missing-batch rejection, 50,000-item deletion and one-step Undo.
- Actual Window key/pointer events at 1x/2x DPI and macOS Cmd behavior: 128 root assets plus a child
  asset, clipped selection, filters/folders/empty results, source/sidecar batch deletion and Undo,
  stable UUIDs, read-only, modifier rejection and unchanged unselected files.
- Search text ownership, other panel focus, native blur, Rename/close/recovery prompts, a real asset
  drag, and same-frame F2/Delete pending-action cancellation.

This is functional scale coverage, not a benchmark or complete 100k-asset graphical acceptance.
Physical-display/platform workflow and complete ED-M1 acceptance remain open.

Commands use `/workspace/.nexora/env.sh`, graphical shell ON, lavapipe and Khronos core/synchronization
validation for the full gate:

```bash
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development -R 'editor.content_keyboard_shortcuts|editor.preview_contract|editor.mesh_reimport|editor.scene_file_input'
ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping
```

Focused tests **4/4 passed**, **19.75 s**, including the final same-frame Rename guard. Development
configure/build and Minimal Monolithic Shipping configure/build passed. The final full gate
passed **153/153**, **351.84 s**, with zero failures or skips. An earlier interrupted run is not
counted as acceptance evidence.
Generated tools, logs and build output remain uncommitted. Both roadmap languages and owning
Editor/EditorImGui/application contracts are synchronized.
