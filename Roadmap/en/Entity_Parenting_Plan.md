# Entity Parenting (Transform Hierarchy) — Plan

> Version: v1.1 | Status: **direction approved by the owner (follow Unity/Unreal conventions);
> ✅ phases 1, 2, 3 (data model), and 4 (GPU scene sync) complete; the Editor roadmap now has a
> graphical tree/filter/selection/reparent/reorder foundation, while rename, virtualization, and
> gizmos remain open** | Updated: 2026-10-02 | Relates to:
> `Transform_Rotation_Scale_Plan.md`, `Editor_Roadmap.md` §ED-M2

## 1. Purpose

Unity and Unreal transforms are relative to a parent: moving, rotating, or scaling a parent moves
its whole subtree. Nexora entities have no hierarchy, so every `runtime::Transform` is world-space,
and the Editor's `SceneDocument` keeps a separate parent field that is display-only and does not
affect any position. World/local gizmo modes, pivots, and "attach a sword to a hand" all need a real
hierarchy. The owner asked to follow Unity/Unreal so users can transfer their habits.

## 2. Verified current state

- `runtime::Entity` has no parent. `SceneDocument` (Editor) stores `{id, parent, name}` nodes whose
  ids are entity ids; `Reparent` rejects cycles but only edits that metadata, and is not undoable.
- Editor scene files (`NEXORA_EDITOR_SCENE 1`) write `node <id> <parent> <name>` lines plus a runtime
  snapshot. Their transforms were authored as world positions, because nothing applied the parent.
- Destroying an entity removes only that entity. `GameWorld` releases audio/physics/character
  bindings for exactly the ids it destroyed.
- Rendering and physics bodies do not read entity transforms today. The character controller reads
  and writes the entity position. The Zig/C wire carries a position.

## 3. Decisions (Unity conventions)

- **`Transform` is local** (relative to the parent); a root's local equals its world. The world
  transform is computed on demand by walking the parent chain.
- **Parent and child must be in the same scene**; cycles and self-parenting are rejected.
- **`SetParent(entity, parent, keep_world = true)`** mirrors Unity's `SetParent(parent,
  worldPositionStays)`: by default the entity keeps its world pose and its local transform is
  recomputed; with `keep_world = false` the local transform is kept and the entity moves with its new
  parent.
- **Destroying an entity destroys its descendants** (Unity `Destroy` on a GameObject). Bindings of
  every destroyed entity are released, and Editor undo restores the whole subtree.
- **Non-uniform scale under a rotated child produces shear**, which a translation/rotation/scale
  transform cannot represent. Like Unity's `lossyScale`, `WorldTransform` returns the nearest TRS
  (component-wise scale), and `WorldMatrix` returns the exact 4x4 affine matrix. The same limitation
  applies to `keep_world` reparenting under such parents.
- **Snapshots become `NEXORA_SCENE 3`** with the parent id after the entity id; versions 1 and 2 load
  with every entity as a root.
- **Editor uses the runtime hierarchy as the single source of truth.** Old Editor scene files are
  migrated by applying their display-only parents with `keep_world = true`, so nothing moves.

## 4. Phases

1. ✅ **Runtime core and Editor unification.** Parent field, `SetParent` with validation,
   `Parent`/`Children`/`WorldTransform`/`WorldMatrix`, cascading destroy with the destroyed ids
   reported, snapshot v3, undoable `SceneEditor::SetParent` and subtree-restoring destroy undo,
   `GameWorld` wrappers, and `SceneDocument` reading parents from the runtime. Character-controlled
   entities had to be roots in this phase; phase 2 lifted that (see §5).
2. ✅ **Gameplay boundary.** Versioned Zig/C wire for parent and world transform (`"Nexora.Parent"`,
   `"Nexora.WorldTransform"`, and `"Nexora.TransformV2"`); character controllers under a parent,
   following Unity: the controller moves in world space, starts each tick from the transform's
   current world position (so a moving parent carries it), and stores the result locally.
3. ✅ **Editor tools (data model; the graphical tools are the Editor roadmap's).** World/local and pivot gizmo modes (math done: `GizmoAxes`, `ApplyGizmo`,
   `GizmoRoots` in `ViewportMath.h`, working on the world TRS like Unity), Hierarchy drag
   reparenting and sibling order (done: `SetSiblingIndex`/`SiblingIndex`, reparent-to-last,
   undoable `SceneEditor::Move`, `SceneDocument::Move`, and `Nodes()` in sibling order).
4. ✅ **Rendering (GPU scene sync; no app draws through it yet).** When renderers consume entity
   transforms, they must use `WorldMatrix`. `RenderSceneSync` (`RenderSync.h`) is that consumer: it
   mirrors the mesh renderers of the active scenes into `renderer::GPUScene` with each entity's exact
   world matrix (shear included) and conservative world bounds (the matrix's spectral norm scales the
   radius), so moving a parent updates every rendered descendant. Matrices are memoized per call, so
   a sync is linear in the entity count; poses that overflow float are kept out. Cameras follow
   the hierarchy too: `CameraView` places the view at the camera's world-matrix position and orients
   it by the world rotation (scale ignored, as in Unity), and `RenderSceneSync::RenderFrame` culls the GPU scene through that camera before
   submitting. Wiring an application's draw loop to it belongs to the renderer and Editor viewport
   work.

## 5. Phase-1 limits (documented, not hidden)

- ~~An entity with a character controller must be a root.~~ Lifted in phase 2: the controller
  converts between its world position and the child's local transform.
- The Zig/C transform wire keeps carrying the **local** position; for roots that is the world
  position, so existing gameplay modules behave exactly as before.
- `PlaySession` apply-back still applies transforms only; parent changes made during play are not
  applied back, and an entity whose parent changed during play is reported as an apply-back conflict
  (its local values belong to another parent, so copying them would move it).
- ~~No sibling ordering beyond scene storage order.~~ Phase 3 added Unity-style sibling indices, kept
  in the scene storage order (so no snapshot format change).

## 6. Risks

| Risk | Mitigation |
| --- | --- |
| Old Editor scenes move after migration | Apply legacy parents with `keep_world = true`; test it |
| Batch commands interact (reparent then destroy) | Validate the batch against a simulated hierarchy before mutating anything |
| Cascade destroy leaks bindings | `GameWorld` releases bindings for every reported destroyed id |
| Deep or corrupt hierarchies in snapshots | Reject unknown parents, self-parents, and cycles at load time |
| A failed legacy migration leaves a half-loaded scene | Rehearse the migration on a scratch world before loading |
| Play apply-back copies local values across a parent change | Report such entities as a conflict |
