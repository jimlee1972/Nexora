#pragma once
#include "Nexora/Runtime/ProjectPackage.h"

namespace nexora::runtime {
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
};

// Frozen catalog and scene templates. Different templates share immutable decoded geometry/PBR
// ownership. Runtime instances must remap authored IDs when cloning into one live World.
class NEXORA_RUNTIME_API LoadedGameProject final {
public:
  [[nodiscard]] AssetUuid ProjectId() const noexcept { return project_; }
  [[nodiscard]] AssetUuid StartupScene() const noexcept { return start_scene_; }
  [[nodiscard]] std::span<const GameSceneEntry> Scenes() const noexcept { return catalog_; }
  [[nodiscard]] std::size_t AssetCount() const noexcept { return asset_count_; }
  [[nodiscard]] std::shared_ptr<const LoadedStaticProject> ResolveScene(AssetUuid) const;
  [[nodiscard]] std::shared_ptr<const LoadedStaticProject> ResolveScene(std::string_view) const;

private:
  friend class GameProjectResolver;
  AssetUuid project_, start_scene_;
  std::vector<GameSceneEntry> catalog_;
  std::unordered_map<AssetUuid, std::shared_ptr<const LoadedStaticProject>, AssetUuidHash> scenes_;
  std::size_t asset_count_{};
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
