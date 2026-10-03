#pragma once

#include "Nexora/Editor/Api.h"
#include "Nexora/Editor/InspectorRotation.h"
#include "Nexora/Foundation/Types.h"
#include "Nexora/Runtime/AssetPipeline.h"
#include "Nexora/Runtime/EditorSdk.h"

#include <compare>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
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
  static constexpr std::uint32_t kSchemaVersion = 2;
  foundation::Uuid id;
  std::string name;
  std::uint32_t schema_version{kSchemaVersion};
};

enum class ProjectAccess : std::uint8_t { ReadWrite, ReadOnly };
enum class ProjectUpgradeState : std::uint8_t { Current, Applied, Required };

class NEXORA_EDITOR_API ProjectWorkspace final {
public:
  ProjectWorkspace();
  ~ProjectWorkspace();
  ProjectWorkspace(ProjectWorkspace &&) noexcept;
  ProjectWorkspace &operator=(ProjectWorkspace &&) noexcept;
  ProjectWorkspace(const ProjectWorkspace &) = delete;
  ProjectWorkspace &operator=(const ProjectWorkspace &) = delete;

  bool Create(const std::filesystem::path &root, std::string name, std::string *error = nullptr);
  bool Open(const std::filesystem::path &root, std::string *error = nullptr);
  bool Open(const std::filesystem::path &root, ProjectAccess access, std::string *error = nullptr);
  bool SaveWorkspace(std::span<const std::string> open_documents, std::string *error = nullptr);
  bool RecoverWorkspace(std::string *error = nullptr);
  bool DiscardRecovery(std::string *error = nullptr);
  bool SaveEditorLayout(std::string_view layout, std::string *error = nullptr);
  [[nodiscard]] std::optional<std::string> LoadEditorLayout(std::string *error = nullptr) const;
  [[nodiscard]] bool HasRecoveryJournal() const;
  [[nodiscard]] bool HasExternalChange() const;
  [[nodiscard]] const ProjectDescriptor &Project() const noexcept { return project_; }
  [[nodiscard]] const std::filesystem::path &Root() const noexcept { return root_; }
  [[nodiscard]] std::span<const std::string> OpenDocuments() const noexcept { return documents_; }
  [[nodiscard]] ProjectAccess Access() const noexcept { return access_; }
  [[nodiscard]] bool Writable() const noexcept { return access_ == ProjectAccess::ReadWrite; }
  [[nodiscard]] ProjectUpgradeState UpgradeState() const noexcept { return upgrade_state_; }

private:
  struct LockState;
  bool WriteWorkspace(std::span<const std::string> documents, std::string *error);
  std::filesystem::path root_;
  ProjectDescriptor project_;
  std::vector<std::string> documents_;
  std::filesystem::file_time_type workspace_write_time_{};
  std::unique_ptr<LockState> lock_;
  ProjectAccess access_{ProjectAccess::ReadOnly};
  ProjectUpgradeState upgrade_state_{ProjectUpgradeState::Current};
};

struct RecentProject final {
  foundation::Uuid id;
  std::filesystem::path root;
  std::string name;
};

// User-level, versioned recent-project state. The application owns this store separately from a
// project so opening a project read-only never grants write access to project-owned files.
class NEXORA_EDITOR_API RecentProjectStore final {
public:
  static constexpr std::size_t kMaximumEntries = 12;

  [[nodiscard]] static std::filesystem::path DefaultPath();
  bool Open(const std::filesystem::path &path, std::string *error = nullptr);
  bool Record(const ProjectWorkspace &workspace, std::string *error = nullptr);
  bool Remove(foundation::Uuid id, std::string *error = nullptr);
  [[nodiscard]] std::span<const RecentProject> Entries() const noexcept { return entries_; }
  [[nodiscard]] const std::filesystem::path &Path() const noexcept { return path_; }

private:
  bool Save(std::string *error);
  std::filesystem::path path_;
  std::vector<RecentProject> entries_;
};

enum class ImportState { Pending, Imported, Cancelled, Failed };
enum class AssetIdentityMode { DerivedFromPath, PersistentReadOnly, PersistentReadWrite };

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
                  Progress progress = {},
                  AssetIdentityMode identity_mode = AssetIdentityMode::DerivedFromPath,
                  std::string *error = nullptr);
  [[nodiscard]] static std::filesystem::path
  IdentitySidecar(const std::filesystem::path &asset_path);
  [[nodiscard]] std::vector<const AssetEntry *> Search(std::string_view query,
                                                       std::string_view type = {}) const;
  [[nodiscard]] const AssetEntry *Find(runtime::AssetUuid id) const;
  [[nodiscard]] std::span<const AssetEntry> Entries() const noexcept { return entries_; }
  [[nodiscard]] bool PersistentIdentities() const noexcept {
    return identity_mode_ != AssetIdentityMode::DerivedFromPath;
  }
  [[nodiscard]] bool WritableIdentities() const noexcept {
    return identity_mode_ == AssetIdentityMode::PersistentReadWrite;
  }
  [[nodiscard]] const std::filesystem::path &ContentRoot() const noexcept { return content_root_; }

