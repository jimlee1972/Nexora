#pragma once

#include "Nexora/Core/JobSystem.h"
#include "Nexora/Editor/MaterialAssetCatalog.h"
#include "Nexora/Editor/MeshAssetCatalog.h"
#include "Nexora/Editor/ProjectContent.h"
#include "Nexora/Editor/StaticProjectExport.h"

namespace nexora::editor {
enum class StaticExportPhase : std::uint8_t {
  Idle,
  Queued,
  Cooking,
  Verifying,
  Staging,
  ReadyToPublish,
  Published,
  Cancelled,
  Stale,
  Failed
};
struct StaticExportSnapshot final {
  std::uint64_t operation{};
  StaticExportPhase phase{StaticExportPhase::Idle};
  std::string message;
  std::string relative_path;
  std::string verify_command;
  std::string checksum;
  std::uint64_t bytes{};
  foundation::Uuid source_project;
  std::uint64_t document_generation{};
};

// One retained operation. Start/Poll/Cancel/Snapshot/Shutdown are serialized authoring-thread
// calls; jobs must outlive this owner. Worker retains only owning input/path/status values.
// Cooking cannot be interrupted internally; cancellation is checked between phases/read chunks.
// Poll rechecks current project, complete scene and imported content before single-file rename.
// No scene saved-baseline/history mutation, executable build, deployment or native-render claim.
class NEXORA_EDITOR_API StaticProjectExportJob final {
public:
  explicit StaticProjectExportJob(core::JobSystem &jobs);
  ~StaticProjectExportJob();
  StaticProjectExportJob(const StaticProjectExportJob &) = delete;
  StaticProjectExportJob &operator=(const StaticProjectExportJob &) = delete;
  bool Start(const ProjectWorkspace &, const SceneDocument &, const ProjectContentSession &,
             const MeshAssetCatalog &, const MaterialAssetCatalog &, runtime::AssetUuid scene_asset,
             std::string *error = nullptr);
  // Returns true once a pending operation reaches Published/Cancelled/Stale/Failed.
  bool Poll(const ProjectWorkspace &, const SceneDocument &, const ProjectContentSession &,
            const MeshAssetCatalog &, const MaterialAssetCatalog &);
  bool Cancel() noexcept;
  [[nodiscard]] bool Busy() const;
  [[nodiscard]] StaticExportSnapshot Snapshot() const;
  void Shutdown() noexcept;

private:
  struct Implementation;
  std::unique_ptr<Implementation> implementation_;
};
} // namespace nexora::editor
