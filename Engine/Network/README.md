# Network contract

`NexoraNetwork` is the portable V2-M5 transport boundary. It depends only on Core and therefore can
be linked by a headless server without Renderer, RHI, Presentation, UI, audio, GPU assets, or Editor.

`INetTransport` owns datagram delivery and is advanced and polled synchronously by its caller.
`Connection` borrows its transport for its whole lifetime; neither object is thread-safe. Handshake
traffic validates both protocol version and build ID before user packets are accepted. Rejection,
disconnect, channel semantics, and sequencing are explicit state rather than transport exceptions.

`NetworkEntityID` is a server-allocated, session-scoped index plus generation and is never a local
ECS `EntityID` or authoring UUID. `ServerEntityMap` exclusively allocates IDs and owns the server
local-to-network binding. `ClientEntityMap` consumes spawn/despawn identities and owns an independent
bidirectional local mapping, so peer-local handles need not match. Despawn tombstones generations and
rejects delayed lookup, spawn, or despawn operations. On reconnect, callers reset both maps: the server
advances live generations before reuse, while the client discards the old session namespace and its
tombstones. Both maps are caller-owned, synchronous, and not thread-safe. A successful client spawn
binding transfers no ownership of the local ECS entity; callers remain responsible for creating it
before binding and destroying it after removing the binding.

`ReplicationSchema` defines a stable schema ID and monotonically increasing version, immutable
16-bit field IDs, wire types, optional quantization bounds/bit counts, and required-versus-optional
compatibility. Schema hashes use a specified field-order-independent FNV-1a byte stream, so the same
contract has the same hash across compilers and builds. A newer schema is backward compatible only
when every old field retains its ID, wire type, and quantization and every added field is optional.
Snapshot encoding is canonical little-endian TLV ordered by field ID. Decoders reject malformed,
duplicate, wrong-wire, and missing-required data; well-formed unknown fields are preserved as opaque
wire values. Encoding and decoding are synchronous, allocate into caller-owned values, and retain no
input spans.

Delta packets identify both snapshot and baseline IDs and use a schema-ordered changed-field bitset.
A missing or expired baseline returns `BaselineMissing`, after which the sender can use the explicit
full-snapshot encoding. `BaselineStore` is caller-owned and capacity-bounded, evicting the oldest
snapshot. Corrupt or truncated inputs are rejected without publishing a partially decoded snapshot.

`InterestManager` owns a separate interest set and resumable scan per connection. It borrows an
`IInterestProvider` only for a synchronous update; spatial indices and explicit subscriptions are
provider implementations rather than Network-owned world state. Providers inspect no more than the
requested budget, and changes become visible atomically after a complete bounded scan: entries emit
spawn semantics and departures emit despawn semantics. An entity never enters a connection's set
unless that connection's provider returned it, preventing fallback to global replication. The
manager and providers are caller-synchronized and not thread-safe.

The included loopback pair delivers in-process datagrams immediately. The deterministic simulated
pair models a UDP-oriented unreliable datagram boundary and supports seeded loss, latency, and
jitter. Both perform no socket or background-thread work. Disconnect and a newly accepted handshake
reset per-session sequencing and queued application packets so the same `Connection` objects can be
reused safely. Native UDP/DTLS/console adapters remain backend gates and must preserve this ownership
and polling contract.

The `linux-headless` configure, build, and test presets set `NEXORA_HEADLESS=ON`. This profile builds
the Foundation → Core → Network → DedicatedServer closure and its contract tests without
configuring RHI, Renderer, Runtime, presentation, Editor, or client application targets.
The Network contract suite runs headlessly and covers repeatable seeded traces, loopback delivery,
loss/latency/jitter boundaries, malformed handshakes, protocol/build mismatch rejection, and 1,000
disconnect/reconnect cycles. Its dependency check validates both the declared module closure and the
targets exposed by an actual `NEXORA_HEADLESS` configuration.
