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
  [[nodiscard]] std::shared_ptr<const Clip> ResolveClip(AssetId) const;
  // Admit the same identity/type inventory with unchanged Clip bytes. Reuse unchanged immutable
  // owners and texture identities; source/Atlas changes never migrate or restart playback.
  [[nodiscard]] std::optional<GameSpriteAssets>
  ReimportSources(std::span<const runtime::RuntimeBlob>, std::string *error = nullptr) const;
  // Own only immutable Clips, so a bridge does not retain superseded source/Atlas generations.
  [[nodiscard]] std::function<std::shared_ptr<const Clip>(AssetId)> ClipLookup() const;
  // Resolves authored IDs through each live instance and uses its actual World transforms.
  // Texture identities are stable within this owner; the surface must drain before replacement.
  [[nodiscard]] std::optional<SpriteFrame> Extract(const runtime::GameSceneView &,
                                                   std::uint32_t width, std::uint32_t height) const;
  // Owning live gameplay metadata uses the same immutable asset/texture admission and extractor.
  [[nodiscard]] std::optional<SpriteFrame> Extract(std::span<const SpriteInstance>,
                                                   const Camera &) const;

private:
  std::unordered_map<AssetId, std::shared_ptr<const SpriteAsset>, runtime::AssetUuidHash> sources_;
  std::unordered_map<AssetId, std::shared_ptr<const Atlas>, runtime::AssetUuidHash> atlases_;
  std::unordered_map<AssetId, std::shared_ptr<const Clip>, runtime::AssetUuidHash> clips_;
  std::unordered_map<AssetId, std::uint64_t, runtime::AssetUuidHash> textures_;
};
} // namespace nexora::twod
