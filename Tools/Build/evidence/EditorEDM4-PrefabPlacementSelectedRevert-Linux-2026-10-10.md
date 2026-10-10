# Targeted live prefab property Revert — Linux, 2026-10-10

Beads `nexora-pmb.1.26`; prerequisite is whole-instance Core Revert PR512
`b1c380c773a510190d6cabb517eb54fffebe9508`.

`RevertSelected` borrows bounded immutable review row indices during one serialized
explicit authoring call. Current writer, explicit authorization, exact project/target
and complete retained/current source closure are revalidated. Duplicate or out-of-range
indices, unsupported groups and any structural row reject before publication.
Empty selection is a semantic no-op that preserves pending Redo.

Selected semantic lanes expand to complete scoped property groups: Name, XYZ position,
quaternion plus authored Euler, scale, Camera/Light/Mesh flags and all stored values,
or one unavailable opaque type's name and payload. Distinct lanes in one group coalesce.
The bounded canonical Runtime version-3 bridge stages only those complete groups;
unselected properties and other scoped nodes retain their exact local values. Local
opaque additions with no source field UUID can be explicitly removed without inventing
an identity. Final complete property publication is one atomic Undo/Redo; live keys,
selection, bindings, clipboard, saved baseline and every project file remain intact.

Actual mixed nested revisions1/2 fixtures verify Name-only restoration preserves
Euler720 and unavailable local bytes; selected rotation plus opaque-addition removal
preserves unselected Name/opaque values. Two selected position lanes restore complete
XYZ2.75/3.25/-7.125 while preserving local scale4/5/6 and authored Euler. Selecting
Camera presence restores the complete dormant91/.2/950 group while preserving local
Light9 and Mesh8123. Duplicate/out-of-range, reader/false authorization, one Undo/Redo
and no-op pending Redo cases pass. Whole-instance rejection/history fixtures continue
to exercise the shared complete source/target validation path.

Final graphical Linux Development configure/build passed. Focused project placement
**1/1 passed,0.18s**; full `ctest --preset linux-development` **243/243 passed,606.95s**,
zero failures or skips. Graphical shell, Cryptography/OpenSSL, Slang, Zig, Showcase and
native ProjectPlayer were enabled. Minimal Monolithic `linux-shipping` configure/build
passed (five actual steps). Changed Markdown validation and the 16-test documentation
regression suite pass.

Graphical row selection, structural reconciliation, source apply/rebase and large-scene
performance are separate. Complete Editor milestones remain **0/8**; no other-platform
or physical-host acceptance is claimed. Public C++ consumers rebuild; stable C/Gameplay
ABI, module dependencies and serialized formats are unchanged.
