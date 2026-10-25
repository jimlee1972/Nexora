#pragma once

#include "Nexora/Editor/EditorWorkspace.h"

namespace nexora::editor {
class ContentBrowserModel;
struct BuildScene final {
  runtime::AssetUuid id;
  std::string path;
  bool enabled{true};
  bool operator==(const BuildScene &) const = default;
};
struct ProjectBuildSettings final {
  static constexpr std::size_t kMaximumBytes = 256 * 1024;
  static constexpr std::size_t kMaximumScenes = 256;
  static constexpr std::size_t kMaximumPathBytes = 1024;
  runtime::AssetUuid start_scene;
  std::vector<BuildScene> scenes;
  bool operator==(const ProjectBuildSettings &) const = default;
};

// Pure owning, bounded schema-1 codec shared by graphical and command-line build producers.
// No filesystem access; callers must resolve paths and catalog identities before building.
[[nodiscard]] NEXORA_EDITOR_API bool ValidateProjectBuildSettings(const ProjectBuildSettings &,
                                                                  std::string *error = nullptr);
[[nodiscard]] NEXORA_EDITOR_API std::optional<ProjectBuildSettings>
ParseProjectBuildSettings(std::string_view, std::string *error = nullptr);
[[nodiscard]] NEXORA_EDITOR_API std::optional<std::string>
SerializeProjectBuildSettings(const ProjectBuildSettings &, std::string *error = nullptr);

// Owner-thread session borrowing a workspace that must outlive it. Reload observes an exact disk
// baseline; Save rejects external replacement, read-only/recovery and unsafe paths. Missing legacy
// settings are an empty optional with no error; corrupt bytes never become an overwrite baseline.
class NEXORA_EDITOR_API ProjectBuildSettingsSession final {
public:
  explicit ProjectBuildSettingsSession(const ProjectWorkspace &workspace);
  [[nodiscard]] bool Reload(std::string *error = nullptr);
  [[nodiscard]] const std::optional<ProjectBuildSettings> &Settings() const noexcept {
    return settings_;
  }
  [[nodiscard]] bool Ready() const noexcept { return ready_; }
  [[nodiscard]] bool ValidateFiles(const ProjectBuildSettings &,
                                   std::string *error = nullptr) const;
  // Reconcile Content renames by full UUID; same-path/different-UUID conflicts reject. Scenes
  // outside Content keep the explicit identity stored in these settings, never a name fallback.
  [[nodiscard]] bool Reconcile(ProjectBuildSettings &, const ContentBrowserModel &,
                               std::string *error = nullptr) const;
  [[nodiscard]] std::optional<BuildScene> Enroll(std::string_view relative_path,
                                                 const ContentBrowserModel &,
                                                 std::string *error = nullptr) const;
  bool Save(const ProjectBuildSettings &, const ContentBrowserModel &,
            std::string *error = nullptr);

private:
  [[nodiscard]] bool Live(std::string *error) const;
  [[nodiscard]] bool Read(std::optional<std::string> &, std::string *error) const;
  const ProjectWorkspace &workspace_;
  std::filesystem::path root_;
  foundation::Uuid project_;
  std::optional<std::string> baseline_;
  std::optional<ProjectBuildSettings> settings_;
  bool ready_{};
};
} // namespace nexora::editor
