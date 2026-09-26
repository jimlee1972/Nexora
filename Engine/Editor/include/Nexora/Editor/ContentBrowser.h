#pragma once

#include "Nexora/Editor/Api.h"
#include "Nexora/Runtime/AssetPipeline.h"

#include <chrono>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace nexora::editor {

enum class ThumbnailState { Loading, Ready, Failed };

struct ContentItem final {
  runtime::AssetUuid id;
  std::filesystem::path path;
  std::string type;
  std::string artifact_hash;
  ThumbnailState thumbnail{ThumbnailState::Loading};
};

struct Breadcrumb final {
  std::string label;
  std::filesystem::path path;
};

class NEXORA_EDITOR_API ContentBrowserModel final {
public:
  explicit ContentBrowserModel(std::uint64_t project_generation = 1);
  bool Reset(std::span<const ContentItem> items, std::uint64_t project_generation);
  bool SetFolder(const std::filesystem::path &folder);
  void SetFilter(std::string query, std::string type = {});
  [[nodiscard]] std::vector<const ContentItem *> Visible(std::size_t offset,
                                                         std::size_t count) const;
  [[nodiscard]] std::span<const Breadcrumb> Breadcrumbs() const noexcept { return breadcrumbs_; }
  [[nodiscard]] std::uint64_t ProjectGeneration() const noexcept { return generation_; }
  [[nodiscard]] const ContentItem *Find(runtime::AssetUuid id) const;

  bool Select(runtime::AssetUuid id, bool additive = false);
  bool Toggle(runtime::AssetUuid id);
  void ClearSelection() noexcept { selection_.clear(); }
  [[nodiscard]] bool IsSelected(runtime::AssetUuid id) const;
  [[nodiscard]] std::vector<runtime::AssetUuid> Selection() const;

  bool Rename(runtime::AssetUuid id, std::string_view filename, std::string *error = nullptr);
  bool Move(std::span<const runtime::AssetUuid> ids, const std::filesystem::path &folder,
            std::string *error = nullptr);
  bool Delete(std::span<const runtime::AssetUuid> ids, std::string *error = nullptr);
  bool Undo();

private:
  bool Commit(std::vector<ContentItem> next, std::string *error);
  [[nodiscard]] bool ValidDestination(const std::filesystem::path &path,
                                      runtime::AssetUuid except = {}) const;
  std::vector<ContentItem> items_;
  std::vector<ContentItem> undo_;
  std::unordered_set<runtime::AssetUuid, runtime::AssetUuidHash> selection_;
  std::filesystem::path folder_;
  std::string query_, type_;
  std::vector<Breadcrumb> breadcrumbs_;
  std::uint64_t generation_{};
};

struct AssetDragPayload final {
  static constexpr std::string_view kType = "NEXORA_ASSET_UUID";
  std::string type{std::string(kType)};
  std::uint64_t project_generation{};
  runtime::AssetUuid asset;
};

enum class DragValidation { Valid, WrongType, StaleProject, MissingAsset, InvalidTarget, ReadOnly };
NEXORA_EDITOR_API DragValidation ValidateDrag(const AssetDragPayload &payload,
                                              const ContentBrowserModel &model,
                                              const std::filesystem::path &target,
                                              bool writable) noexcept;

class NEXORA_EDITOR_API AssetDependencyGraph final {
public:
  bool Set(runtime::AssetUuid asset, std::span<const runtime::AssetUuid> dependencies);
  [[nodiscard]] std::vector<runtime::AssetUuid> Forward(runtime::AssetUuid asset) const;
  [[nodiscard]] std::vector<runtime::AssetUuid> Reverse(runtime::AssetUuid asset) const;
  [[nodiscard]] std::vector<runtime::AssetUuid> FindCycle() const;

private:
  std::unordered_map<runtime::AssetUuid, std::vector<runtime::AssetUuid>, runtime::AssetUuidHash>
      edges_;
};

struct ReimportResult final {
  std::uint64_t project_generation{};
  runtime::AssetUuid asset;
  std::string source_hash;
  std::string settings_hash;
  std::string artifact_hash;
  std::vector<runtime::AssetUuid> dependencies;
  std::string diagnostic;
  bool cancelled{};
};

class NEXORA_EDITOR_API ReimportTransaction final {
public:
  ReimportTransaction(std::uint64_t generation, runtime::AssetUuid asset,
                      std::string previous_artifact);
  bool Stage(ReimportResult result);
  bool Commit(std::uint64_t current_generation, AssetDependencyGraph &graph);
  void Cancel() noexcept { staged_.reset(); }
  [[nodiscard]] std::string_view Artifact() const noexcept { return artifact_; }
  [[nodiscard]] std::string_view Diagnostic() const noexcept { return diagnostic_; }

private:
  std::uint64_t generation_{};
  runtime::AssetUuid asset_;
  std::string artifact_, diagnostic_;
  std::optional<ReimportResult> staged_;
};

struct FileEvent final {
  std::filesystem::path path;
  std::chrono::steady_clock::time_point observed;
  bool self_write{};
};

class NEXORA_EDITOR_API WatcherDebouncer final {
public:
  explicit WatcherDebouncer(std::chrono::milliseconds delay) : delay_(delay) {}
  void Push(FileEvent event);
  [[nodiscard]] std::vector<std::filesystem::path> Flush(std::chrono::steady_clock::time_point now);

private:
  std::chrono::milliseconds delay_;
  std::unordered_map<std::string, FileEvent> pending_;
};

enum class DirtyConflictChoice { Pending, Reload, Keep, Compare };
struct DirtyConflict final {
  runtime::AssetUuid asset;
  std::string editor_hash;
  std::string disk_hash;
  DirtyConflictChoice choice{DirtyConflictChoice::Pending};
};

class NEXORA_EDITOR_API DirtyConflictModel final {
public:
  bool Detect(runtime::AssetUuid asset, std::string editor_hash, std::string disk_hash, bool dirty);
  bool Resolve(runtime::AssetUuid asset, DirtyConflictChoice choice);
  [[nodiscard]] const DirtyConflict *Find(runtime::AssetUuid asset) const;
  [[nodiscard]] std::span<const DirtyConflict> Conflicts() const noexcept { return conflicts_; }

private:
  std::vector<DirtyConflict> conflicts_;
};

} // namespace nexora::editor
