# V2-M8 portable Pose Search contract

`NexoraPoseSearch` is an optional, renderer-free animation feature with only a Foundation
dependency. Enable it with `NEXORA_ENABLE_POSE_SEARCH`; `OFF` omits its target, implementation,
and runtime dependency. Shipping Minimal, Shipping Dedicated, and headless profiles force it off.
Shipping Full allows it in Monolithic builds. Runtime, AI, and the character motor do not link it
implicitly. This feature supplies Pose Search without requiring Motion Matching or an ML SDK.

`FeatureExtractor::Extract` provides a translation-only reference layout: selected joints'
root-relative XYZ positions, their finite-difference XYZ velocities, then caller-provided
root-relative XYZ trajectory points. The first selected joint is the root. Positions and previous
positions use the same caller-defined axes and joint order; trajectory points already use that
root-relative frame. Features use the caller's units. Joint selection, orientation transforms,
trajectory horizon, feature weights, and matching schema versions are authoring responsibilities.
This extractor does not sample clips, retarget skeletons, or extract rotations.

`PoseDatabase::Build` copies a versioned weighted schema and finite feature samples, validates the
whole input, canonicalizes signed zero, and sorts by `(clip, time)`. A clip ID must be nonzero;
times are finite and nonnegative, and duplicate clip/time keys are rejected. Dimensions must agree;
weights are finite and nonnegative with at least one positive weight. Limits are 256 dimensions
and 65,536 samples. Empty or malformed builds return false without changing the previous database.
Allocation failures use standard C++ exceptions. Successful rebuilds replace all storage.

The fingerprint uses canonical encoding version 1: little-endian 32-bit encoding version,
schema version, dimension count, IEEE-754 float32 weights, sample count, then for each sorted sample
a 64-bit clip ID, float32 time, 64-bit tag mask, and float32 features. FNV-1a-64 identifies these
bytes, including constraints and weights; it is a reproducibility/debug identity, not a security
digest or serialized asset format. Reordered input and signed-zero changes have identical output.

`Search` validates the query schema version, dimensions, and finite values; requires all specified
tags and rejects any forbidden tags. It performs an exact exhaustive weighted squared-distance
search with double intermediates. Exact ties select the first canonical clip/time key. A query
with incompatible data, conflicting constraints, no eligible samples, or an unbuilt database
returns `nullopt`. Results own clip/time references, cost, tags, and database identity. Searches
never publish a partially scanned result, and take O(samples × dimensions); callers must budget
query frequency. Zero weights exclude dimensions from the distance, but not input validation.

Objects are caller-owned, synchronous, and do not create threads, jobs, filesystem I/O, GPU work,
or gameplay events. `Samples()` borrows storage until a successful build or destruction; serialize
builds with searches and view access. This API does not mutate transforms or choose gameplay events.
The C++ feature boundary is not a new stable C ABI; external gameplay/plugin integration must use
the existing bridge contracts.

`animation.v2_m8_pose_search` verifies canonical fingerprints, input-order independence, tag/weight
selection, transactional rejection, finite extremes, feature extraction, and maximum-capacity
search against an independent reference. `build.pose_search_profiles` checks CMake File API target
and source graphs across six profile configurations, including the Foundation-only dependency
and Modular/Monolithic linkage. These gates accept only this portable Pose Search slice. Compute
skinning, GPU pose sampling, motion warping, inertialization, sync
group authoring/TRS integration, editor visualization, cooked database loading, and optional
Motion Matching remain open;
V2-M8 and total V2 progress remain unaccepted/46%.

Portable compressed pose storage and explicit local bind-space retargeting are delivered separately
by [NexoraAnimation](../Animation/README.md), without introducing a dependency on PoseSearch.
