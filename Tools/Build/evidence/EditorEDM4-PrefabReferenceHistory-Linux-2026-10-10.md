# Undoable prefab authoring reference — Linux, 2026-10-10

Beads: `nexora-pmb.1.15`. Parent is selected-revert feature `4aa6059d`;
its prerequisite foundation is not a main-merge candidate.

Managed four-CPU Linux with graphical shell, cryptography, Slang, Zig gameplay,
showcase and native Project Player enabled; pinned dependency overrides retained.

- Focused reference history, atomic property snapshots, guarded prefab review and
  actual isolation controls: **4/4 passed, 0.31s**.
- Full Linux Development: **247/247 passed, zero skipped, 641.88s**.
- Linux Shipping: configure/build successful, five incremental build steps.
- Logs: `reflected-inspector/reference-focused-{build,tests}.log`,
  `reference-full-{configure,build,tests}.log`,
  `reference-shipping-{configure,build}.log` in the execution work area.

The actual isolated owner publishes a variant, advances its base independently,
then changes only its retained reference. Reusable scene bytes stay identical,
the prepared signature changes, the document becomes dirty, and one Undo/Redo
restores both reference and clean/dirty state without changing keys, generation,
selection or clipboard. Ordinary property transactions preserve that context.
Mixed properties/reference changes use the same single history transaction.

Wrapped Save advances the variant revision for metadata-only changes, retains the
exact previous asset, preserves pending Redo and initializes a clean reference
after writer or read-only reopen. Review follows the current authoring reference
after Undo, rather than its last published metadata. Original scene bytes stay
unchanged. A missing retained source closure rejects publication and preserves
the stored source and local history.

Actual named Scene Save and Save All reject tagged documents without changing
disk bytes or acknowledging a baseline. A batch prepared before adding a tag
rejects stale publication. Untagged ordinary scenes retain existing behavior.

References are owning authoring context persisted by the existing wrapped asset
format, not reusable scene bytes. The serialized host validates actual project,
writer and source authority. No runtime/scene schema, C/Gameplay ABI or module
dependency changes; public C++ consumers rebuild. Three-way rebase planning,
graphical conflict choices and structural/live-instance reconciliation remain
unfinished. Full milestones remain **0/8**. Require accepted prerequisites,
retarget only this feature to main and rerun hosted CI before merge.
