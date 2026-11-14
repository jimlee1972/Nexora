# Gameplay generation staging

`GameplayGeneration` owns the mutable TwoD gameplay values for one single-scene or catalog
embedding: a Play `World`, Sprite/collider metadata, Tilemaps, an `InstancePool`, optional
`SceneSession` and `GameplayBridge`. It owns no gameplay library, renderer, native surface or
external service. Single-scene creation loads an empty scene; the embedding activates it.
Catalog creation retains immutable authored sources and starts with no live scene.
The existing single-scene factory retains its signature and Tilemap-enabled default. An additive
four-argument overload can disable the Tilemap bridge capability; staging preserves that choice.
C++ consumers rebuild for the updated owner layout. The C/Zig wire contracts do not change.

## Staging contract

All mutation and `Stage()` run on the construction thread. Before staging, close admission of
gameplay work, drain every callback/work lease and release all World/metadata borrows. The owner
must remain alive on that thread through staging and retirement. Mutable accessors support host
integration; they do not add synchronization or make retained references safe across publication.
Wrong-thread calls, recursive staging, entered bridge callbacks, pool iteration and active scene
publication reject staging. Stopped catalogs, missing single scenes and non-Play Worlds reject.

`Stage()` allocates an unpublished independent generation. Failure returns null and publishes no
state. The copy preserves World scene/entity IDs and lifecycle state, complete Sprite/collider and
Tilemap values, pending/completed scene requests and revisions, camera and cached query snapshots,
animation clock/pause/style, and immutable Clip owners. Every copied Sprite/bridge reference points
to its new World/session/pool. Pool owner/index/generation, free-list order and stale-handle state
are copied exactly without consuming another pool owner ID. Handles remain values scoped to the
generation selected by the host context; hosts must not expose both copies as one mutable pool.

The Clip resolver and immutable asset registry must outlive every active, candidate and retired
generation. Resolver captures must not borrow generation World/metadata or gameplay library code.
Resolver copying must not mutate the host or its assets. Reentrant `Stage()` from a resolver copy
rejects; allocation/copy exceptions discard only the candidate. This API does not clone external
content registries, resource handles, input, audio, UI, storage, window state or background tasks.
An embedding must isolate those separately or reject reload when their isolation is unavailable.

## Strict Runtime transaction

Use the strict `GameplayModuleHost(host, barrier, transaction)` overload. Keep host tables and
their contexts at stable addresses through module callbacks and dynamic-library finalizers.

1. `begin` calls `Stage()` after quiescence, retains a separate candidate context, routes all
   candidate World/TwoD callbacks to it and advertises `RELOAD_CANDIDATE` for the opt-in SDK lifecycle.
2. `rollback` discards the candidate after its Destroy and library finalizers. The live context,
   library, World, pool and query/animation state remain callable and unchanged.
3. `retire` quarantines the complete old generation without rebinding its cached API contexts.
   Old Stop/Destroy/finalizers operate on that retired generation only.
4. `commit` publishes the candidate after all old code retires, then releases retired values.
   Publication invalidates old World/metadata borrows. No throwing work belongs in retire/commit.

The owner remains responsible for full service isolation, work leases and native frame/resource
retirement. This reusable boundary does not itself implement production Player/Editor reload or
source-file watching. Main packaged Sprite services and native integration remain separate tasks.

The standalone [Player embedding](../../Apps/TwoDPlayer/README.md#strict-generation-ownership)
now supplies per-generation host/API contexts and external-service admission around this owner.
It also guards save-state capture before Runtime starts the staging hook. Those policies belong to
the application; `Stage()` itself continues to copy only the TwoD values described above.

## Regression coverage

`twod.gameplay_generation` exercises single/catalog isolation, partial paused animation clocks,
stable/stale pool handles, cached queries, pending scene transitions, throwing resolver copies,
wrong-thread calls and callback/copy reentry. `twod.source_reload_transaction`, enabled with
`NEXORA_ENABLE_ZIG_GAMEPLAY`, consumes five independently compiled canonical SDK libraries. It
checks 32 successful unique-library replacements and 128 rejected prepare/restore/reloaded/schema
candidates, drains a real leased old-library job before capture, and reimports a source UUID to a
different Atlas/region/Pivot while retaining World anchors and pool handles. Extraction checks CPU
geometry and retained assets; it does not claim native pixel acceptance for reload.

```bash
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development
ctest --test-dir build/linux-development \
  -R '^twod\.(gameplay_generation|source_reload_transaction)$' --output-on-failure
```
