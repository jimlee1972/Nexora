#pragma once

#include "Nexora/Editor/EditorWorkspace.h"

#include <bit>
#include <optional>
#include <unordered_map>

namespace nexora::editor {

// Persistent 64-bit scene resource identity, independent of path, import bytes and host size.
// Changing this derivation requires migrating saved mesh references. Zero UUID is invalid.
[[nodiscard]] constexpr std::uint64_t MeshResourceId(runtime::AssetUuid asset) noexcept {
  if (asset.high == 0 && asset.low == 0)
    return 0;
  auto value = std::rotl(asset.high, 23) ^ asset.low;
  value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
  value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
  value ^= value >> 31;
  return value == 0 ? 1 : value;
}

struct MeshAssetSnapshot final {
  runtime::AssetUuid asset;
  std::uint64_t resource{};
  std::shared_ptr<const MeshGeometry> geometry;
};

// Authoring-thread CPU catalog. Publication is atomic and does not mutate a scene or touch a GPU.
// Returned snapshots own geometry across replacement/unload. Every lookup checks project
// generation.
class NEXORA_EDITOR_API MeshAssetCatalog final {
public:
  bool Publish(std::span<const AssetEntry> assets, std::uint64_t generation,
               std::string *error = nullptr);
  void Clear() noexcept;
  [[nodiscard]] std::uint64_t Generation() const noexcept { return generation_; }
  [[nodiscard]] std::optional<MeshAssetSnapshot> ResolveAsset(runtime::AssetUuid asset,
                                                              std::uint64_t generation) const;
  [[nodiscard]] std::optional<MeshAssetSnapshot> ResolveResource(std::uint64_t resource,
                                                                 std::uint64_t generation) const;

private:
  std::uint64_t generation_{};
  std::unordered_map<std::uint64_t, MeshAssetSnapshot> meshes_;
};

} // namespace nexora::editor
