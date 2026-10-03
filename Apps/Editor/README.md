# Nexora Editor

`NexoraEditor` is the standalone process boundary for the Editor Preview. The currently portable
executable opens a versioned project, indexes its `Content` tree, and emits deterministic shell
evidence. `NexoraEditorCore` supplies stable panel IDs and authoring models without introducing a
private presentation path.

```bash
NexoraEditor --project=/path/to/project --report=editor-report.json
```

The default open is read-write and owns the project's single writer lease until process shutdown.
Use `--read-only` for an explicit observer; it validates existing asset identity sidecars and never
upgrades or writes project-owned files. `--recent-projects=PATH` overrides the user-level recent
project store for isolated automation.

This command is a headless workflow/evidence entry point, not the graphical acceptance gate. The
native docking host and visual Scene/Game views must use the public Window and Presentation
contracts when implemented.

Configure with `NEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON` to build the optional
`NexoraEditorImGui` host and its headless draw-data contract test. The option fetches a pinned
Dear ImGui docking release; it remains off by default so the deterministic CLI workflow does not
acquire a graphical dependency.

On a host with a display and native presentation support, launch the shell with:

```bash
NexoraEditor --project=/path/to/project --graphical
NexoraEditor --graphical
```

`--frames=N` supplies a bounded native smoke run for target-host automation. A missing display or
presentation backend is reported as an error rather than silently falling back to the CLI.
When `--project` is omitted, the graphical Project Browser accepts a UTF-8 root and name, supports
create, read-write open, read-only open, and recent-project shortcuts, and keeps actionable
activation errors visible. Its one-shot request is executed by the application on the authoring
thread; a candidate workspace/index/content session replaces the empty state only after every step
succeeds.
The shell tracks the surface's live client extent and DPI scale, rebuilds its bucketed font atlas,
and submits backend-neutral textured/indexed UI draws directly into the acquired native GPU
backbuffer. It also round-trips a versioned project layout and supplies a live `SceneDocument` to
the Hierarchy panel. The same process binds its deterministic `AssetWorkspace` index to a live
Content panel with breadcrumbs, folder navigation, search/type filtering, virtualized UUID-keyed
rows, thumbnail state, selection, typed drag/drop, dependency inspection, and background reimport.
Project-selector activation now indexes content through an application-owned `AssetImportQueue`;
the selector shows bounded progress and can cancel without activating a partial project. Content
reimport uses the same queue, keeps source/settings hashes and dependency context in its staging
result, and publishes only after the authoring thread revalidates the live asset revision.
Cancelled, failed, or stale jobs leave the previous artifact active and expose structured diagnostic
codes in the Content panel.
Rename/move/delete operate through a recoverable project-content filesystem transaction, with
project-local trash and one-step undo. The real project index creates or validates sibling
`<asset>.meta` identity records; filesystem mutations move those records with their source,
preserving UUID and artifact identity across rename, move, undo, and process reopen. It does not
create a validation-device offscreen target or a full-frame CPU RGBA image. The docked Project panel
shows the stable project UUID, schema and upgrade result, access mode, canonical root, and bounded
recent-project list. Schema-1 projects upgrade atomically to schema 2 only while holding the writer
lease; a read-only legacy open reports that an upgrade is required.

Dirty external changes now block automatic reload and open a Content-panel decision dialog. Compare
shows the retained editor/disk hashes and keeps the conflict open; Reload or Keep records the
terminal authoring-thread choice without direct UI filesystem access. Dependency cycles are also
shown in the Content panel and continue to block artifact publication.

The ED-M2 graphical Hierarchy foundation now renders a parent-aware expandable tree, filters by
entity name, supports plain/Ctrl/Shift selection with a retained generation-keyed anchor, clips
visible-row submission, and routes rename, sibling ordering, and drag/drop reparenting through the
generation-safe, undoable `SceneDocument` contracts. The docked Inspector also edits local position, Euler degrees (quaternion storage), and scale for single or multiple
selections through the same generation-safe document boundary. Mixed fields are explicit, one field
edit applies atomically to the entire selection, and invalid transforms roll back without a partial
write. SceneDocument persists authored Euler hints through save/reload and restores them with undo.
The Hierarchy can create root entities and children of the single selected entity, then selects the
new node. Invalid names and stale parents are rejected, while creation participates in scene Undo.
The Hierarchy also offers Copy and Paste buttons and Ctrl+C/Ctrl+V outside text inputs. Copies retain
the source's captured world pose even after the source moves; Paste selects the new root entity,
and one Undo removes a single pasted copy.
Delete selected, or press Delete while the Hierarchy is focused, to remove selected subtrees.
Undo restores the deleted entities and their names; multiple selected roots undo one at a time.
Duplicate or Ctrl+D copies the current selection without replacing an earlier Copy selection.
The central Scene panel also shows a top-down X/Z grid and entity markers at their world positions.
Click a marker to select it, use the wheel to zoom, or drag with the middle button to pan.
Ctrl-click toggles selection, Shift-click selects a visible range, and F or Frame selected centers
the view on the current selection.
Drag a selected marker to move selected objects in X/Z. The move commits when released, Escape
cancels it, and one Undo restores the previous positions.
Scene View, reflected component widgets, the complete graphical save/restart workflow, and the ED-M2 visual exit gate
remain open.

The graphical shell now saves the active scene with Ctrl+S or the Scene panel's Save Scene button
to `.nexora/scenes/Main.scene` under the project root. It reloads that scene when the project
opens and creates a starter root only when no saved scene exists. Read-only projects reject saves;
a corrupt or unreadable saved scene remains untouched and blocks saving until repaired. The Scene
panel reports save and load failures. This is one-scene persistence, not the complete Scene View or
the ED-M2 save/restart acceptance workflow.
The Scene panel also exposes Undo (Ctrl+Z outside text inputs). Undoing entity creation removes
stale node metadata and selection, allowing the resulting scene to save and reload cleanly.

This is an ED-M1 graphical foundation, not ED-M1 acceptance. Physical-display and Windows
fresh-project workflow acceptance remain open.

The Linux virtual-display acceptance fsyncs a seeded workspace journal and its directory, launches
the graphical process, and sends SIGKILL before a recovery choice. It verifies unchanged journal
and committed workspace bytes, then relaunches with the writer lease and exercises keyboard-only
Recover and Discard independently. Recovery runs must exit successfully as well as report native
UI rendering. This covers abrupt termination with an existing journal; crash injection during a
workspace write and physical-display/Windows acceptance remain separate gates.
