# Transform Rotation and Scale Extension — Plan

> Version: v1.1 | Status: **direction approved by the owner (follow Unity/Unreal conventions);
> phase 1+2 in progress, later phases not started** | Updated: 2026-10-01 | Relates to: `Editor_Roadmap.md` §ED-M2 (gizmos),
> `Engine_API_Foundation_Roadmap.md`

## 1. Purpose and decision needed

`runtime::Transform` carries a position only (`double x, y, z`). The Editor's portable viewport math
(`ViewportMath.h`) can pick and drag along an axis, but rotation gizmos, scale gizmos, world/local
space, pivot rules, and negative-scale handling cannot be specified or tested until an entity can
actually have a rotation and a scale. This plan records what such a change touches so it can be
reviewed as one decision rather than discovered file by file. It is not authorization to start.

**Decision requested:** approve (or amend) the recommended option in §4 and the phases in §6.

## 1a. Decisions (2026-10-01)

The owner chose to follow the Unity and Unreal conventions so that users coming from them can
transfer their habits. Concretely:

- **Option A** below: extend `runtime::Transform` in place (one component, like Unity's `Transform`
  and Unreal's `USceneComponent` relative transform), with per-axis, possibly non-uniform scale.
- **Rotation is stored as a quaternion**; Euler angles are an Editor presentation. To keep what a
  designer typed (as Unity does with its serialized Euler hint), the Euler hint is an **Editor-layer**
  value stored with the Editor's scene data, not in the runtime component (phase 4).
- **Snapshots upgrade to v2** (`NEXORA_SCENE 2`) on the first save; v1 stays readable with identity
  rotation and unit scale.
- **Phases 1 and 2 ship together.** A data model without persistence would silently drop rotation and
  scale on save/load, so they are one change. The same change must also stop position-only writers
  (the Zig/C `write_component` bridge and the character controller's per-tick position update) from
  resetting rotation and scale; both previously replaced the whole `Transform` with `{x, y, z}`.
- Unity and Unreal transforms are **relative to a parent**. Nexora entities have no hierarchy yet, so
  until parenting exists this transform is effectively world-space. Parenting stays out of scope here
  but is the natural next plan.
  *(Update: parenting is now the [Entity Parenting Plan](Entity_Parenting_Plan.md); its phase 1 makes
  the transform local to the parent and moves snapshots to `NEXORA_SCENE 3`.)*

## 2. Verified current state

- `runtime::Transform` (`Runtime.h`) is `{double x, y, z}` with defaulted equality.
- The scene snapshot is the text schema `NEXORA_SCENE 1`; each entity writes `id x y z camera light
  mesh_renderer …` and loading rejects non-finite positions. A save/load round trip is asserted to be
  byte-identical in `runtime.v1_m4_vertical_slice`.
- The Zig/C gameplay boundary exchanges transforms through `GameplayTransformWire`, three packed
  doubles keyed by the stable component ID of `"Nexora.Transform"`. `GameplayHostBridge.h` states this
  wire format is deliberately independent of `runtime::Transform`'s memory layout, so adding members
  does not by itself change the C ABI.
- `GameWorld` / `SceneEditor` / `PlaySession` (apply-back `TransformApplyDiff`) and the Editor's
  `GizmoTransaction`, `UnknownComponentStore`, and `ViewportMath` all use `runtime::Transform` by value.
- The renderer side already has full math: `math::Transform` with `Compose` / `Decompose`
  (`Math.h`), and `GPUScene::UpdateTransform` takes a `math::Matrix4`.
- Number of files that mention `Transform` outside `Math.h`: about 25 (Runtime, Editor, Renderer
  GPU scene, Presentation, Showcase, Zig module, and tests).

## 3. Constraints

- The public C ABI and the Zig wire format must not change silently. Anything new is additive and
  versioned.
- Existing v1 snapshots must keep loading, with identity rotation and unit scale.
- Round-trip determinism must hold for the new fields (a fixed float format, no locale dependence).
- Validation must reject non-finite values, a non-normalizable quaternion, and a zero or non-finite
  scale, in the loader and in the setters, matching how positions are validated today.
- Do not claim graphical-Editor progress: the Editor stays at 0/8 until real graphical acceptance
  exists.

## 4. Options

**A. Extend `runtime::Transform` in place (recommended).** Add `rotation` (unit quaternion,
default identity) and `scale` (default 1,1,1) with sensible defaults, keep position members first.
One component stays the single source of truth; `Compose`/`Decompose` already map it to the renderer.
Cost: every by-value user recompiles, and aggregate initializers like `Transform{x, y, z}` keep
working only because new members default.

**B. A separate `Rotation`/`Scale` component beside `Transform`.** No change to existing code, but two
components must stay consistent, gizmos and undo must write both atomically, and the Zig wire needs a
second component ID. More moving parts for the same result.

**C. Matrix-only transform.** Store a `Matrix4`. Loses authored intent (Euler/quaternion, negative
scale ambiguity) and makes the editor fields lossy. Not recommended.

## 5. Impact of option A

| Area | Change | Compatibility |
| --- | --- | --- |
| `Runtime.h` | add `rotation`, `scale` with defaults | source-compatible for positional init of x,y,z |
| Snapshot | write `NEXORA_SCENE 2` with the extra tokens; keep reading `1` (identity/unit) | v1 loads unchanged; v1 files are not rewritten unless saved |
| Zig / C wire | keep `GameplayTransformWire` as is; add a **new** versioned wire struct and component ID for rotation/scale | existing modules keep working; no ABI break |
| `SceneEditor`, `PlaySession`, `TransformApplyDiff` | compare and apply all fields | conflict rules extend to rotation/scale |
| `GizmoTransaction` | snapshots and restores full transforms | cancel semantics unchanged |
| `ViewportMath` | add rotate-about-axis angle, scale-along-axis ratio, local/world axis, pivot | CPU-only, tested like the existing math |
| Renderer GPU scene | build `Matrix4` through `math::Compose` | already supported |
| Docs | Runtime, Editor, Renderer READMEs; Editor roadmap row; root README (both languages) | — |

## 6. Phases (each ends with evidence, not a claim)

1+2. **Data model and persistence (one change).** Extend `Transform` with defaults, validation
   (finite values, non-zero scale, non-degenerate quaternion, normalized on apply and on load), the
   snapshot v2 writer and v1 reader with a deterministic round trip, position-only writers that
   preserve rotation/scale, and hostile-input cases in `editor.parser_robustness`. Gate:
   `linux-development` and the full-feature config pass; Zig cross-compile for Windows and macOS with
   `-Werror`; v1 snapshots still load and re-save in the v2 form.
3. **Boundary.** New versioned Zig/C wire component; ABI layout tests; existing Zig module unchanged
   and still passing. Gate: the Zig gameplay tests and the ABI layout gate.
4. **Editor math.** Rotation/scale gizmo math with world/local/pivot and negative-scale rules, and
   multi-selection pivots. Gate: deterministic tests plus a documented decision table.
5. **Docs and status.** Update contract READMEs and the bilingual roadmap/README text. The graphical
   Editor remains unaccepted.

## 7. Risks

| Risk | Mitigation |
| --- | --- |
| Silent behavior change for code that compares or copies `Transform` | defaulted equality now includes new fields; audit every `==` and by-value copy in phase 1 |
| Float formatting differences break byte-identical snapshots | fixed format (`max_digits10`, classic locale) with a round-trip test |
| Quaternion drift under repeated edits | normalize on set and on load; test a long edit sequence |
| Negative scale flips winding and gizmo handedness | explicit rule and tests before any gizmo polish |

## 8. Out of scope

Hierarchy/parenting transforms, animation retargeting, physics-body synchronization, and any
graphical Scene View. Those need their own plans.

## 9. Questions (answered 2026-10-01)

1. Option A, B, or C? **A**, following Unity/Unreal.
2. Snapshot migration? **v2 on first save**; v1 stays readable.
3. Euler angles in the Inspector, quaternion in storage? **Yes**, with the Euler hint kept in the Editor
   layer so typed values are preserved.
