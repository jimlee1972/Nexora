# Main Game/reload bridge integration repair — Linux Cloud, 2026-11-14

Combined final-source Linux Development acceptance passes **549/549**, zero failures/skips, **2280.01 s**, all **15 local gates**, **16 actual GNU15 commands** and **10 Monolithic CPU cases**. Exact-head repair CI is required before delivery.

The original main Game source/animation/pool generation `08498a00` / tree `bdb6ba74` passed
540/540 unfiltered Linux tests, fourteen local gates, thirteen actual GNU15 compiler commands
and 41/41 first-attempt hosted checks. Before its agent's final merge action, PR #790 was updated
to `0d439a77` and already merged as `30fcf6fd`, incorporating main `21d70c40` (PR #763 reload
and PR #789 Sprite-source authoring). The old checks do not validate that combined tree.

New-head GNU15 mimalloc and MSVC Shipping SDK logs both fail in `GameplayBridge::State`:
the World member changed from a reference to a pointer, while the main Game constructor still
used `world(w)`. Exact-parent comparison also finds both bridge state-copy/fork implementations
missing. The new pool private-copy tag and friendship no longer match the retained
`GameplayGeneration` consumer; the first repair-focused build exposed those compile errors.
The following focused build also exposed the dropped packaged Sprite include and typed startup
admission. Both are restored while retaining the new packaged reload hooks. Existing CPU/native
packaged Sprite tests continue to require complete typed admission before Play. Every original
accepted snapshot, new failed job log and both failed focused-build logs is retained.

The repair initializes the same borrowed World pointer, restores the original bridge
`CopyStagedState` and single-scene `ForkForReload` implementations, and keeps both the main Game
router and reload bridge operations. Main Game and scene-session routers reject single-scene
fork/rebind before accessing single-scene metadata. The existing pool `ReloadCopy` implementation
also serves the owning GameplayGeneration branch, with its required private friendship restored.
There is no second pool-copy implementation or public C/Zig/package format change.

The main Game regression supplies valid cloned World/scene/pool inputs and checks rejection,
unchanged API context/revision, original World affine values, the live animated source and the
same pooled handle. Existing bridge tests retain their 32 real clone/retirement/rebind generations;
existing owning GameplayGeneration and Editor ReloadablePlay tests cover both supported paths.
The first complete build additionally found the dropped authored Sprite staging owner, packaged
texture declarations and Scene/Sprite compositor block. Those accepted paths and the one-EndFrame/
one-UI-submission ownership are restored, preserving the new startup checkpoints and packaged
reload hooks. The new facade forwards its no-library authored preview to the existing owning
backend. A subsequent actual graphical Editor build and all six focused regressions pass (0.33 s).
The failed frozen-six full build remains separately archived; it never reached CTest acceptance.
These tests do not accept full main Game source watching/reload or complete ED-WF-07/08.

Final combined commands and successful results are recorded below and in the machine receipt.
Parent workflow, graphical Editor, TwoD and physical/clean-host milestones remain open and unchanged.

## Retained first native integration failure

Frozen-eight configure/all-target build and all fifteen GNU15 commands passed. The first
completed full-549 case, `editor.linux_prefab_isolation`, failed after 38.74 seconds because
Prepare remained disabled with two unresolved conflicts. The exact owned full CTest was stopped
with exit130 and is not accepted. Its complete project, native output/input and source snapshots
are retained separately, including a second byte-hashed fixture copy.

The trace identifies provisional popup targeting: frame802 publishes Take source at674.5,643.5;
frame803 moves its visible center to618.5,652.5. The logged real click at674,644 misses that final
popup. The second choice similarly uses pre-final geometry. Incoming fast mouse acknowledgements
exposed the existing private observer's hidden sizing-frame coordinates. The observer now reports
option centers only when the popup is not hidden/skipping, the item is visible and its center is
inside ClipRect. It changes no widget action, rebase permission, consent, native gesture count,
retry policy, eight-second bound or exact file/revision/read-only oracle. `nexora-whr.8.3.5.1`
tracks this demonstrated cause separately from historical unexplained Save issue `nexora-49s`.

The unchanged-input native isolation case passes after this observation correction (**42.71 s**);
the synthetic UI regression also passes (**6.36 s**). A fresh nine-source full generation follows.

## Retained authored-binding integration failure

Frozen-nine configure/all-target build and fifteen GNU15 command variants passed, and the
corrected Prefab isolation case passed inside the full run. Later `editor.play_authored_sprites`
failed2.86s because initial sources were unavailable before Create; `editor.sprite_source_native`
also failed0.31s at module load. The exact owned full process was stopped130 after214 passes/two
failures; it is not accepted. Its source/log snapshots and native-attempt artifacts are retained.
The existing C++ fixture removed its temporary project in catch, so that original project is
unavailable and is not claimed retained.

Incoming PlayTwoDServices removed captured Runtime-byte/reference/source admission and private
Sprite seeding, and its parent proxy omitted the existing Source API getter. Those initial-Play
steps are restored; reload continues to use resource forks. Source callbacks now share the same
phase gate, including cached Capture-write rejection, through an owner-aware parent proxy. The
same parent acquisition copies only its negotiated supported prefix, preserving caller tails.
A first local extension attempt encountered the private generic proxy's final declaration;
allowing this deliberate nonmoving extension retains its base subobject as the stable C context.
The original compile log is retained. No C/Zig/public package layout changes are introduced.

Actual Editor build and five focused regressions pass2.50s: initial authored binding/malformed/
source-deleted and32 module-free cycles, real native Source presentation, reload, service and
generic proxy tests. New assertions check legacy parent prefix/tails, the Source suffix and
cached Capture write rejection with unchanged source/World state. `nexora-whr.8.3.5.2` remains
open until fresh combined full/linkage/CI acceptance.

## Retained Windows export-consumer integration failure

Repair head `146b3475` passed full549/549 (2289.04 s), all fourteen local gates and
metadata. Its twelve C++ source hashes remain unchanged. Hosted Windows SDK producer/consumer
passed, but four push/PR DX12/Vulkan native package jobs failed the same Scene snapshot assertion.
Merged main30fc had replaced the existing owning snapshot-reader call with raw JSON decoding,
while retaining `return $snapshot.State`; the raw root has no such wrapper member.

The production export again calls `Read-ShowcaseStateSnapshot`, copies its owning JSON/Markdown
and returns its decoded State. The expanded actual PowerShell regression loads both production
functions, demonstrates the old consumer returns no scene state, and passes after restoration.
It retains the exclusive-writer race, eight invalid/incomplete states, stale-file removal, exactly
one fresh X request and exact scene/evidence values. Native gestures, five-second bounds and
scene assertions remain unchanged. The four failed hosted logs and initial read-only PowerShell
cache initialization failure are preserved separately. Portable PowerShell7.4.6 is staged outside
the repository with its published SHA256 verified. A fresh full549/fifteen-gate generation and
new exact-head CI are required; no earlier successful generation is reused as final acceptance.

## Executed combined generation

- Full Linux Development configure, all-target build and unfiltered CTest: 549/549, zero failures/skips.
- Sixteen actual GNU15 command variants across fifteen producer/consumer sources, including main Editor/Player, TwoDPlayer, bridge/generation, new main session and both PlayContext variants plus the Editor reload facade consumer. All original warnings remain enabled.
- Actual PowerShell snapshot reader and Export-State consumer pass, including exclusive publication, stale-file removal, one fresh input request, exact owning scene evidence and eight invalid/incomplete states. The existing Windows-only race regression also loads the same reader and preserves exact tour/Markdown evidence, bounded incomplete rejection and no rejected publication; it was executed separately after fixing its loader, with product sources unchanged from the full549 run.
- Full Shipping build; both the canonical SDK main Game five-phase services test and existing static animation-root native regression pass. Development and Shipping each verify 65,536 independently expected Sprite/pool pixels, stable second captures, unchanged central 80x80 3D pixels, source deletion/Unicode relocation, pool identity and SDK Stop/Destroy/native drain.
- Monolithic graphical Editor/Player build plus ten CPU cases, including both reload families and packaged Sprite typed admission/rollback/retirement.
- Actual TwoD-OFF/testing-OFF Editor/Player build and capability-4 package refusal.
- Stable API/documentation utilities, twelve touched C++/inline files, thirteen Markdown files/three bilingual pairs and unchanged root README are checked separately before commit.

Original successful, externally merged failing, intermediate focused/build-failing and final
combined generations remain distinct. The machine receipt preserves commands, source hashes,
SDK records, native receipts and log hashes. Parent ED-WF-07/08 and physical/clean-host gates
remain open; Workflow5/9, graphical Editor0/8 and TwoD9/11 are unchanged.
