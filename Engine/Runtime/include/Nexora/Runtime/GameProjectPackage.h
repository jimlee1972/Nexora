#pragma once
#include "Nexora/Runtime/ProjectPackage.h"

namespace nexora::runtime {
// Persisted Editor metadata interpreted only by the explicit source-capable game package.
inline constexpr std::uint64_t kGameSpriteSourceReferenceType = 0x4544535052540001ULL;
inline constexpr std::string_view kGameSpriteSourceReferenceName = "editor.sprite.source";
[[nodiscard]] inline std::optional<AssetUuid>
ReadGameSpriteSourceReference(std::uint64_t type, std::string_view name,
                              std::span<const std::byte> data) {
  if (type != kGameSpriteSourceReferenceType || name != kGameSpriteSourceReferenceName ||
      data.size() != 17 || data[0] != std::byte{1})
    return {};
  AssetUuid id;
  for (unsigned i = 0; i < 8; ++i) {
    id.high |= std::uint64_t(std::to_integer<unsigned char>(data[1 + i])) << (i * 8);
    id.low |= std::uint64_t(std::to_integer<unsigned char>(data[9 + i])) << (i * 8);
  }
  return id == AssetUuid{} ? std::nullopt : std::optional{id};
}
inline constexpr std::size_t kMaximumGameScenes = 256;
inline constexpr std::size_t kMaximumGameScenePathBytes = 1024;
// Paths are immutable catalog locators for diagnostics/name lookup, never filesystem authority.
struct GameSceneEntry final {
  AssetUuid id;
  std::string path;
  bool operator==(const GameSceneEntry &) const = default;
};
struct GameProjectPackage final {
  AssetUuid project;
  AssetUuid start_scene;
  std::vector<GameSceneEntry> scenes;
  std::vector<RuntimeBlob> assets;
  std::vector<AssetUuid> animation_clips{};
};

// Frozen catalog and scene templates. Different templates share immutable decoded geometry/PBR
// ownership. Runtime instances must remap authored IDs when cloning into one live World.
class NEXORA_RUNTIME_API LoadedGameProject final {
public:
  [[nodiscard]] AssetUuid ProjectId() const noexcept { return project_; }
  [[nodiscard]] AssetUuid StartupScene() const noexcept { return start_scene_; }
  [[nodiscard]] std::span<const GameSceneEntry> Scenes() const noexcept { return catalog_; }
  [[nodiscard]] std::size_t AssetCount() const noexcept { return asset_count_; }
  // Structurally validated, owning source/Atlas/Clip NXABs. Optional TwoD consumers must
  // additionally validate the typed payloads before reporting admission, executing callbacks or
  // native drawing.
  [[nodiscard]] std::span<const RuntimeBlob> SpriteAssets() const noexcept { return sprites_; }
  // Canonical explicit Clip roots; the immutable catalog does not start animation implicitly.
  [[nodiscard]] std::span<const AssetUuid> AnimationClips() const noexcept { return clips_; }
  [[nodiscard]] unsigned Capability() const noexcept {
    return !clips_.empty() ? 4U : sprites_.empty() ? 2U : 3U;
  }
  [[nodiscard]] std::shared_ptr<const LoadedStaticProject> ResolveScene(AssetUuid) const;
  [[nodiscard]] std::shared_ptr<const LoadedStaticProject> ResolveScene(std::string_view) const;

private:
  friend class GameProjectResolver;
  AssetUuid project_, start_scene_;
  std::vector<GameSceneEntry> catalog_;
  std::unordered_map<AssetUuid, std::shared_ptr<const LoadedStaticProject>, AssetUuidHash> scenes_;
  std::size_t asset_count_{};
  std::vector<RuntimeBlob> sprites_;
  std::vector<AssetUuid> clips_;
};

[[nodiscard]] NEXORA_RUNTIME_API std::optional<LoadedGameProject>
LoadGameProjectPackage(const GameProjectPackage &, std::string *error = nullptr);
[[nodiscard]] NEXORA_RUNTIME_API std::optional<ByteBuffer>
EncodeGameProjectPackage(const GameProjectPackage &, std::string *error = nullptr);
[[nodiscard]] NEXORA_RUNTIME_API std::optional<LoadedGameProject>
DecodeGameProjectPackage(std::span<const std::byte>, std::string *error = nullptr);
// Explicit bounded regular package only, using the same alias-rejecting file reader as StaticView.
[[nodiscard]] NEXORA_RUNTIME_API std::optional<LoadedGameProject>
ReadGameProjectPackage(const std::filesystem::path &, std::string *error = nullptr);
} // namespace nexora::runtime
