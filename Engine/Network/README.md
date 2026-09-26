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
