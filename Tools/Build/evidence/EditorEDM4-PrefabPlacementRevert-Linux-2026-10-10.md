# Atomic live prefab instance property revert — Linux, 2026-10-10

Beads `nexora-pmb.1.24`; prerequisite is owning review PR511 `944ee22d`.
Core Revert requires an immutable current review, current project writer and explicit
serialized host authoring authorization. It restores all supported properties of the
whole persistent placement from the exact retained mixed nested closure. Current
published root advancement does not substitute newer properties for retained values.

Source and target materialize in temporary Worlds/documents. Names, authored Euler
and unknown opaque name/bytes come from the retained source. Bounded canonical
Runtime version-3 property records preserve TRS, component flags and every stored
Camera/Light/Mesh value, including nondefault dormant lens/intensity/64-bit resource
references. Target IDs, generations, parent relationships, sibling order, selection,
clipboard, placement bindings and scene save baseline remain intact. Any structural
parent difference rejects the whole operation. Source/current closure and target
scope are rechecked immediately before one `ApplyPropertySnapshot` transaction.
No source or scene file is written before ordinary explicit Scene Save.

Actual mixed nested revisions1/2 fixtures cover nontrivial rotation37.5, mirrored
scale, name/Euler720/opaque changes and local component additions. Complete active
component fixtures revert to dormant91/.2/950 lens, intensity3.5 and 64-bit Mesh/Shader
IDs. One Undo returns all local state and one Redo restores the whole retained baseline.
Read-only/false authorization/foreign/stale targets, recovery/external changes,
same-revision source replacement, missing archives and structural changes reject
without partial state. Root retained1/current2, unchanged files, pending Redo on
no-op, saved baseline/dirty transitions, explicit Scene Save and exact reopen pass.

Existing individual component presence-edit Undo loses nondefault dormant data;
that independently confirmed preexisting bug is tracked in `nexora-owg.2.6`. This
revert's complete property history preserves both active and inactive before/after
snapshots without changing public Runtime removal semantics or weakening assertions.

Final graphical Linux Development configure/build passed (two incremental steps).
Focused atomic property, nested materialization, project placement and semantic
comparison **4/4 passed,15.29s**. Full `ctest --preset linux-development`
**243/243 passed,614.36s**, zero failures or skips. Graphical shell,
Cryptography/OpenSSL, Slang, Zig, Showcase and native ProjectPlayer were enabled.
Minimal Monolithic `linux-shipping` configure/build passed (five actual steps).

This accepts the Core operation only. Graphical confirmation, targeted revert,
structural reconciliation, source apply/rebase and large-scene performance remain
separate. Complete Editor milestones stay **0/8**; Linux does not claim other OS
or physical-host acceptance. Public C++ consumers rebuild; stable C/Gameplay ABI
and serialized formats are unchanged.
