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

## Historical candidate gates before exact-head closure

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


## Exact-head CI closure — PR 717

✅ Head `c66cecd09df6861dce4c2103e021633eb088fcd1` passed all 38 checks from
PR run `40215766422` and its push counterpart `40215770787`. Both runs completed successfully.
After confirming unchanged head, base `72c82cc7`, mergeability and the authorized repository,
PR #717 was squash merged as `7968bf629b69db564a3473229950f0af2960809f`.
No failed or pending check was bypassed.

| Hosted gate | Result |
| --- | --- |
| Linux Development configure/build/full verbose CTest | 457 registered, zero failures, existing portable-font skip; 1317.02 s |
| Windows Development full CTest | 397 registered, zero failures, existing portable-font skip; 1782.44 s |
| macOS Development full CTest | 376 registered, zero failures, existing portable-font skip; 927.45 s |
| Corrected CLI fixture | Passed on Linux, Windows and macOS; macOS 0.18 s |
| Linux Shipping configure/build | Passed in build-contract; separate Linux Shipping package job passed |
| Feature-off, ASan/UBSan and TSan | Passed; retained existing font skip |
| Native display and all remaining platform/package gates | Passed |

The required Linux commands ran on hosted Ubuntu, not this Windows developer PC:

```bash
cmake --preset linux-development -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON -DNEXORA_ENABLE_SLANG=ON -DNEXORA_ENABLE_D3D12_WORK_GRAPHS=OFF -DNEXORA_ENABLE_VULKAN_BACKEND=ON -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON -DNEXORA_BUILD_PROJECT_PLAYER=ON -DNEXORA_ENABLE_PROJECT_PLAYER_NATIVE=ON
cmake --build --preset linux-development
ctest --preset linux-development -V --output-log build/linux-development/development-tests.log
cmake --preset linux-shipping -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON
cmake --build --preset linux-shipping
```

The receipt `build/scene-settings/phase2-ci717.json` retains exact-head/base/check identities.
The original macOS failure remains recorded above. Phase 2 is accepted solely for package/cooker
and source-free CPU consumption; Runtime transitions, native pixels, common executable building,
Editor Play and final relocated Editor ZIP retain their own gates. Scene-delivery acceptance is
2/5; complete graphical Editor milestone acceptance remains 0/8.
