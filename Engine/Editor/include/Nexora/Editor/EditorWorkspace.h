#pragma once

#include "Nexora/Editor/Api.h"
#include "Nexora/Runtime/AssetPipeline.h"
#include "Nexora/Runtime/EditorSdk.h"

#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nexora::editor {

struct PanelDescriptor final {
  std::string_view id;
  std::string_view title;
};

class NEXORA_EDITOR_API ProductShell final {
public:
  [[nodiscard]] static std::span<const PanelDescriptor> Panels() noexcept;
  [[nodiscard]] static bool IsStablePanelId(std::string_view id) noexcept;
  bool RouteCommand(std::string command);
  [[nodiscard]] std::string_view LastCommand() const noexcept { return last_command_; }

private:
  std::string last_command_;
};

struct ProjectDescriptor final {
  static constexpr std::uint32_t kSchemaVersion = 1;
  std::string name;
  std::uint32_t schema_version{kSchemaVersion};
};

class NEXORA_EDITOR_API ProjectWorkspace final {
public:
  bool Create(const std::filesystem::path &root, std::string name, std::string *error = nullptr);
  bool Open(const std::filesystem::path &root, std::string *error = nullptr);
  bool SaveWorkspace(std::span<const std::string> open_documents, std::string *error = nullptr);
  bool RecoverWorkspace(std::string *error = nullptr);
  [[nodiscard]] bool HasExternalChange() const;
  [[nodiscard]] const ProjectDescriptor &Project() const noexcept { return project_; }
  [[nodiscard]] const std::filesystem::path &Root() const noexcept { return root_; }
  [[nodiscard]] std::span<const std::string> OpenDocuments() const noexcept { return documents_; }

private:
  bool WriteWorkspace(std::span<const std::string> documents, std::string *error);
  std::filesystem::path root_;
  ProjectDescriptor project_;
  std::vector<std::string> documents_;
  std::filesystem::file_time_type workspace_write_time_{};
};

enum class ImportState { Pending, Imported, Cancelled, Failed };
struct AssetEntry final {
  runtime::AssetUuid id;
  std::string relative_path;
  std::string type;
  std::string artifact_hash;
  ImportState state{ImportState::Pending};
  std::string error;
};

class NEXORA_EDITOR_API AssetWorkspace final {
public:
  using Cancelled = std::function<bool()>;
  using Progress = std::function<void(std::size_t, std::size_t)>;
  bool ImportTree(const std::filesystem::path &content_root, Cancelled cancelled = {},
                  Progress progress = {});
  [[nodiscard]] std::vector<const AssetEntry *> Search(std::string_view query,
                                                       std::string_view type = {}) const;
  [[nodiscard]] const AssetEntry *Find(runtime::AssetUuid id) const;
  [[nodiscard]] std::span<const AssetEntry> Entries() const noexcept { return entries_; }

private:
  std::vector<AssetEntry> entries_;
};

class NEXORA_EDITOR_API SceneDocument final {
public:
  SceneDocument(runtime::World &world, runtime::Id scene);
  runtime::Id Create(std::string name, runtime::Id parent = 0);
  bool Select(std::span<const runtime::Id> entities);
  bool Reparent(runtime::Id entity, runtime::Id parent);
  bool SetTransform(runtime::Id entity, runtime::Transform transform);
  bool CopySelection();
  bool Paste();
  bool Undo();
  bool Save(const std::filesystem::path &path) const;
  bool Reload(const std::filesystem::path &path);
  [[nodiscard]] std::span<const runtime::Id> Selection() const noexcept { return selection_; }
  [[nodiscard]] std::optional<runtime::Id> Parent(runtime::Id entity) const;
  [[nodiscard]] std::string_view Name(runtime::Id entity) const;

private:
  struct Node final {
    runtime::Id id{}, parent{};
    std::string name;
  };
  runtime::World &world_;
  runtime::Id scene_{};
  runtime::SceneEditor editor_;
  std::vector<Node> nodes_;
  std::vector<runtime::Id> selection_;
  std::vector<Node> clipboard_;
};

} // namespace nexora::editor
