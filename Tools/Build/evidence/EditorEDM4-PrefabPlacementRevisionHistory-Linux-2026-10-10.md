# Atomic live placement property and revision history — Linux

Acceptance recorded 2026-10-11 Taipei (2026-10-10 UTC).
Beads `nexora-pmb.1.28`; dependency Core PR514737611b3.
Only this supporting transaction is proposed for eventual Main acceptance.

`ApplyPrefabPlacementSnapshot` parses a complete bounded candidate and atomically
publishes exactly one advancing retained placement revision with its properties.
Instance/source UUIDs, exact scoped stable mappings, other placement records,
entity order and hierarchy remain unchanged. Unbound nodes retain canonical
metadata/runtime properties and their actual internal Node state, including
latent Euler hints that are absent from serialized current rotation.

Current PreparedSave, authoring authority and Editor World kind are required.
Nil/foreign instances, stale observations, changed mappings/scopes/source,
other placement changes, unbound edits, structural changes, invalid revision
and nontrivial equal-revision changes reject before mutation. Exact no-op leaves
pending Redo intact. Ordinary property snapshots still reject changed bindings.
The owner must independently resolve and revalidate source closure and current
writer/Play/recovery/external/consent policy; this primitive grants no authority.

The immutable history entry owns complete runtime/metadata and binding before/
after states. PushUndo captures the old binding before revision publication;
replay validates exact current state and restores properties plus revision
without IO or baseline change. Storage is staged/reserved before Runtime atomic
replacement. Rejected replay leaves cursor/content intact. Selection, keys,
clipboard and saved baseline survive. Limits remain4096 tracked nodes and8MiB
per complete source/prepared/runtime transaction, not a total history quota.

Actual public placement owners create two placements with retained nested scopes
and an unbound node. Source advances independently. Candidate stores authored
rotation, unavailable opaque bytes, disabled Camera91/.2/950, Light3.5 and64bit
Mesh/Shader references. Forty exact Undo/Redo cycles restore property bytes and
revision together. Tests cover unknown/mapping/scope/source/other placement/
unbound/hierarchy/input authority rejection, no-op pendingRedo, foreign failed
replay, latent720-degree hint revival, clipboard, Save/reopen and source archive
conservation. Exploratory const-fixture and early history-order failures are
excluded from accepted evidence; official canonical Runtime3 parsing creates
nondefault dormant fixture values.

Final focused related tests **4/4 passed,8.61s**; new mixed nested transaction
passes0.07s. Graphical Linux Development configure/build passed. Full
`ctest --preset linux-development`: **244/244 passed,605.99s**, zero failures or
skips. Shell, Cryptography, Slang, Zig, Showcase and native ProjectPlayer enabled.
Minimal Monolithic `linux-shipping` configure/build passed74 actual cold steps;
this profile does not validate full graphical Shipping. Documentation checks
and synchronized English/Traditional Chinese contracts accompany the change.

Source-rebase conflict planning, structural reconciliation and graphical source
rebase remain separate. Complete Editor milestones remain **0/8**. Public C++
consumers rebuild; stable C/Gameplay ABI and module graph are unchanged.
No other OS or physical-host execution is claimed.
