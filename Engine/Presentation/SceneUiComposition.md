# Offscreen Scene composition in UI

`SceneDrawData::sampledByUi` explicitly prepares one offscreen LDR Scene for the acquired frame's
single `RenderUi` submission. The default is false: ordinary scenes, UI texture IDs, HDR composition
and existing exclusive workflows keep their existing behavior. This is a native compositor
boundary; packaged Editor source admission and gameplay reload remain embedding responsibilities.

## Producer and consumer

Set `offscreen = true` and `sampledByUi = true` before `DrawScene`. The producer uses the existing
full-surface offscreen target, with the default viewport. A direct or HDR sampled request rejects.
After a successful draw, `SceneTexture()` returns an opaque `SceneTextureReference` containing a
surface domain, acquired-frame serial and view identity. The value owns no renderer pointer or
image. It is available only on the render owner thread before the frame's UI submission.

An indexed UI command consumes that reference through `sceneTexture`, with `textureId = 0` and
zero instance fields. Its vertices supply the destination rectangle and UVs; UV0..1 selects the
complete Scene image. A caller preparing a camera projection chooses the destination aspect.
Place the image command before Sprite/HUD/window commands in their owning UI draw list. The
normal GUI ordering then preserves translucent quads and overlapping windows in the one UI pass.

```cpp
scene.offscreen = true;
scene.sampledByUi = true;
// Check DrawScene == Ready, then require a SceneTexture value before publishing host state.
const auto reference = surface.SceneTexture();
imageCommand.textureId = 0;
imageCommand.sceneTexture = *reference;
// RenderUi receives the complete indexed Scene image, Sprite and GUI packet once.
```

All-zero `sceneTexture` selects the ordinary `textureId` path. No additional integer texture IDs
are reserved. Existing C/Gameplay/Zig V1 command layouts are unchanged; the public C++ structs and
optional surface method require C++ consumers to rebuild. Sprite preparation and FP16 TwoD linear
submission explicitly reject typed Scene references, including malformed references paired with
ordinary texture IDs. They do not silently reinterpret these values as ordinary textures.

## Admission and lifetime

All operations run on the surface/render owner thread between Acquire and Present. Every typed
reference must match the prepared surface, frame and view. Foreign, stale, partially initialized,
unprepared, instanced, out-of-range, nonfinite or invalid-clip commands reject before the UI call
uploads textures or records draws. A bad packet leaves the prepared Scene available for a valid
packet. The normal duplicate-UI rejection remains in force. Frame completion, resize, surface
replacement and drain invalidate references; never serialize them or retain them for another frame.

The Scene target, sampler descriptor and UI vertex/index uploads belong to fenced native frame
resources. Borrowed CPU spans expire after their calls return. Vulkan transitions the offscreen
attachment from color-write to shader-read before the UI pass. DX12 records the corresponding
resource transition and retains the sampled target/cache through a later direct view. Metal's
encoder samples the owned texture with its normal command-buffer resource lifetime. Submitted
frame waits protect reuse, resizing and destruction. No gameplay module owns GPU references.

Sampling completes that offscreen Scene once; `sceneComposites` increments once for the prepared
view, while `nativeUiDrawCalls` counts its actual indexed image and other UI commands separately.
Multiple image commands may reference the same prepared Scene in the one packet.

An opt-in sampled LDR/non-shadow view conservatively reserves the union of its image commands'
clip rectangles. One disjoint explicit direct LDR/non-shadow Scene may follow UI using the existing
second view upload slot; overlapping and third views reject. Ordinary offscreen/full-surface and
shadow/HDR workflows retain their exclusive admission rules. A wider GUI clip can conservatively
block a second view even when its image triangles cover less area.

## Embedding publication

Prepare the complete unpublished World, immutable assets, Scene target/reference and every
Sprite/Atlas admission before committing a host candidate. Native failure follows the surface
recovery policy; CPU failure keeps the prior World and owners. This API does not by itself make
Editor publication transactional. In particular, it does not remove the configured source-package
Play guard. That parent workflow still requires actual mixed Scene/source pixels, scene remapping,
Play lifecycle and retirement acceptance.
