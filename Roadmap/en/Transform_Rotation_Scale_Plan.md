# Transform Rotation and Scale Extension — Plan

> Version: v1.0 | Status: **plan only; nothing in this document is implemented or approved** |
> Updated: 2026-10-01 | Relates to: `Editor_Roadmap.md` §ED-M2 (gizmos),
> `Engine_API_Foundation_Roadmap.md`

## 1. Purpose and decision needed

`runtime::Transform` carries a position only (`double x, y, z`). The Editor's portable viewport math
(`ViewportMath.h`) can pick and drag along an axis, but rotation gizmos, scale gizmos, world/local
space, pivot rules, and negative-scale handling cannot be specified or tested until an entity can
actually have a rotation and a scale. This plan records what such a change touches so it can be
reviewed as one decision rather than discovered file by file. It is not authorization to start.

**Decision requested:** approve (or amend) the recommended option in §4 and the phases in §6.

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

1. **Data model.** Extend `Transform`, validation, defaults; unit tests for identity defaults and
   rejection of NaN/inf/zero scale/degenerate quaternion. Gate: `linux-development` and the
   full-feature config pass; Zig cross-compile for Windows and macOS with `-Werror`.
2. **Persistence.** Snapshot v2 writer, v1 reader, deterministic round trip, hostile-input cases added
   to `editor.parser_robustness`. Gate: v1 fixtures still load and re-save to the documented v2 form.
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

## 9. Open questions for the owner

1. Option A, B, or C?
2. Should snapshots migrate to v2 on first save, or stay v1 unless rotation/scale is non-default?
3. Euler angles in the Inspector, quaternion in storage — confirm.
