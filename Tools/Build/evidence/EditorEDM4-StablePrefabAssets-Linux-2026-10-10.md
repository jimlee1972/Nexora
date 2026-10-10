# Stable prefab assets and dependency resolution — Linux acceptance, 2026-10-10

This Editor Core prerequisite owns exact scene/unknown bytes, stable asset/node/field UUIDs,
revisioned canonical storage, base references and scoped nested references. It does not yet provide
graphical isolation, instance materialization or override diff/revert/apply/rebase.

## Acceptance

- Full graphical/native `linux-development` configure/build/CTest with Slang, Zig, Showcase and
  native Project Player: **234/234 passed**, zero skips, **581.56 seconds**. The build had **312 steps**.
  Monolithic `linux-shipping` configure/build passed, **14 steps**, with Editor excluded.
- `editor.prefab_asset_graph`: **1/1**, **1.25 seconds**. Actual SceneDocument rename, Undo,
  schema-3 save/reload and unavailable opaque payloads preserve stable identities and exact bytes.
  The identity callback can mutate its external previous object without invalidating the captured
  owning identity baseline.
- Exact codec round trips and actual disk save/reopen retain every node/property/base/nested field.
  Every-byte truncation, unknown version, trailing records, overflow counts, corrupt scenes,
  duplicate/nil IDs and foreign node/property mappings reject without a partial asset.
- Actual variant/base and repeated nested placements have separate instance scopes. Missing/stale
  revisions, duplicate supplied asset identities, invalid attachments and mixed base/nested cycles
  reject. Depth 32 succeeds and deeper input rejects; shared dependency diamonds remain memoized.
  Expanded instance and actual 2050-node source/aggregate-node limits reject without partial graphs.
- Project-owned canonical publication uses the real writer lease and exact previous revision bytes.
  Actual create/reopen/replacement, stale revision, read-only observer, occupied foreign staging,
  pending recovery, changed workspace, aliased source and externally corrupted prefab fixtures
  validate preservation and explicit resolution. Reads use bounded incremental IO.
- Touched C++ clang-format and `git diff --check` passed. CI explicitly requires the prefab test to
  be registered and pass in graphical Linux acceptance.

Commands: repository `linux-development` with graphical shell, Slang, Zig, Showcase and Project
Player/native enabled, `cmake --build --preset linux-development -j4`,
`ctest --preset linux-development`, `cmake --preset linux-shipping`,
`cmake --build --preset linux-shipping -j4`. Exact pinned ImGui/Vulkan source overrides reuse the
repository's dependency versions. Windows/macOS/Android/iOS execution is not claimed from Linux.

Each supplied asset UUID has one exact revision; conflicting revision requirements reject. Variants
retain a full materialized scene with base metadata. Current property IDs cover supported built-in
fields and whole opaque components. Native field interpretation, persistent instance metadata,
transactional nested materialization and complete graphical prefab workflows remain open. Atomic
publication follows the serialized single-writer filesystem contract, not hostile-race isolation or
a multi-file power-loss journal. Supporting acceptance does not change the **0/8** milestone count.
