# Multi-scene package candidate — Windows 2026-10-26

Scope: phase 2 of [project scene delivery](../../../Roadmap/en/Project_Scene_Delivery_Plan.md).
This evidence covers saved-scene capture, a pure multi-scene cooker, the Runtime catalog and its
CPU CLI verifier. It does not accept live scene transitions, game executable building, Editor Play
selection, publication freshness/cancellation or the final Editor-only ZIP.

## Candidate behavior

- Capture uses the enabled saved-scene list and startup UUID; missing disabled scene files are
  omitted. It uses isolated Worlds/Documents and copied referenced CPU catalogs, preserving the
  current Editor document. Pure cooking uses the same StaticView scene/asset validation authority
  through an owning-data/span adapter, merging the union by full UUID and identical cooked bytes.
- A distinct `NXGAME` schema-1/capability-2 wire contains a sorted catalog, startup and canonical
  NXAB assets. Shared mesh/material bytes occur once. Resolved immutable scene templates share CPU
  mesh/PBR owners; copied template/resource owners survive catalog destruction.
- Catalog/startup errors, missing/extra dependencies, cross-scene dependencies, global resource
  collisions, corrupt framing/checksums and unsupported required opaque behavior reject.
- StaticView schema-1 bytes and inspection policy remain separate. Both consumers use the extracted
  original bounded file reader/NXAB preflight. Windows intake additionally rejects reparse points.
- `NexoraProjectPlayer --verify-game-package` reports startup and counts, with native rendering and
  gameplay explicitly false. It does not consult authoring scene/source paths.

## Windows scoped verification

The existing isolated `build/developer-preview-20261024/source` checkout was synchronized with
the candidate; only touched C++ files/ranges were formatted. Commands:

```powershell
cmake --preset windows-development -DNEXORA_BUILD_PROJECT_PLAYER=ON
cmake --build --preset windows-development --target NexoraGameProjectPackageTests NexoraGameProjectExportTests NexoraProjectPackageTests NexoraStaticProjectExportTests NexoraProjectPlayer --parallel 3
ctest --preset windows-development --output-on-failure -R '^(runtime.(game_project_package|game_project_player_cli|static_project_package|static_project_player_cli)|editor.(game_project_export|static_project_export))$'
```

Six selected tests passed, zero failures (35.61 s):

| Test | Time | What it proves |
| --- | --- | --- |
| runtime.game_project_package | 0.04 s | Non-Main startup, overlapping authored IDs in isolated templates, canonical input permutations, shared CPU ownership/lifetime and failed admission |
| runtime.game_project_player_cli | 0.37 s | Real Runtime-only verifier consumes the two-scene/four-asset package in a directory with no authored scenes; legacy/corrupt consumption rejects |
| runtime.static_project_package | 1.79 s | Existing StaticView wire/consumer regressions remain valid |
| runtime.static_project_player_cli | 2.84 s | Existing StaticView CLI behavior remains valid |
| editor.game_project_export | 0.17 s | Actual saved settings/scenes, excluded missing scene, shared imported OBJ, deterministic scene-order permutation, unresolved resource rejection and source-independent cooked consumption |
| editor.static_project_export | 30.31 s | Existing owning StaticView producer regression remains valid after extraction |

Raw logs remain at `build/scene-settings/package-20261026-001611-{configure,build,test}.log`
and corresponding stderr files; `package-latest.json` identifies the final invocation.
The initial fixture compile error (implicit brace conversion to optional MeshComponent) was
corrected without changing production warnings. The extraction preserves one validation pass
for the legacy StaticView encoder; aggregate asset-copy/required-component admission now happens
before building an oversized or unsupported union. Earlier runs remain preserved; its log remains under the earlier package run.
This is developer-PC evidence, not clean-host, VM or native multi-scene pixel acceptance.

## Remaining gates

Full Linux Development configure/build/CTest and Shipping, feature-off/ABI/platform CI and native
scene session/build integration remain pending for this candidate. Phase-1 PR #716 validates a
different source head and must not be used as phase-2 acceptance. Keep this package phase pending until its own gates pass; the overall graphical Editor milestone status remains unchanged.

## PR 717 first CI attempt and fixture correction

At head `899668c6c32eb7b532d99fe8ffe3cc36feeb5d59`, PR run `40212398192`
passed Linux Development and Linux/Windows/macOS Shipping package jobs. The macOS
Development gate failed `runtime.game_project_player_cli`: Python supplied its `/var`
symlink temporary path to the deliberately alias-rejecting package reader. No merge was
performed. The fixture now resolves its temporary root to the physical directory before
writing or reading; production alias admission is unchanged. The verifier test also prints
its captured exit/stdout/stderr on failure. The corrected CLI fixture passed on the Windows
developer PC; macOS and the complete updated-head CI gates remain pending. This is not
native scene-transition or final Editor ZIP acceptance.
