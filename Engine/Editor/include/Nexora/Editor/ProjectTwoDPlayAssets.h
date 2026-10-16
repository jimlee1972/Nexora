#pragma once

#include "Nexora/Editor/ProjectContent.h"
#include "Nexora/Editor/TwoDPlayAssets.h"

namespace nexora::editor {
// Serialized authoring-thread capture. Host authorization includes current Play/modal policy.
// No borrowed workspace, Content, store, source file or publication authority survives capture.
class NEXORA_EDITOR_API ProjectTwoDPlayAssets final {
public:
  [[nodiscard]] static std::shared_ptr<const ProjectTwoDPlayAssets>
  Capture(const ProjectWorkspace &, const ProjectContentSession &,
          std::uint64_t expected_project_generation, std::uint64_t cooked_generation,
          std::span<const runtime::AssetUuid> roots, bool authorized, std::string *error = nullptr,
          TwoDPlayAssetLimits limits = {});
  // Recheck immediately before activation; an observation does not grant ongoing authority.
  [[nodiscard]] bool Matches(const ProjectWorkspace &, const ProjectContentSession &,
                             bool authorized, std::string *error = nullptr) const;
  [[nodiscard]] foundation::Uuid Project() const noexcept { return project_; }
  [[nodiscard]] const std::filesystem::path &Root() const noexcept { return root_; }
  [[nodiscard]] std::uint64_t ProjectGeneration() const noexcept { return project_generation_; }
  [[nodiscard]] std::uint64_t ContentRevision() const noexcept { return content_revision_; }
  [[nodiscard]] std::shared_ptr<const TwoDPlayAssets> Assets() const noexcept { return assets_; }

private:
  ProjectTwoDPlayAssets() = default;
  struct Source final {
    runtime::AssetUuid id;
    std::filesystem::path path;
    std::string type, artifact;
    runtime::ByteBuffer bytes, metadata;
    std::vector<runtime::AssetUuid> dependencies;
  };
  foundation::Uuid project_;
  std::filesystem::path root_;
  std::uint64_t project_generation_{}, content_revision_{};
  std::vector<Source> sources_;
  std::shared_ptr<const TwoDPlayAssets> assets_;
};
} // namespace nexora::editor
