#pragma once

#include "Nexora/Runtime/AssetPipeline.h"
#include "Nexora/Runtime/CookedSceneAssets.h"
#include "Nexora/Runtime/Runtime.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <unordered_map>

namespace nexora::runtime {

inline constexpr std::size_t kMaximumProjectPackageBytes = 256 * 1024 * 1024;
inline constexpr std::size_t kMaximumProjectPackageAssets = 4096;
inline constexpr std::size_t kMaximumProjectBlobBytes = 96 * 1024 * 1024;
inline constexpr std::size_t kMaximumProjectGeometryBytes = 128 * 1024 * 1024;

// StaticView only: no gameplay/plugin execution, native rendering, compilation or signing.
struct StaticProjectPackage final {
  AssetUuid project;
  AssetUuid scene;
  std::vector<RuntimeBlob> assets;
};

struct StaticRenderItem final {
  Id entity{};
  TransformMatrix world_transform{};
  std::shared_ptr<const CookedMesh> mesh;
  // Absent means the documented neutral material, and requires legacy shader ID zero.
  std::shared_ptr<const CookedScalarPbr> material;
};

// An isolated, owning, fully resolved static candidate. All access is read-only; resolving an
// item copies shared ownership and remains valid after this package is destroyed/replaced.
class NEXORA_RUNTIME_API LoadedStaticProject final {
public:
  [[nodiscard]] AssetUuid ProjectId() const noexcept { return project_; }
  [[nodiscard]] AssetUuid SceneAssetId() const noexcept { return scene_asset_; }
  [[nodiscard]] Id SceneId() const noexcept { return scene_; }
  [[nodiscard]] const World &WorldView() const noexcept { return world_; }
  [[nodiscard]] const CookedScene &SceneData() const noexcept { return scene_data_; }
  [[nodiscard]] std::span<const StaticRenderItem> RenderItems() const noexcept { return items_; }
  [[nodiscard]] std::size_t AssetCount() const noexcept { return asset_count_; }
  [[nodiscard]] std::size_t InactiveComponentCount() const noexcept { return inactive_components_; }
  [[nodiscard]] std::shared_ptr<const CookedMesh> ResolveMesh(std::uint64_t resource) const;

private:
  friend NEXORA_RUNTIME_API std::optional<LoadedStaticProject>
  LoadStaticProjectPackage(const StaticProjectPackage &, std::string *);
  AssetUuid project_, scene_asset_;
  Id scene_{};
  World world_{WorldKind::Play};
  CookedScene scene_data_;
  std::vector<StaticRenderItem> items_;
  std::unordered_map<std::uint64_t, std::shared_ptr<const CookedMesh>> meshes_;
  std::size_t asset_count_{}, inactive_components_{};
};

// Pure bounded validation/serialization. Failed candidates never mutate a previously loaded
// package. Canonical encoding sorts full UUIDs. Existing NXAB wire requires little-endian hosts.
[[nodiscard]] NEXORA_RUNTIME_API std::optional<LoadedStaticProject>
LoadStaticProjectPackage(const StaticProjectPackage &, std::string *error = nullptr);
[[nodiscard]] NEXORA_RUNTIME_API std::optional<ByteBuffer>
EncodeStaticProjectPackage(const StaticProjectPackage &, std::string *error = nullptr);
[[nodiscard]] NEXORA_RUNTIME_API std::optional<LoadedStaticProject>
DecodeStaticProjectPackage(std::span<const std::byte>, std::string *error = nullptr);
// Explicit input only; rejects symlinks (including parent aliases), non-regular files and files
// exceeding the package limit. Does not consult project source paths or execute code.
[[nodiscard]] NEXORA_RUNTIME_API std::optional<LoadedStaticProject>
ReadStaticProjectPackage(const std::filesystem::path &, std::string *error = nullptr);
} // namespace nexora::runtime
