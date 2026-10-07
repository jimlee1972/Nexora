# V2-M8 portable pose storage and retarget contract

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

Objects are caller-owned, synchronous, externally synchronized, and have no threads, callbacks,
filesystem I/O, gameplay events, root-motion authority, or transform mutation. Allocations use
standard C++ exception behavior; validation failures return false/nullopt. `Bytes()` borrows
storage until successful replacement, move, or destruction. Builds/loads and view access must
be serialized with sampling. Copying owns a separate byte buffer.

`animation.v2_m8_pose_storage_retarget` checks 8,192 round-trip poses, interpolation, compression
size, canonical bytes, finite extremes, malformed/truncated input, transactional rejection,
move safety, parent mapping, bind-axis rotation, displacement ratios, and overflow rejection.
`build.animation_profiles` inspects actual target/source graphs in six profiles. Acceptance is
limited to compressed storage and explicit local retargeting. GPU skinning/sampling, motion
warping, inertialization, sync groups, editor/cook integration, and Motion Matching remain open;
V2-M8 and overall V2 completion are not claimed.
