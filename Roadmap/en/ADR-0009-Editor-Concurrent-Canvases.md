# ADR-0009: Bounded concurrent Editor Scene and Game canvases

Status: Accepted for bounded implementation; validation in progress. Date: 2026-10-14.

## Context

Scene and Game currently share one native scene submission per acquired frame. Showing Scene
therefore replaces the Game preview with an inspection map. Removing only the admission guard is
unsafe: the second draw can overwrite first-draw instance/material/geometry uploads, rewrite Vulkan
descriptors, or release targets referenced by an unsubmitted command buffer.

## Decision

Add default-false `ISurface::SupportsBoundedSceneViews()`, forwarded by `RenderSurface`. A supporting
native adapter may admit at most two explicit, disjoint direct LDR/non-shadow viewports after the
single UI submission. Touching edges are allowed. Full-surface implicit, offscreen, HDR, shadow and
before-UI workflows retain one scene submission. Geometry/material/texture validation remains
mandatory; a rejected descriptor, overlap or third view consumes no admission. Commit admission
only after recording a successful draw, and reset it at the next successful Acquire.

Editor uses the capability to publish both native canvases when their current physical rectangles
are disjoint. Unsupported or overlapping arrangements retain a visible fallback. Both views use
current owning frame geometry; Game still uses its isolated Play clone and frozen CPU assets.
No second UI pass, readback/composite fallback or backend handle crosses the application boundary.

## Ownership and bounds

Each protecting-frame slot has at most two independently owned scene uploads and geometry-prefix
keys. Vulkan additionally owns each view's descriptors, depth/framebuffer and optional scene-target
state, because direct target recreation must not destroy the first unsubmitted view. Its PBR pool
is bounded to two sets of the existing 64-material budget per frame slot. DX12 and Metal retain
existing serial depth ownership where clearing the next disjoint view is safe; mutable scene
uploads remain separate. Advanced offscreen/HDR/shadow resources continue using the exclusive path.
Second-view storage is allocated lazily. Existing per-view geometry/instance/material limits apply;
this is not a total application/native-memory or physical GPU budget claim.

The existing immutable geometry cache leases all admitted ranges until the next frame, across
both draws. Eviction cannot invalidate the first view while the second is being recorded.
All view uploads/keys/targets are drained and released on the same owner/fence boundaries as before;
resize/recovery and abandoned recordings must account for both slots. GPU timing remains one
command-buffer interval including UI and both views, never an invented per-view measurement.

## Compatibility and gates

Public C++ consumers rebuild for the additive capability. Default-false surfaces preserve their
existing fallback. Stable C/Zig layouts, asset/resource identities, source formats, shaders and
module dependencies do not change. This supporting slice does not close ED-M3 or accept HDR,
floating-window occlusion, arbitrary viewport counts or physical display/GPU/DPI workflows.

Required evidence: portable admission boundaries/overflow/mode/reset, meaningful old-guard and
upload-alias regressions, distinct native PBR/geometry/UI pixels on Vulkan/DX12/Metal, repeated
frames/resize/drain ownership, and actual simultaneous Editor Scene/Game with Play motion,
pause/step/stop and unchanged authoring source/Undo. Run complete Linux Development and Shipping,
then every applicable final-head hosted CI job. Current validation is still in progress.
