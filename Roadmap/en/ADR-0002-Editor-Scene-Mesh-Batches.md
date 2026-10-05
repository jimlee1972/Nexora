# ADR-0002: Native Scene mesh batches

Status: Accepted for implementation. Date: 2026-10-04.

## Context

The Editor proxy preview currently submits one mesh with many transform instances. Authored scenes
need multiple geometries in one depth-tested viewport. RenderSurface owns the native presentation
pass, uploads, and completion fences; Editor must not acquire backend handles or build a second pass.

## Decision

Extend SceneDrawData with an optional borrowed span of SceneMeshBatch records. Each record selects
an index range in the shared uint16 index upload and an instance range in the shared instance upload.
Indices address the complete shared vertex upload. Empty batches retain the existing full-mesh,
all-instances draw, and an empty instance span still means one identity instance.

Vulkan and DX12 validate every range before allocating or recording, upload the arrays once, and
issue the selected indexed draws inside the existing scene/depth pass. Limit batches and instances
to 4,096, vertices to 65,535, and indices to 1,048,576. Reject zero counts, incomplete triangles,
unaligned triangle starts, out-of-range spans, and overflow without consuming the scene submission.
Overlapping ranges are allowed for submeshes and repeated geometry. MVP, lighting, texture, and base
material remain shared for the submission; transform and tint remain per instance.

The spans are borrowed only during DrawScene; the frame fence protects the copied GPU uploads
after the call returns. Resize, recovery,
offscreen composition, clipping, UI ordering, and one-scene-submission-per-frame rules are unchanged.
sceneDrawCalls counts accepted submissions and sceneInstances counts uploaded instance records.

## Consequences and gates

Existing callers are source compatible. All consumers must rebuild the C++ descriptor; there is no
new serialized scene format, stable C ABI field, module dependency, or shader input. Unsupported
backends continue to report Unsupported. Future authored-mesh residency and per-material batching
remain separate features; the extension itself does not complete ED-M2.

Validate range boundaries and uint32 overflow in portable contracts, and Vulkan X11 pixels with
distinct geometry/instance ranges and repeated invalid descriptors before a successful draw. Run
full Linux Development and Shipping gates; CI compiles DX12 on Windows. This cloud execution does
not establish physical-display or physical-GPU acceptance.

Vulkan retains bounded upload capacity per fence-protected frame slot, copying fresh submission
bytes without reallocating at the same or smaller size. Growth stages replacement before releasing
old storage; resize/teardown drains and releases every slot. This does not add per-asset residency or
change source-span lifetime. Native call-tracing/pixel tests cover allocation failure and ownership.