private:
  std::vector<AssetEntry> entries_;
  std::filesystem::path content_root_;
  AssetIdentityMode identity_mode_{AssetIdentityMode::DerivedFromPath};
};

// Editor view of one runtime scene: node names plus selection, clipboard, and persistence. The
// hierarchy itself lives in the runtime (Entity::parent); this class reads it from there.
class NEXORA_EDITOR_API SceneDocument final {
public:
  struct NodeKey final {
    runtime::Id id{};
    std::uint64_t entity_generation{};
    std::uint64_t document_generation{};
    auto operator<=>(const NodeKey &) const = default;
  };
  struct NodeView final {
    runtime::Id id{}, parent{};
    std::string_view name;
    std::uint64_t entity_generation{};
    std::uint64_t document_generation{};
    [[nodiscard]] NodeKey Key() const noexcept {
      return {id, entity_generation, document_generation};
    }
  };
  SceneDocument(runtime::World &world, runtime::Id scene);
  // Creates a node; with a parent the new entity starts at the parent's origin (identity local).
  runtime::Id Create(std::string name, runtime::Id parent = 0);
  bool Select(std::span<const runtime::Id> entities);
  bool Select(std::span<const NodeKey> entities);
  bool Rename(NodeKey entity, std::string name);
  // Undoable; keeps the entity's world pose like dragging in Unity's Hierarchy. Rejects cycles.
  bool Reparent(runtime::Id entity, runtime::Id parent);
  // A Hierarchy drag: reparent keeping the world pose and place the node at `index` among its new
  // siblings (clamped to the last position), as one undo step.
  bool Move(runtime::Id entity, runtime::Id parent, std::size_t index);
  bool Move(NodeKey entity, std::optional<NodeKey> parent, std::size_t index);
  bool SetTransform(runtime::Id entity, runtime::Transform transform);
  bool SetTransforms(std::span<const NodeKey> entities,
                     std::span<const runtime::Transform> transforms);
  // Degrees use extrinsic Z-X-Y composition. One field edit is one atomic undo transaction.
  bool SetEulerField(std::span<const NodeKey> entities, std::size_t axis, double degrees);
  // Preserves authored revolutions while the local quaternion matches; otherwise canonical angles.
  [[nodiscard]] std::optional<EulerDegrees> EulerAngles(runtime::Id entity) const noexcept;
  [[nodiscard]] std::optional<runtime::Transform> Transform(runtime::Id entity) const noexcept;
  bool CopySelection();
  bool Paste();
  // Deletes selected subtrees. Each selected root is one undo step; descendants are not deleted
  // twice.
  bool DeleteSelection();
  bool Undo();
  bool Save(const std::filesystem::path &path) const;
  bool Reload(const std::filesystem::path &path);
  [[nodiscard]] std::span<const runtime::Id> Selection() const noexcept { return selection_; }
  [[nodiscard]] std::optional<NodeKey> Key(runtime::Id entity) const noexcept;
  [[nodiscard]] std::uint64_t Generation() const noexcept { return document_generation_; }
  [[nodiscard]] std::optional<runtime::Id> Parent(runtime::Id entity) const;
  [[nodiscard]] std::string_view Name(runtime::Id entity) const;
  // In runtime sibling order (scene storage order), so a Hierarchy view can list children as
  // ordered by Move and the runtime.
  [[nodiscard]] std::vector<NodeView> Nodes() const;

private:
  struct EulerHint final {
    runtime::Transform transform;
    EulerDegrees degrees;
  };
  struct Node final {
    runtime::Id id{};
    std::string name;
    std::uint64_t generation{};
    std::optional<EulerHint> euler_hint{};
  };
  struct ClipboardNode final {
    std::string name;
    runtime::Transform world_transform;
  };
  struct UndoEntry final {
    enum class Kind { Runtime, Rename } kind{Kind::Runtime};
    NodeKey entity;
    std::string previous_name;
    std::vector<std::pair<NodeKey, std::optional<EulerHint>>> previous_hints{};
    std::vector<Node> deleted_nodes{};
    std::vector<runtime::Id> previous_selection{};
  };
  runtime::World &world_;
  runtime::Id scene_{};
  runtime::SceneEditor editor_;
  std::vector<Node> nodes_;
  std::vector<runtime::Id> selection_;
  std::vector<ClipboardNode> clipboard_;
  std::vector<UndoEntry> undo_;
  std::uint64_t document_generation_{};
  std::uint64_t next_entity_generation_{1};
};

} // namespace nexora::editor
