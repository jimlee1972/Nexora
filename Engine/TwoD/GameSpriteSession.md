# Main packaged-game Sprite state

`GameSpriteSession` is an optional TwoD owner on the main Player's or configured Editor's existing
Play `World`.
It augments `Runtime::GameSceneSession`; it neither loads a second World nor replaces the
authoritative Runtime scene service. The stable C/Zig host, 2D/source/pool tables and all
package/asset codecs remain unchanged. C++ consumers rebuild.

## Ownership and calls

The embedding's World outlives this owner, every metadata preparation and every module callback.
The owner retains shared immutable, fully admitted `GameSpriteAssets`, immutable scene-template owners,
per-instance Sprite/collider values, one stable `GameplayBridge` and one `InstancePool`.
The default pool capacity is 4096; explicit capacities use the existing 1..100000 pool bounds.
Source-free main games and StaticView hosts leave the optional adapter unset and retain their
original capability table. Main games with auxiliary typed assets expose the existing optional
2D API, including source Sprite, animation, collider/query, camera and pool operations. Tilemap
operations remain unsupported. No mutable content-registration API is added.

All operations serialize on the construction thread with World mutation and native preparation.
C API entries reject foreign-thread, reentrant or outstanding-preparation calls with Lifecycle
without publishing outputs. Host C++ borrows expire on publication/entity destruction. The owner
outlives preparations; destroy/drain those before destroying the session. No game-library callback or native
resource is stored in metadata; preparation/extraction performs no source IO or watching.

The initial published state copies authored source references through each instance's actual
authored-to-live mapping, with default white styling. Gameplay then owns live Sprite/collider
changes. Rendering never replays immutable references over those edits. Call `Reconcile` after
World EndFrame and before transition/rendering to forget destroyed descendants and their animator
and query state. Other logical scenes outside the Runtime catalog are not a second render scope.
API entity arguments are live World handles; no authored-ID lookup service is introduced.

Configured Editor host despawn calls `Bridge().ForgetEntity`, which clears Sprite/collider
metadata, animation state and cached entity-query readiness together. Subsequent overlap/sweep
calls reject the invalid snapshot without publishing outputs until an explicit rebuild. The
session's metadata-only helper is not a substitute for this bridge cleanup.

## Scene preparation

Call `Prepare` only inside the Runtime scene preparation boundary, after gameplay callbacks.
It validates the complete candidate scene/mapping scope, source closure, unique live identities,
retained template/mapping identity and primary scene. There are at most 256 instances and 100000
mapped authored identities. Retained instances copy current Sprite/collider values; new instances
copy their authored source bindings. Deleted metadata remains deleted. No live World is modified.

Only one owning `Prepared` can be outstanding. Its frame extraction borrows the exact read-only
candidate World for that call, checked by an opaque identity token; no candidate World borrow is
retained. Its private metadata is already bound to the stable final World object, without reading
that object's pre-commit contents. Native Scene/Sprite admission runs before Runtime publishes
the candidate World. Runtime then invokes `Commit`, which swaps metadata and advances revision
without allocation or callbacks. Rejected/throwing native admission destroys the preparation,
retaining all live metadata, World values, animator clocks and global pool handles.

The stable bridge synchronizes topology revision at its next operation: it invalidates query
snapshots, forgets retired entity animations and resets the 2D camera only when primary changes.
Retained entity clocks, pause/speed/style and immutable Clip owners remain intact. The pool is
global to this owner and survives scene Replace/Additive/unload until gameplay explicitly destroys
its handles. The existing owner/index/generation checks still reject stale handles.

## Frames and lifetime

One current-frame extraction combines current scene metadata with pool values and actual World
affine poses. Render-local identities preserve entity ordering followed by stable pool slot order;
they reserve no Runtime World ID range. The shared extractor retains source crop/Pivot, flips,
negative scale, tint and sorting. It enforces the existing 100000 input/64 resolved Atlas bounds.
The default overlay camera remains centered with vertical size 20, independently of the 3D camera.
Gameplay advances animation explicitly; the host adds no clock or automatic playback.

The native host draws the existing 3D palette and one Sprite UI overlay. It borrows uploads only
for that call; the surface owns copied fence resources and drains before catalog destruction.
Missing/malformed metadata or failed CPU/native admission follows the existing scene rollback and
native-device failure policy. This scene transaction does not roll back arbitrary gameplay callback
side effects or recover a lost native device.

## Acceptance scope

`twod.game_sprite_session` covers actual main-host negotiation, legacy table prefixes, remapped
metadata, count-only animation probes, preserved World/style/clock, pool handles, rejected native
preparation, persistent Replace/Additive/unload, missing source startup rejection, destroyed
descendants and owner-thread guards. `editor.linux_game_source_services` builds a canonical SDK
game through the actual Editor producer, deletes original sources, relocates the game to a Unicode
path and exercises real D/W/S/A input across five exact native source/pool masks. A separately
built source-free game's central 80x80 3D region stays exact, and SDK Stop/Destroy drains the pool.

The bridge rejects the single-scene `ForkForReload` and `RebindForReload` operations before
accessing single-scene metadata. Rejection preserves this Runtime World, stable API context,
source/animation values and pool handles. These single-scene bridge operations do not implement
the embedding's main Game reload transaction.

## Embedding reload

The owner has separate `ForkForReload` and `RebindForReload` operations for an embedding's existing
World transaction. Fork requires a distinct, already cloned Play World with every metadata entity
in the same scene. It copies scene metadata, animation clocks/events, camera, query state, revision
and pool slots/free list/handle identities, while retaining immutable asset/template owners. The
Clip resolver owns its shared asset catalog; destroying the original session cannot invalidate the
branch. Fork does not publish state or open native devices. Allocation failures propagate to the
embedding's rollback boundary.

Rebind validates every scene/entity membership before changing any pointer. It performs no
allocation, callback or native operation. It retargets the session, scene metadata and stable
bridge to the admitted World; cached API contexts keep their identity. Both operations reject
foreign-thread, outstanding-preparation, bridge-entry or pool-iteration calls. The embedding owns
both Worlds and sessions until all candidate or retired module callbacks and library finalizers
have completed; these primitives do not themselves provide that lifecycle policy.

Configured Editor Play uses these operations inside `PackagedPlayContext` and
`ReloadablePlayGameplayModule`. A generation-owned proxy guards the existing parent/source C
tables. Candidates mutate private World/metadata/pool state; failed Create/Restore/Start or forbidden
Capture writes discard it. At acceptance, old cached callbacks are retargeted to the quarantined
retired World/session before Stop/Destroy. The new owner is adopted only after retired teardown.
This reuses the canonical Runtime scene service and legacy input snapshots; rich custom-scene
UI/audio/input services remain a separate embedding.

`editor.packaged_sprite_play` exercises 32 successful CPU module replacements and 160 rejected
attempts, preserving live entity/pool identities and a half-frame animation clock while checking
retired callback writes. `editor.linux_packaged_source_services` compiles the saved SDK ScriptProject
in the actual Editor and checks source/pool pixels through animation, Additive, unload and Replace.
The latter is a native initial-generation proof, not 32 native Zig replacements. Source watching/
publication, source reimport and complete strict Editor/Player acceptance remain parent work.
Milestone totals remain unchanged.
