# ADR-0007: Cooperative native plugin lifecycle

Status: accepted for the real host prerequisite; full ED-M6 remains open.

## Context

The real plugin host checked ABI and registered borrowed services, but unconditional native unload
could leave service pointers or plugin work referring to unmapped code. The graphical manager needs
copied diagnostics and safe disabling without assuming every existing plugin can stop.

## Decision

Keep the required engine ABI and optional registration entry point. Add an optional, independently
versioned C lifecycle getter with capacity/size/schema checks and shutdown/quiescence callbacks.
ABI inspection precedes all other calls; inspection cannot activate work. Stage bounded registration
with a weak provider identity and publish atomically. Revoke visibility in every registry copy before
shutdown, and retain no registry pointer after synchronous registration.

Owner-thread callers drain their service borrows/jobs first. Only acknowledged quiescence permits
native unload. Legacy, rejected and non-quiescent plugins remain resident until process restart,
including after host destruction. Own bounded diagnostic rows instead of exposing native handles.
Windows loads UTF-8 paths through native wide paths; POSIX keeps native bytes. The public C++ SDK
layout changes require rebuilding consumers; the C ABI additions preserve legacy admission.

## Consequences and acceptance

The contract is cooperative, serialized and nonreentrant. It provides neither automatic draining of
already borrowed services nor a sandbox, signature verification or native crash isolation. Truthful
quiescence and no activation before registration are mandatory plugin obligations. Restart pins are
bounded per host admission budget, not a process-global limit; Runtime code must survive until restart.

The ExamplePlugin implements the lifecycle. Real compiled modules exercise background work, native
unload events, copies, Unicode paths, rejection, rollback and exact service/admission bounds; a C
translation unit consumes the public header. Full Linux graphical Development and Monolithic
Shipping, including an explicit Full Shipping SDK test build, are required delivery gates. Detailed
commands/results belong in evidence. Full PluginManager and all ED milestones remain unmarked.
