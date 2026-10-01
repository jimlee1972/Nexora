#pragma once

#include "Nexora/Math/Math.h"
#include "Nexora/RHI/Device.h"
#include "Nexora/Renderer/GPUDrivenPipeline.h"
#include "Nexora/Renderer/GPUScene.h"
#include "Nexora/Runtime/Api.h"
#include "Nexora/Runtime/Runtime.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_map>

namespace nexora::runtime {

// The renderer's view of a column-major runtime matrix: math::Matrix4 is indexed (row, column),
// with the translation in column 3. Values are narrowed to float.
[[nodiscard]] NEXORA_RUNTIME_API math::Matrix4
ToRenderMatrix(const TransformMatrix &matrix) noexcept;
// A sphere that contains `local` after `matrix`, including under shear: the radius is scaled by
// the matrix's largest stretch (its spectral norm), not by a per-axis scale. It bounds what the GPU
// draws: the float matrix ToRenderMatrix uploads, evaluated in float, so neither coefficient
// narrowing nor the GPU's rounding can move a drawn point outside it. When that float evaluation
// could overflow in an intermediate sum, the radius is infinite (unbounded), which the sync
// rejects.
[[nodiscard]] NEXORA_RUNTIME_API math::Sphere TransformBounds(const TransformMatrix &matrix,
                                                              const math::Sphere &local) noexcept;

// What a camera entity sees, as Unity's Camera does: the position follows the entity's exact world
// matrix and the orientation its world rotation (the product of the chain's rotations), so a camera
// under a moving or turning parent follows it, and scale (non-uniform or negative) never skews or
// flips the view. Nexora is right-handed: the camera looks down its local -Z with local +Y up,
// through CameraComponent's vertical field of view (degrees) and clip planes, into the renderer's
// [0, 1] depth range. The far plane culls through the frustum, so `maximum_distance` is unlimited.
// nullopt for a missing entity, one without a camera, camera data that is invalid as the renderer's
// floats (field of view outside (0, 180), near not positive, far not beyond near), or an aspect
// ratio that is not positive and finite.
[[nodiscard]] NEXORA_RUNTIME_API std::optional<renderer::GPUDrivenView>
CameraView(const World &world, Id camera, float aspect);

// What a mesh renderer draws, as the renderer knows it: resource indices and the mesh's bounds in
// mesh space. Supplied by the caller, which owns asset residency.
struct RenderResourceBinding final {
  std::uint32_t mesh_resource_index{};
  std::uint32_t material_resource_index{};
  math::Sphere local_bounds{};
};
// nullopt means the mesh renderer cannot be drawn yet (for example, its asset is not resident); it
// is then kept out of the GPU scene.
using RenderResourceResolver =
    std::function<std::optional<RenderResourceBinding>(const MeshComponent &)>;

struct RenderSyncStatistics final {
  std::size_t created{};
  std::size_t updated{};
  std::size_t destroyed{};
  // Mesh renderers whose world matrix or bounds do not fit the renderer's float data (overflow), or
  // whose parent chain is cyclic because Entity::parent was corrupted by a direct write.
  std::size_t rejected{};
};

// One culled scene frame: what the sync changed, what culling kept, and the submitted frame.
struct CulledSceneFrame final {
  Id camera{};
  RenderSyncStatistics sync{};
  renderer::GPUDrivenStatistics culling{};
  SceneFrameResult frame{};
};

// Mirrors the mesh renderers of a World's active scenes into a renderer::GPUScene, one GPU object
// per entity. Each object's transform is the entity's exact world matrix (World::WorldMatrix), so a
// child moves with its parent and keeps any shear; the renderer never sees local transforms.
//
// Sync() creates objects for new mesh renderers, updates only the transform, bounds, or resources
// that changed (moving a parent therefore updates every rendered descendant), and destroys the
// objects of entities that were destroyed, lost their mesh renderer, left an active scene, or were
// rejected. Visibility and LOD belong to other systems and are left untouched after creation.
// World matrices are computed once per entity per call, so a call is linear in the entity count.
//
// Ownership: a sync owns exactly the objects it created, in the one GPUScene that holds them.
// While it owns any, it is bound to that scene's GPUScene::InstanceId (GPU handles carry no scene
// identity, and an address can be reused by a new scene): Sync, RenderFrame, and Release refuse any
// other scene, including a scene rebuilt at the same address or the same scene after Clear().
// Moving the bound GPUScene is fine; its identity moves with its contents. Release() destroys the
// objects and unbinds, so the sync can then serve another scene; when the bound scene was destroyed
// or cleared, Abandon() forgets the objects without touching any scene. Destroy and Release pass
// `retire_fence` to GPUScene::Destroy. A sync cannot be copied or move-assigned, which would
// duplicate or silently drop that ownership; moving it hands the objects over. Destroying a sync
// that still owns objects leaves them in the scene, so call Release() first. The sync and the
// GPUScene are externally synchronized, like the GPUScene itself.
class NEXORA_RUNTIME_API RenderSceneSync final {
public:
  RenderSceneSync() = default;
  RenderSceneSync(const RenderSceneSync &) = delete;
  RenderSceneSync &operator=(const RenderSceneSync &) = delete;
  // Takes over `other`'s objects and binding; `other` is left empty and unbound.
  RenderSceneSync(RenderSceneSync &&other) noexcept;
  RenderSceneSync &operator=(RenderSceneSync &&) = delete;
  ~RenderSceneSync() = default;

  // nullopt, with nothing changed, when `scene` is not the scene this sync's objects live in.
  std::optional<RenderSyncStatistics> Sync(const World &world, renderer::GPUScene &scene,
                                           const RenderResourceResolver &resolve,
                                           std::uint64_t retire_fence);
  // Syncs `scene`, then renders it through the world's first camera (active scenes and storage in
  // order) with the frustum and distance culling of renderer::BuildGPUDrivenCommands, so only the
  // objects that camera can see are submitted. Like RenderSceneFrame, it needs a camera and a
  // light, and returns nullopt without them, with an unusable camera, for a scene the sync is not
  // bound to, or when scene rendering is compiled out (the sync still runs then); a frame in which
  // every object is culled still clears its targets. Upload extraction and GPUScene::CommitFrame
  // stay with the caller that owns the GPU buffers.
  std::optional<CulledSceneFrame>
  RenderFrame(const World &world, renderer::GPUScene &scene, const RenderResourceResolver &resolve,
              std::uint64_t retire_fence, rhi::Device &device, rhi::TextureHandle target,
              const rhi::TextureDescriptor &target_descriptor, rhi::PipelineHandle pipeline);
  // The GPU object mirroring `entity`, if any.
  [[nodiscard]] std::optional<renderer::GPUObjectHandle> Handle(Id entity) const;
  [[nodiscard]] std::size_t ObjectCount() const noexcept { return objects_.size(); }
  // Destroys every object this sync created and unbinds it. false, with nothing destroyed, when
  // `scene` is not the scene those objects live in.
  bool Release(renderer::GPUScene &scene, std::uint64_t retire_fence);
  // Forgets every object and unbinds without touching any scene: for when the bound scene was
  // destroyed or cleared, so its objects no longer exist to release.
  void Abandon() noexcept;

private:
  struct Mirror final {
    renderer::GPUObjectHandle handle{};
    renderer::GPUObjectDescriptor descriptor{};
  };
  std::unordered_map<Id, Mirror> objects_;
  // GPUScene::InstanceId of the scene holding objects_, or 0 when the sync owns nothing.
  std::uint64_t scene_id_{};
};

} // namespace nexora::runtime
