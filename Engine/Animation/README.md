# V2-M8 portable animation contract

`NexoraAnimation` is optional (`NEXORA_ENABLE_ANIMATION_V2`) and depends only on Foundation.
OFF removes its target and source; Shipping Minimal, Shipping Dedicated, and headless profiles
force OFF. Shipping Full supports Monolithic linkage. Runtime, AI, Renderer, and PoseSearch do
not link it implicitly. External gameplay integrations must use the existing stable C ABI bridge;
this C++ API is not a new plugin ABI.

`AnimationPoseStorage::Build` copies frame-major local TRS poses with a fixed joint order. It
requires 1–512 joints, 1–4096 frames, at most 1,048,576 samples, finite components, positive scale,
and nonzero quaternions. It normalizes rotation using double intermediates and chooses a stable
quaternion hemisphere; equivalent q/-q and signed zero produce identical bytes. Translation and
scale use per-joint min/max bounds with unsigned 16-bit quantization; normalized rotations use
four signed 16-bit coefficients. The per-sample payload is 20 bytes versus 40 bytes for float TRS;
bounds cost 48 bytes per joint, so short clips can cost more than uncompressed storage. Uniform
sample timing and skeleton identity belong to the caller's asset metadata. Scale cannot reflect
or collapse a joint. This is a CPU representation; no GPU upload or shader format is implied.

The serialized format is explicitly little endian, without native struct padding: a 16-byte
header (`NPS1`, uint32 format version 1, uint32 joint count, uint32 frame count), then 12 float32
bounds per joint (min XYZ translation/scale, max XYZ translation/scale), then frame-major samples
(3 uint16 translation, 4 int16 XYZW quaternion scaled by 32767, 3 uint16 scale). Bounds are
IEEE-754 binary32. `Load` validates version, budget, exact size, finite ordered bounds, positive
scale bounds, and near-unit quantized quaternion norms before publishing owned storage. This
format has no security digest; bundle verification remains an outer boundary.

`Sample(frame)` takes a finite fractional frame in `[0, FrameCount()-1]`; it does not wrap or
clamp invalid input. Translation and scale interpolate linearly with double intermediates,
and rotation uses shortest-arc slerp. Quantization error for each scalar channel is bounded by
half its range / 65535 plus float rounding; decoded rotations are normalized. Sampling returns
owned poses. Failed builds and loads preserve previous data, including when loading a borrowed
view of the object's own bytes. Moved-from storage is empty and safely reusable.

`PoseRetargeter` owns source/target local bind poses and explicit one-to-one joint mappings.
Parent arrays must be topologically ordered (`-1` root, otherwise an earlier joint); each mapped
child requires its source parent to map to its target parent, and roots map to roots. Unmapped
target joints retain their target bind pose. Mappings are sorted by target index, so input order
does not alter application. The rotation alignment is target bind rotation × inverse source
bind rotation. Apply aligns source local translation displacement into target bind axes, scales
it by an explicit positive translation ratio, applies the source rotational displacement, and
scales by the source/target bind-scale ratio. Source and target use the same units and axis
convention; this local mapping supports authored compatible hierarchies. Different topology,
IK/end-effector preservation, automatic name matching, twist distribution, and animation graph
integration require separate adapters. Invalid or overflowing poses fail without a partial result.

`SyncGroup` owns up to 256 looping clip clocks, each with up to 256 optional named markers.
`SetMembers` copies and validates the complete input before replacement: nonzero unique clip IDs,
finite positive durations, times in `[0, duration)`, finite nonnegative weights, and either zero
markers or at least two. Marker times are strictly increasing in that same interval; names are
nonempty and unique within a clip. Invalid replacement leaves the previous group unchanged.
Membership replacement resets clocks to the supplied times; weight updates preserve clocks.

`Update(seconds)` advances the highest-weight member using its own duration. Equal weights select
the lowest clip ID; zero-weight members still receive follower samples, while an all-zero group
returns `nullopt` without advancing. Ticks are finite and nonnegative. Large finite deltas are
reduced before addition to avoid overflow. Output is an owning vector sorted by clip ID. Compatible
marker layouts have the same unique names in the same cyclic order, allowing a different first
marker. Followers interpolate between the leader's bounding markers, including the wrap segment.
Missing or incompatible layouts use normalized loop phase, reported by `marker_synced=false`.
Only followers report marker synchronization. Leadership changes start from the new leader's last
synchronized clock; they do not reset its phase. No previous clip is sampled for synchronization.
Clock math uses doubles; repeatability assumes the same floating-point environment, not bitwise
replay across arbitrary CPUs. Exact marker boundaries use the outgoing segment.
Updates perform O(members × markers) matching and allocate the output plus a leader metadata copy;
callers budget this synchronous work. Validation also checks duplicate names pairwise. Marker name
byte length is caller-owned authoring policy; the limits bound counts, not total string bytes.

The group produces visual sample times only. Callers own clip metadata, group membership, weight
policy, blend evaluation, and event/root-motion authority. It never loads clips, mutates transforms,
or dispatches events. Runtime does not link Animation implicitly: a consumer explicitly links both,
selects a clip with `AnimationGraph::Play`, then passes its synchronized time to
`AnimationGraph::Synchronize` and samples with `Update(0)`. The graph uses float times; consumers
must convert into its valid interval (rounding to duration requires wrapping to zero). Non-looping
clips are outside this group contract. Marker authoring/editor tools and compressed TRS graph
integration remain future work.

Objects are caller-owned, synchronous, externally synchronized, and have no threads, callbacks,
filesystem I/O, gameplay events, root-motion authority, or transform mutation. Allocations use
standard C++ exception behavior; validation failures return false/nullopt. `Bytes()` borrows
storage until successful replacement, move, or destruction. Builds/loads and view access must
be serialized with sampling. Copying owns a separate byte buffer.

`animation.v2_m8_pose_storage_retarget` checks 8,192 round-trip poses, interpolation, compression
size, canonical bytes, finite extremes, malformed/truncated input, transactional rejection,
move safety, parent mapping, bind-axis rotation, displacement ratios, and overflow rejection.
`animation.v2_m8_marker_sync` verifies marker/wrap interpolation, cyclic layouts, normalized
fallback, stable leadership, weight switches, transactional rejection, tick partitioning, bounded
capacity, and extreme finite clocks. `animation.v2_m8_sync_group_graph` drives two Runtime graphs
through wrap and leader switches while checking visual poses and zero seek root motion.
`build.animation_profiles` inspects both implementation sources
and actual target/source graphs in six profiles. Acceptance is limited to compressed storage,
explicit local retargeting, and portable marker synchronization. GPU skinning/sampling, motion
warping, inertialization, marker authoring/editor/cook integration, and Motion Matching remain open;
V2-M8 and overall V2 completion are not claimed. See [marker synchronization acceptance](../../Tests/Animation/sync-group-acceptance.md).
