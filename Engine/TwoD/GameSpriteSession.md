# Main packaged-game Sprite state

`GameSpriteSession` is an optional TwoD owner on the main Player's existing Play `World`.
It augments `Runtime::GameSceneSession`; it neither loads a second World nor replaces the
authoritative Runtime scene service. The stable C/Zig host, 2D/source/pool tables and all
package/asset codecs remain unchanged. C++ consumers rebuild.

## Ownership and calls

The embedding's World outlives this owner, every metadata preparation and every module callback.
The owner retains a fully admitted `GameSpriteAssets` value, immutable scene-template owners,
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
source/animation values and pool handles; it does not provide a main Game reload transaction.

This adopts the main native Player only. Configured packaged Editor Play adoption, source watching/
publication and complete strict Editor/Player reload with 32 real replacements remain separate
parent work. No reload command/transaction is added by this adapter, and existing module-state-only
validation is not proof of World/service rollback. Milestone totals remain unchanged.
