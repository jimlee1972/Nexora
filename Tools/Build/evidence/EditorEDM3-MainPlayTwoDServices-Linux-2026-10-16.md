# Main Editor Play 2D services — Linux, 2026-10-16

Beads: `nexora-zih.5`, supporting `nexora-zih`. Base: `5f3da9895379af7324a73e232d53fb83ecdc5a40`.

The actual `NexoraEditor` Play start path now selects recognized project Atlas/Clip sources,
captures the existing project-scoped cooked closure and revalidates current scope/source bytes
immediately after cloning, before module loading. The selected active document supplies the scene
identity. No gameplay library or no 2D sources preserves the inspection/3D path. Projects with 2D
sources must pass capture even when the selected library only uses 3D services.

`PlayTwoDServices` borrows the existing clone, owns fresh Sprite/collider metadata and a 4096-slot
pool, retains immutable Atlas/Clip owners, and supplies the canonical bridge. The host table is
constructed after binding, so capability/getter presence reflects the session. Full module
replacement destroys the old module before its services. Failed Create/Start clears candidate
services; Stop/Destroy callbacks retain the World and pool. Generic despawn forwards every
`LastDestroyed` ID, including descendants, before later queries.

Validation on the recorded source hashes in the [machine-readable record](EditorEDM3-MainPlayTwoDServices-Linux-2026-10-16.json):

- `cmake --preset linux-development`, `cmake --build --preset linux-development`,
  `ctest --preset linux-development`: passed, **333/333**, zero skips, **1068.91 seconds**.
- New `editor.play_twod_services`: passed in 0.05 seconds. Its 32 cycles cover real Play clones,
  project freeze/revalidation, external owner destruction, failed Create/Start, module replacement,
  descendant cleanup, pool handle isolation, wrong-thread bridge rejection, short API output
  preservation, source Undo/Redo conservation and capability-free 3D loads.
- Focused new/existing host tests: 2/2, 0.06 seconds. Initial fixture compilation omitted the
  required fixed-update callback on two `PlaySession::Start` calls; corrected before these passes
  and the complete gate. No failing full-run case required a retry.
- Existing native 3D Play soak: 32 cycles, 128 lifecycle records, source bytes/pixels unchanged,
  zero validation errors. Warmed peak RSS growth **147,456 bytes**, below **33,554,432**.
  This is a bounded regression guard, not a leak-freedom or new native 2D Game View claim.
- `cmake --preset linux-shipping`, `cmake --build --preset linux-shipping`: passed.
  This Minimal Monolithic configuration excludes Editor/testing.
- Existing graphical feature-OFF build at `/tmp/nexora-editor-20261016/2d-off`, configured with
  `NEXORA_ENABLE_2D=OFF`, `NEXORA_ENABLE_ZIG_GAMEPLAY=OFF`: built `NexoraEditor`,
  `NexoraPlayGameplayModuleTests`, `NexoraPlayDiagnosticsTests`, `NexoraPlaySceneServicesTests`.
  Three host/service tests passed, 0.16 seconds; optional 2D tests were absent and no TwoD dynamic
  dependency was present. This is an actual Editor build, not configure-only evidence.
- Documentation tooling: 16/16 tests; eight changed Markdown documents pass validation.
  Five touched C++ files pass clang-format; root README is unchanged.

The initial full gate passed 333/333 in 1084.36 seconds on `cb6b7916`. PR review then found
that EditorApp directly used TwoD without declaring its direct module/target dependency. The
corrected source declares `EditorApp -> TwoD`, links the graphical executable only under the
2D feature gate, and links the new optional test directly. The complete Development, Shipping
and actual graphical feature-OFF gates above were repeated after that fix. Both full runs passed
without retries; initial evidence is retained separately in the JSON. Hosted checks for the
initial head do not substitute for corrected-head CI.

All calls serialize on the Editor owner thread. Bind requires an active scene in a Play World;
the caller owns immediate project authorization/revalidation. API operations affect only that
scene. Tilemap callbacks return `UNSUPPORTED` because this host supplies no Tilemap configuration.
Gameplay explicitly advances animation. Other direct World destruction paths must notify the
bridge separately. Copied API tables expire at module destruction and cannot cross sessions.

Main Game View 2D drawing/native pixel acceptance, authored Sprite persistence, Tilemap binding,
seamless hot reload/native texture retirement and physical target-host acceptance remain open.
ED 0/8 and 2D 9/11 (P0 6/8) milestone counts are unchanged; the parent remains in progress.
