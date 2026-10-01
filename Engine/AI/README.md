# V2-M7 AI / navigation contract

`NexoraAI` is the renderer-free V2 AI/navigation boundary. It depends only on Core and is safe to
include in headless simulation/training builds. The first V2-M7 vertical slice establishes the
contracts that keep expensive multi-agent work budgeted and keeps gameplay mutation outside AI.

`HierarchicalNavigationWorld` owns region and node topology and performs coarse region routing before
fine node search. `NavigationQueryScheduler` is the only public budgeted query path in this module:
requests are caller-owned data, one queued request per agent is coalesced by cancellation, work is
priority/age ordered, stale world generations are rejected, and `Process()` never executes more than
the configured per-tick budget. It is synchronous and caller-driven; callers may run it from their
job system, but the scheduler itself creates no threads and performs no hidden background work.

`GridCostField` is a generic non-negative spatial cost provider. Navigation can consume any
`INavigationCostProvider`, so danger, congestion, terrain preference, tactical influence, or other
game-specific fields remain outside the pathfinding contract. Construction rejects a `width`/`height`
pair whose product would overflow `size_t` (in addition to either dimension being individually zero)
by degrading to an empty field rather than undersizing the backing storage, so every subsequently
in-range `Set`/`Cost` call stays a bounds-checked no-op instead of an out-of-bounds access.

`CrowdSystem` consumes immutable agent snapshots and returns `CrowdResult` containing only
`CharacterIntent`. It never receives or mutates a character transform/controller. The reference
implementation uses an XZ spatial hash and local separation so the API is not defined around a
quadratic all-agents scan. A gameplay character layer remains responsible for applying intent.

The optional `NexoraAIIntegration` module projects AI locomotion into Runtime's horizontal
`CharacterIntent`. It is kept outside both modules to preserve AI's Core-only dependency and
Runtime's renderer-only dependency; invalid movement values, including finite inputs whose projected
velocity or planar magnitude overflows to infinity, are rejected, and the adapter never owns or
mutates a character.

`UtilityAI` emits the same `AIAction` type used by learned policies and supports deterministic
selection, cooldown eligibility, and hysteresis around the current action. `IPolicyRuntime` is
framework neutral, batch-oriented, versioned by policy ID/version, and emits `AIAction`; ONNX or
another ML backend is therefore an optional adapter rather than a second gameplay-control path.
`PolicyRuntimeDriver` accepts deferred inference, reuses a bounded-age cached batch, and falls back to
a caller-supplied common action when inference is unavailable, invalid, or too stale.

`SimulationLODPolicy` classifies full/reduced/far/dormant agents and exposes deterministic
agent-ID phase staggering. `AISimulationScheduler` turns those schedules into explicit perception,
decision, and navigation work items plus per-tick counters. Far agents therefore update at lower
frequencies instead of producing a same-frame spike, while dormant agents have zero scheduled work
until an external relevance change wakes them.

`SelfPlayBridge` is a synchronous tooling boundary over `ISelfPlayEnvironment`: reset, observations,
actions, fixed simulation step, rewards, and termination are explicit. It does not include an
engine-native trainer or ML framework dependency.

`SelfPlayBatchOrchestrator` is the first portable multi-world coordinator. It derives stable seeds
from a caller-supplied base seed and world ID (the base seed is SplitMix64-mixed before the world ID
is combined, so consecutive base seeds do not share world seeds), bounds registered worlds and worlds serviced per tick,
and uses round-robin scheduling so an active world is advanced at most once per call. Environments
and the shared policy runtime are borrowed; each world owns an independent bridge and policy-result
cache, preventing a deferred action batch from leaking between simulations. Reset/action/step
failures and callback exceptions fail only the affected world. Snapshots own their returned frame
data, and completed or failed worlds may be explicitly retired to release a slot. The coordinator is
synchronous, caller-thread-only, non-reentrant, and not thread-safe; it does not create workers,
batch inference across worlds, or provide a trainer/distributed service.

AI objects and environments are caller-owned and not thread-safe. Returned vectors own their
storage. The portable contract test includes 5,000 navigation requests with a hard 32-query tick
budget, 5,000 far-agent phase staggering, stale-query rejection, crowd intent-only output,
deterministic Utility AI, learned-policy action compatibility, influence cost sampling, single-world
self-play lifecycle, bounded/fair multi-world service, deterministic world seeds, per-world deferred
policy isolation, and failure isolation. Production NavMesh streaming, job-system adapters, policy
backends, and large-scale/distributed multi-world training remain later V2-M7 work packages.
