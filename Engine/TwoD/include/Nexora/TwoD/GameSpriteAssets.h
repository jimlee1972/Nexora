#pragma once
#include "Nexora/Runtime/GameSceneSession.h"
#include "Nexora/TwoD/Scene.h"

namespace nexora::twod {
// Optional typed admission for Runtime's structurally validated game Sprite assets. Immutable
// owning CPU data only; no source paths, World mutation, callbacks or native resources.
class NEXORA_2D_API GameSpriteAssets final {
public:
  [[nodiscard]] static std::optional<GameSpriteAssets> Load(std::span<const runtime::RuntimeBlob>,
                                                            std::string *error = nullptr);
  [[nodiscard]] std::shared_ptr<const SpriteAsset> Source(AssetId) const;
  [[nodiscard]] std::shared_ptr<const Atlas> ResolveAtlas(AssetId) const;
  // Resolves authored IDs through each live instance and uses its actual World transforms.
  // Texture identities are stable within this owner; the surface must drain before replacement.
  [[nodiscard]] std::optional<SpriteFrame> Extract(const runtime::GameSceneView &,
                                                   std::uint32_t width, std::uint32_t height) const;

private:
  std::unordered_map<AssetId, std::shared_ptr<const SpriteAsset>, runtime::AssetUuidHash> sources_;
  std::unordered_map<AssetId, std::shared_ptr<const Atlas>, runtime::AssetUuidHash> atlases_;
  std::unordered_map<AssetId, std::uint64_t, runtime::AssetUuidHash> textures_;
};
} // namespace nexora::twod
