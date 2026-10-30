# Optional 2D effects service

HD2D-01 delivers the data contract and snapshot service, not a native renderer. The current
Player does not register this service yet. A host must advertise only native features it actually
implements: lighting=1, shadows=2, selective bloom=4. The default service advertises zero and
rejects enabled submissions. No effect is silently emulated by the existing 3D path.

`NexoraGameplayTwoDEffectsApiV1` is independently negotiated through the appended
`get_twod_effects_api` services suffix. The full previous services prefix is unchanged. Probe
`struct_size` before reading the getter; absent/short/wrong-version services are unsupported.
Zig uses `nx.getTwoDEffects(services)` or `try context.effects()`. Check `capabilities()` before
requesting effects; the presence of a table alone is not feature support.

## Ownership, lifetime and publication

The host owns the service until after module Stop/Destroy. APIs are borrowed and all callbacks
run on the construction thread. Wrong-thread calls return LIFECYCLE. Null arguments return
INVALID_ARGUMENT; short/wrong versions return UNSUPPORTED. Output structures are untouched on
failure. Exceptions, including allocation failure, are contained as LIFECYCLE.

`submit` is a complete replacement, not a patch. It validates a bounded batch, resolves every
Sprite identity and optional emission dependency, copies all arrays, then atomically publishes
one pending immutable snapshot. Failure leaves pending/active state and revision unchanged.
The caller may immediately release or modify its arrays after return. The latest accepted batch
wins before publication. `BeginFrame` is called by the host before game updates; submissions during
that update take effect on the following boundary, not partway through rendering. Multiple fixed
updates and presentation share the same frozen camera/content/effects frame. Retained snapshots
own their arrays and may outlive subsequent publications; they do not own World or GPU resources.

State distinguishes accepted and active revisions. Counts/enabled describe active state, not
pending requests. Zero-array submissions explicitly remove previous effects. Repeated BeginFrame
without a submission retains the same object/revision. No callback consumes input or performs GPU
work. Only the owner may call BeginFrame; violations throw before changing state.

Host-provided Sprite and mask resolvers borrow live World/pool/catalog owners and must not mutate
or reenter the service. Full generation-bearing entity/pool handles are used, never render indices.
The renderer must resolve identities against its extraction snapshot again: an object destroyed
after submission cannot transfer settings to a reused slot. Immutable asset generations retained
by extraction protect GPU dependencies; this service alone does not keep an Atlas resident.

## Units and bounds

All vectors use existing 2D world coordinates; camera projection is host-owned. Light/ambient and
emission colors are linear RGB. Intensity and color channels are finite [0, 1e6]; positions are
finite [-1e6, 1e6]. Radius is (0, 1e6], bias [0,radius], softness and frame strength [0,1]. IDs are
nonzero, unique within each light/occluder namespace. Sprite identities are unique per kind.
Groups are u32 bit masks; zero matches nothing. A light affects a receiver when their masks
intersect; its shadow map includes casters intersecting that light mask. Receiver masks select
lights, not a second per-receiver subset of that light's casters. Shader arithmetic must remain finite for accepted
inputs; native paths must bound accumulation before conversion to RGBA16F.

Limits: 32 lights, 8 shadow-casting lights, 256 polygons, 16 vertices/polygon, 2048 total polygon
edges, 100000 Sprite overrides, 1024 angular shadow samples/light. These are admission limits,
not performance guarantees. Exceeding a limit rejects the entire batch as INVALID_ARGUMENT.

Occluders contain 3..16 vertices without a repeated closing vertex. Closure is implicit. Either
winding and concave simple polygons are legal; nonfinite coordinates, zero-length edges,
adjacent backtracking, self-intersections/touching and zero area reject. Unused fixed-array vertices
are ignored. Each polygon must have a nonzero stable ID; polygons are not inferred from alpha.

Sprite kind 0 is entity, 1 is pool. Missing host identity/mask resolvers are UNSUPPORTED, invalid
or stale identities/dependencies are INVALID_ARGUMENT. No emission Atlas (both UUID halves zero)
requires region and emission intensity zero. A present Atlas/region must resolve even with zero
emission strength. Reserved fields must be zero. Receive-light/shadow and enabled fields are 0/1.

Bloom intensity is finite [0,1e6], radius [0,64] output pixels, levels 0..5. Nonzero intensity
requires at least one level. The native path begins at half resolution; UI remains outside.
Strength scales decorative light/emission/bloom; games keep critical hazards independent.
Disabled snapshots may retain validated configuration; no native feature is claimed or executed.

## Compatibility and acceptance

The original services minimum stays unchanged; the new getter extends only the tail. Each prefix
is copied only through a complete advertised field. Existing C/Zig Sprite, Atlas and Clip wires
are unchanged. Runtime holds only the language-neutral table, with no TwoD dependency.

C++ tests cover prefix byte guards, thread/version/output failures, stale identity/mask rejection,
copy/retention/publication, invalid polygons, duplicates and capacity. Zig tests compare sizes,
alignment and offsets with canonical C, and probe old/absent hosts. Native effects, content tools,
Zig diagnostic and SDK publication remain later gates in the
[paired roadmap](../../Roadmap/en/HD2D_Lighting_Plan.md).
