# Network contract

`NexoraNetwork` is the portable V2-M5 transport boundary. It depends only on Core and therefore can
be linked by a headless server without Renderer, RHI, Presentation, UI, audio, GPU assets, or Editor.

`INetTransport` owns datagram delivery and is advanced and polled synchronously by its caller.
`Connection` borrows its transport for its whole lifetime; neither object is thread-safe. Handshake
traffic validates both protocol version and build ID before user packets are accepted. Rejection,
disconnect, channel semantics, and sequencing are explicit state rather than transport exceptions.

The included deterministic transport pair models a UDP-oriented unreliable datagram boundary and
supports seeded loss, latency, and jitter. It performs no socket or background-thread work; native
UDP/DTLS/console adapters remain backend gates and must preserve this ownership and polling contract.

The `linux-headless` configure, build, and test presets set `NEXORA_HEADLESS=ON`. This profile builds
the Foundation → Core → Network → DedicatedServer closure and its contract tests without
configuring RHI, Renderer, Runtime, presentation, Editor, or client application targets.
