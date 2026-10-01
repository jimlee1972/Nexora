#pragma once

#include "Nexora/Math/Math.h"
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
class NEXORA_RUNTIME_API RenderSceneSync final {
public:
  RenderSyncStatistics Sync(const World &world, renderer::GPUScene &scene,
                            const RenderResourceResolver &resolve, std::uint64_t retire_fence);
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
