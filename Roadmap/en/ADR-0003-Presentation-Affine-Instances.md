# ADR-0003: Exact affine Presentation instances

Status: Accepted for implementation. Date: 2026-10-04.

## Context

A rotated child below nonuniform scale can have an affine world matrix that a TRS instance cannot
represent. Presentation previously uploaded the public TRS descriptor directly; native previews
therefore could not preserve exact geometry or inverse-transpose normals through such ancestry.
Runtime already provides exact column-major world matrices, and SceneDocument owns matching
prospective matrices. Presentation must remain independent of Runtime/Editor and native handles.

## Decision

Append optional `SceneInstance::model_transform`, a row-major float 4x4 affine override. When set,
it owns the transform and unused TRS fields are ignored. Tint remains required and finite. Require
finite matrix values, canonical last row `[0, 0, 0, 1]`, an invertible linear part, and derived normal
coefficients within finite float range. Keep the existing TRS validation when absent, empty-instance
identity, bounded mesh ranges, one-scene-submission rule and completion ownership. Classify the
binary32 determinant with error-free binary64 product/sum expansions, so cancellation cannot
accept singular matrices or reject an exactly invertible one.

One common CPU packer validates and creates private 112-byte records: three float4 model rows,
three padded float4 inverse-transpose normal rows, and float4 tint. Vulkan, DX12 and Metal upload
these records, never the optional public descriptor. Vertex shaders use model-row dot products and
normal-row dot products; MVP, lighting and UV/material binding keep their existing conventions.
Normal rows may share a positive rescale, preserving direction while bounding GPU arithmetic.
Shaders normalize finite input/output normals and light vectors with magnitude scaling, including
zero normals (ambient only), to avoid overflow or underflow during length calculation.
Source spans are borrowed during DrawScene only; the backend copies packed bytes into the acquired
frame's fence-owned storage before returning. Reject malformed input before GPU allocation/recording
without consuming the scene submission. `ValidateSceneInstance` exposes the same pure CPU gate.

## Consequences and evidence

Existing aggregate callers retain identity/TRS behavior. C++ consumers rebuild for the appended
field; stable C/Zig ABI, scene formats, module graph and native handle ownership are unchanged.
Editor callers must transpose owning Runtime column-major matrices explicitly before submission;
this boundary change does not itself connect Scene/Game authored meshes or complete ED-M2.

Portable tests check closed-form points, normal/tangent orthogonality, mirrored transforms,
invalid affine data, override semantics, legacy TRS and upload budgets. Vulkan X11 pixels compare
an affine instance to independently baked geometry/normals and test invalid-then-valid recovery.
Run full Linux Development and Shipping gates, deterministic shader regeneration with the same
compiler, and cross-platform CI. Physical-GPU/display and native editor acceptance remain separate.
