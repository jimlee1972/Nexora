# ADR-0010: Optional copied Play pointer input

Status: Accepted for implementation; acceptance gates below apply. Date: 2026-10-14.

## Context

The original V3 input snapshot carries movement and action buttons. It cannot transport pointer
motion or wheel input without changing the meaning/layout used by existing modules. Editor
already owns native Window batches, capture/release admission and one selected logical user.

## Decision

Append optional `capture_pointer_input` after the existing 2D callback in the V3 host. Advertise
a distinct capability only for an implemented provider. Keep the original required V3 prefix,
legacy input snapshot and 2D extension intact. Consumers check size before reading a suffix,
then version, capability and callback. An extension checks the end of its own field, never the
size of future complete host tables. C and Zig convenience guards preserve outputs when absent.

`NexoraPointerInputSnapshotV1` has a caller-sized/versioned prefix, the existing batch sequence,
signed double client-pixel X/Y deltas, horizontal/vertical Window wheel units and five raw held
button bits (left/right/middle/back/forward). These raw bits differ from legacy profile action
bits. No native pointer, ownership transfer or asynchronous work crosses the ABI. The caller
supplies valid aligned storage; errors preserve its bytes. A larger future output buffer keeps
its unknown suffix. Provider context outlives module Stop/Destroy.

Editor sums each batch with finite saturation: motion is bounded to +/-65,536 native client
pixels per axis and wheel to +/-122,880 units per axis (1,024 120-unit steps). Reads do not consume
the result; repeated Fixed/Update reads retain sequence/data until an owner input/admission mutation. Consumers deduplicate by sequence
when applying frame displacement; Update must also cover batches without a fixed tick. An empty next batch clears deltas/wheel while preserving
held buttons. Sequence advances with the legacy snapshot, including neutral batches.

Capture acquisition discards actions/motion/wheel but may seed its last pointer position. With
no seed, the first sample establishes an anchor. Blur/Escape wins over every event in the batch.
Focus/user/profile replacement and Stop reset motion, wheel and anchor. Resize/DPI/display
changes suppress pointer displacement/wheel for that batch and re-anchor; ordinary key/button
release processing continues. Unselected logical users get owning neutral snapshots with the
same sequence. Pause/manual Step admits no pointer input; Resume requires reacquisition.

## Compatibility and gates

The host suffix and wire are additive; no ABI major, legacy prefix, scene format, shader or
module dependency changes. C++ consumers rebuild. Providers that do not implement pointer input
leave the callback null and capability unset. Old 2D-only host sizes must still negotiate 2D.

Required gates include C11/C++/real Zig layout and size guards, short original/2D host buffers,
descriptor/error preservation, 32 input lifecycles with arithmetic/reset/user/focus boundaries,
and an actual C-ABI module controlling native Game pixels from captured pointer/wheel input.
Complete Linux Development/Shipping and all final-head hosted CI gate merge.

This is bounded native-client motion. Cursor locking, raw/unbounded relative input, application
camera sensitivity/inversion, physical devices, gamepads and full pointer-look/ED-M3 acceptance
remain separate.
