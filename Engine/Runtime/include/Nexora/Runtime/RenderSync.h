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
// the matrix's largest stretch (its spectral norm), not by a per-axis scale.
[[nodiscard]] NEXORA_RUNTIME_API math::Sphere TransformBounds(const TransformMatrix &matrix,
                                                              const math::Sphere &local) noexcept;

// What a camera entity sees, as Unity's Camera does: the view follows the entity's exact world
// matrix, so a camera under a moving parent follows it, and ignores scale. Nexora is right-handed:
// the camera looks down its local -Z with local +Y up, through CameraComponent's vertical field of
// view (degrees) and clip planes, into the renderer's [0, 1] depth range. `maximum_distance` is the
// far plane. nullopt for a missing entity, one without a camera, invalid camera data (field of view
// outside (0, 180), near not positive, far not beyond near), a degenerate orientation, or an aspect
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
// The sync and the GPUScene are externally synchronized, like the GPUScene itself. A sync only
// ever touches objects it created; destroy and Release() pass `retire_fence` to GPUScene::Destroy.
// One culled scene frame: what the sync changed, what culling kept, and the submitted frame.
struct CulledSceneFrame final {
  Id camera{};
  RenderSyncStatistics sync{};
  renderer::GPUDrivenStatistics culling{};
  SceneFrameResult frame{};
};

class NEXORA_RUNTIME_API RenderSceneSync final {
public:
  RenderSyncStatistics Sync(const World &world, renderer::GPUScene &scene,
                            const RenderResourceResolver &resolve, std::uint64_t retire_fence);
  // Syncs `scene` (always), then renders it through the world's first camera (active scenes and
  // storage in order) with the frustum and distance culling of renderer::BuildGPUDrivenCommands, so
  // only the objects that camera can see are submitted. Like RenderSceneFrame, it needs a camera
  // and a light, and returns nullopt without them, with an unusable camera, or when scene rendering
  // is compiled out; a frame in which every object is culled still clears its targets. Upload
  // extraction and GPUScene::CommitFrame stay with the caller that owns the GPU buffers.
  std::optional<CulledSceneFrame>
  RenderFrame(const World &world, renderer::GPUScene &scene, const RenderResourceResolver &resolve,
              std::uint64_t retire_fence, rhi::Device &device, rhi::TextureHandle target,
              const rhi::TextureDescriptor &target_descriptor, rhi::PipelineHandle pipeline);
  // The GPU object mirroring `entity`, if any.
  [[nodiscard]] std::optional<renderer::GPUObjectHandle> Handle(Id entity) const;
  [[nodiscard]] std::size_t ObjectCount() const noexcept { return objects_.size(); }
  // Destroys every object this sync created.
  void Release(renderer::GPUScene &scene, std::uint64_t retire_fence);

private:
  struct Mirror final {
    renderer::GPUObjectHandle handle{};
    renderer::GPUObjectDescriptor descriptor{};
  };
  std::unordered_map<Id, Mirror> objects_;
};

} // namespace nexora::runtime
